#include "core/Autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace Autostart {

#ifdef Q_OS_WIN

static const char RunKey[] = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";

// Tests must not touch the developer's real entry.
static QString valueName()
{
    return QStandardPaths::isTestModeEnabled() ? QStringLiteral("kk-sip-test") : QStringLiteral("kk-sip");
}

bool isEnabled()
{
    return QSettings(QLatin1String(RunKey), QSettings::NativeFormat).contains(valueName());
}

QString command()
{
    return QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
}

QString registeredCommandLine()
{
    return QSettings(QLatin1String(RunKey), QSettings::NativeFormat).value(valueName()).toString();
}

void setEnabled(bool on, bool minimized)
{
    QSettings run(QLatin1String(RunKey), QSettings::NativeFormat);
    if (!on) {
        run.remove(valueName());
        return;
    }
    QString line = QLatin1Char('"') + command() + QLatin1Char('"');
    if (minimized)
        line += QStringLiteral(" --minimized");
    run.setValue(valueName(), line);
}

#elif defined(Q_OS_MACOS)

// A per-user LaunchAgent that runs kk-sip at login.
static QString label()
{
    return QStandardPaths::isTestModeEnabled() ? QStringLiteral("io.github.kochevrin.kk-sip-test")
                                               : QStringLiteral("io.github.kochevrin.kk-sip");
}

static QString entryPath()
{
    return QDir::homePath() + QStringLiteral("/Library/LaunchAgents/") + label() + QStringLiteral(".plist");
}

bool isEnabled()
{
    return QFile::exists(entryPath());
}

QString command()
{
    // kk-sip.app/Contents/MacOS/kk-sip: register the bundle, so `open` starts it the
    // way Finder does (Dock icon, microphone permission tied to the app).
    QDir dir(QCoreApplication::applicationDirPath());
    if (dir.cdUp() && dir.cdUp() && dir.dirName().endsWith(QLatin1String(".app")))
        return dir.absolutePath();
    return QCoreApplication::applicationFilePath();
}

static QStringList arguments(bool minimized)
{
    const QString app = command();
    if (!app.endsWith(QLatin1String(".app")))
        return minimized ? QStringList{app, QStringLiteral("--minimized")} : QStringList{app};
    // -g: start in the background, without taking focus from what the user is doing.
    QStringList args{QStringLiteral("/usr/bin/open"), QStringLiteral("-g"), QStringLiteral("-a"), app};
    if (minimized)
        args << QStringLiteral("--args") << QStringLiteral("--minimized");
    return args;
}

QString registeredCommandLine()
{
    QFile f(entryPath());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    // The <string>s of the <array> that follows <key>ProgramArguments</key>.
    QXmlStreamReader xml(&f);
    QStringList args;
    bool inArguments = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isEndElement() && inArguments && xml.name() == QLatin1String("array"))
            break;
        if (!xml.isStartElement())
            continue;
        if (xml.name() == QLatin1String("key"))
            inArguments = xml.readElementText() == QLatin1String("ProgramArguments");
        else if (inArguments && xml.name() == QLatin1String("string"))
            args << xml.readElementText();
    }
    return args.join(QLatin1Char(' '));
}

void setEnabled(bool on, bool minimized)
{
    const QString path = entryPath();
    if (!on) {
        QFile::remove(path);
        return;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return;
    QXmlStreamWriter xml(&f);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeDTD(QStringLiteral("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
                                "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"));
    xml.writeStartElement(QStringLiteral("plist"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("1.0"));
    xml.writeStartElement(QStringLiteral("dict"));
    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("Label"));
    xml.writeTextElement(QStringLiteral("string"), label());
    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("ProgramArguments"));
    xml.writeStartElement(QStringLiteral("array"));
    const QStringList args = arguments(minimized);
    for (const QString &a : args)
        xml.writeTextElement(QStringLiteral("string"), a);
    xml.writeEndElement();
    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("RunAtLoad"));
    xml.writeEmptyElement(QStringLiteral("true"));
    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("LimitLoadToSessionType"));
    xml.writeTextElement(QStringLiteral("string"), QStringLiteral("Aqua"));
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();
    f.commit();
}

#else

static QString entryPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/autostart/kk-sip.desktop");
}

bool isEnabled()
{
    return QFile::exists(entryPath());
}

QString registeredCommandLine()
{
    QFile f(entryPath());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
    for (const QString &l : lines)
        if (l.startsWith(QLatin1String("Exec=")))
            return l.mid(5);
    return {};
}

QString command()
{
    // Inside an AppImage the binary lives in a temporary mount; the image itself is stable.
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    if (!appImage.isEmpty())
        return appImage;
    const QString self = QCoreApplication::applicationFilePath();
    // Installed in PATH: keep the entry short so it survives package updates.
    if (QStandardPaths::findExecutable(QStringLiteral("kk-sip")) == self)
        return QStringLiteral("kk-sip");
    return self;
}

static QString quoted(const QString &path)
{
    if (!path.contains(QLatin1Char(' ')))
        return path;
    QString escaped = path;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\")).replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

void setEnabled(bool on, bool minimized)
{
    const QString path = entryPath();
    if (!on) {
        QFile::remove(path);
        return;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return;
    QString exec = quoted(command());
    if (minimized)
        exec += QStringLiteral(" --minimized");
    f.write(QStringLiteral("[Desktop Entry]\n"
                           "Type=Application\n"
                           "Name=kk-sip\n"
                           "Comment=SIP softphone\n"
                           "Icon=kk-sip\n"
                           "Exec=%1\n"
                           "Terminal=false\n"
                           "X-GNOME-Autostart-enabled=true\n"
                           "X-KDE-autostart-after=panel\n")
                .arg(exec)
                .toUtf8());
    f.commit();
}

#endif

void refresh(bool minimized)
{
    if (isEnabled())
        setEnabled(true, minimized);
}

} // namespace Autostart
