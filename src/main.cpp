#include "core/Autostart.h"
#include "core/WindowPlacement.h"
#include "core/Database.h"
#include "core/MicrosipImport.h"
#include "core/Settings.h"
#include "core/SingleInstance.h"
#include "sip/SipEngine.h"
#include "ui/Icons.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <memory>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QTextStream>
#include <QTranslator>

static QString findFile(const QDir &dir, const QString &name)
{
    // MicroSIP writes MicroSIP.ini or microsip.ini depending on version; match case-insensitively.
    const QStringList hits = dir.entryList({name}, QDir::Files);
    for (const QString &h : hits)
        if (h.compare(name, Qt::CaseInsensitive) == 0)
            return dir.filePath(h);
    return {};
}

static int importMicrosip(const QString &folder)
{
    QTextStream out(stdout);
    const QDir dir(folder);
    if (SingleInstance().forwardToRunning(QStringLiteral("show"))) {
        out << QCoreApplication::translate("main", "Quit kk-sip first, then run the import again.") << Qt::endl;
        return 1;
    }

    Settings &settings = Settings::instance();
    settings.load();
    Theme::apply(Theme::modeFromString(settings.theme));
    Database db;
    QString error;
    if (!db.open(&error)) {
        out << error << Qt::endl;
        return 1;
    }

    const QString ini = findFile(dir, QStringLiteral("microsip.ini"));
    if (!ini.isEmpty()) {
        const QList<AccountConfig> found = MicrosipImport::readAccounts(ini, &error);
        const int added = MicrosipImport::mergeAccounts(settings.accounts, found);
        if (settings.currentAccountId.isEmpty() && !settings.accounts.isEmpty())
            settings.currentAccountId = settings.accounts.first().id;
        settings.save();
        out << QCoreApplication::translate("main", "Accounts: %1 found, %2 added (disabled until you enter passwords).")
                   .arg(found.size()).arg(added)
            << Qt::endl;
    } else {
        out << QCoreApplication::translate("main", "microsip.ini not found in %1").arg(dir.absolutePath()) << Qt::endl;
    }

    const QString xml = findFile(dir, QStringLiteral("contacts.xml"));
    QFile file(xml);
    if (!xml.isEmpty() && file.open(QIODevice::ReadOnly)) {
        const QList<Contact> found = MicrosipImport::readContacts(&file);
        const int added = MicrosipImport::mergeContacts(&db, found);
        out << QCoreApplication::translate("main", "Contacts: %1 found, %2 added.").arg(found.size()).arg(added)
            << Qt::endl;
    } else {
        out << QCoreApplication::translate("main", "Contacts.xml not found in %1").arg(dir.absolutePath()) << Qt::endl;
    }
    return 0;
}

// --help, --version and --import-microsip must work without a display (ssh, scripts).
static bool needsGui(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        const QByteArray a(argv[i]);
        if (a == "-h" || a == "--help" || a == "--help-all" || a == "-v" || a == "--version"
            || a.startsWith("--import-microsip"))
            return false;
    }
    return true;
}

int main(int argc, char *argv[])
{
    const bool gui = needsGui(argc, argv);
    std::unique_ptr<QCoreApplication> appHolder(gui ? new QApplication(argc, argv)
                                                    : new QCoreApplication(argc, argv));
    QCoreApplication &app = *appHolder;
    QCoreApplication::setApplicationName(QStringLiteral("kk-sip"));
    QCoreApplication::setApplicationVersion(QStringLiteral(KKSIP_VERSION));
    if (gui) {
        QGuiApplication::setDesktopFileName(QStringLiteral("kk-sip"));
        QApplication::setWindowIcon(Icons::app());
        QApplication::setQuitOnLastWindowClosed(false);
    }

    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale(), QStringLiteral("qtbase"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QCoreApplication::installTranslator(&qtTranslator);
    QTranslator appTranslator;
    if (appTranslator.load(QLocale(), QStringLiteral("kk-sip"), QStringLiteral("_"), QStringLiteral(":/i18n")))
        QCoreApplication::installTranslator(&appTranslator);

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "Minimal SIP softphone"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("number"),
                                 QCoreApplication::translate("main", "Number or sip:/tel: link to call"),
                                 QStringLiteral("[number]"));
    const QCommandLineOption importOption(
        QStringLiteral("import-microsip"),
        QCoreApplication::translate("main", "Import accounts (microsip.ini) and contacts (Contacts.xml) from a MicroSIP "
                                        "folder, then exit. kk-sip must not be running."),
        QStringLiteral("folder"));
    parser.addOption(importOption);
    const QCommandLineOption minimizedOption(
        QStringLiteral("minimized"), QCoreApplication::translate("main", "Start in the tray without showing the window"));
    parser.addOption(minimizedOption);
    parser.process(app);
    const QString target = parser.positionalArguments().value(0);

    if (parser.isSet(importOption))
        return importMicrosip(parser.value(importOption));

    SingleInstance instance;
    if (instance.forwardToRunning(target.isEmpty() ? QStringLiteral("show") : QStringLiteral("dial ") + target))
        return 0;
    instance.listen();

    Settings &settings = Settings::instance();
    settings.load();
    Theme::apply(Theme::modeFromString(settings.theme));

    Database db;
    QString error;
    if (!db.open(&error)) {
        QMessageBox::critical(nullptr, QStringLiteral("kk-sip"),
                              QCoreApplication::translate("main", "Cannot open database: %1").arg(error));
        return 1;
    }

    SipEngine engine;
    if (!engine.start(&error)) {
        QMessageBox::critical(nullptr, QStringLiteral("kk-sip"),
                              QCoreApplication::translate("main", "Cannot start SIP stack: %1").arg(error));
        return 1;
    }
    engine.applyAccounts(settings.accounts);

    MainWindow window(&engine, &db);
    QObject::connect(&instance, &SingleInstance::messageReceived, &window, &MainWindow::handleExternal);
    Autostart::refresh(settings.startHidden);
    if (WindowPlacement::usesKWinRule())
        WindowPlacement::setKWinRule(settings.rememberPosition, window.size());
    // Autostart passes --minimized; a manual launch or a first run without accounts shows the window.
    if (!parser.isSet(minimizedOption) || !target.isEmpty() || settings.accounts.isEmpty())
        window.show();
    if (!target.isEmpty())
        window.handleExternal(QStringLiteral("dial ") + target);

    const int rc = app.exec();
    engine.shutdown();
    return rc;
}
