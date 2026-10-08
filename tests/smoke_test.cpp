// End-to-end smoke test: real SIP stack against a real PBX (see tests/README.md).
//
//   KKSIP_TEST_SERVER   PBX address, default 127.0.0.1:5070 (extensions 101-103, password "secret",
//                       600 = echo, 601 = busy); "offline" skips the tests that need it
//                       (Windows CI has no Asterisk)
//   KKSIP_PJSUA         pjsua binary used as the remote party
//   KKSIP_SCREENSHOTS   optional directory for PNG screenshots of the UI

#include "core/Autostart.h"
#include "core/WindowPlacement.h"
#include "core/Database.h"
#include "core/MicrosipImport.h"
#include "core/Settings.h"
#include "core/SipUri.h"
#include "core/UpdateChecker.h"
#include "sip/SipEngine.h"
#include "ui/AccountDialog.h"
#include "ui/AccountSwitcher.h"
#include "ui/CallPanel.h"
#include "ui/DialerTab.h"
#include "ui/HistoryTab.h"
#include "ui/ListDelegate.h"
#include "ui/IncomingDialog.h"
#include "ui/MainWindow.h"
#include "ui/SettingsDialog.h"
#include "ui/Theme.h"

#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QTabBar>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QMenu>
#include <QProcess>
#include <QPushButton>
#include <QTimer>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTranslator>
#include <QTreeWidget>

class SmokeTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void sipUri();
    void settingsRoundTrip();
    void microsipImport();
    void audioDevices();
    void registration();
    void outgoingEcho();
    void outgoingBusy();
    void incomingAnswer();
    void incomingMissed();
    void historyViews();
    void manyAccounts();
    void busyLamp();
    void dialSuggestion();
    void autostart();
    void kwinRule();
    void accountSwitcher();
    void wideView();
    void screenshots();
    void updateVersions();
    void closeKeepsRunning();
    void layoutAudit();

private:
    QProcess *startRemote(const QString &target, bool registerAs103 = false);
    void shot(QWidget *w, const QString &name);
    bool waitNoCalls(int ms = 10000);
    HistoryEntry lastHistory() const;

    QString m_server = qEnvironmentVariable("KKSIP_TEST_SERVER", QStringLiteral("127.0.0.1:5070"));
    QString m_shots = qEnvironmentVariable("KKSIP_SCREENSHOTS");
    QStringList m_layoutProblems; // collected by shot() from every screen it sees
    bool m_offline = m_server == QLatin1String("offline");
    std::unique_ptr<Database> m_db;
    std::unique_ptr<SipEngine> m_engine;
    std::unique_ptr<MainWindow> m_window;
    AccountConfig m_acc101;
    AccountConfig m_acc102;
};

void SmokeTest::initTestCase()
{
    // KKSIP_LANG=ru renders the screenshots with the Russian translation.
    static QTranslator translator;
    if (translator.load(QLocale(qEnvironmentVariable("KKSIP_LANG", QStringLiteral("en"))), QStringLiteral("kk-sip"),
                        QStringLiteral("_"), QStringLiteral(":/i18n")))
        QApplication::installTranslator(&translator);

    Theme::apply(Theme::modeFromString(qEnvironmentVariable("KKSIP_THEME", QStringLiteral("dark"))));
    QStandardPaths::setTestModeEnabled(true);
    QDir(Settings::instance().configDir()).removeRecursively();
    QDir(Settings::instance().dataDir()).removeRecursively();

    Settings &s = Settings::instance();
    m_acc101 = {};
    m_acc101.id = QStringLiteral("acc101");
    m_acc101.label = QStringLiteral("Office");
    m_acc101.server = m_server;
    m_acc101.user = QStringLiteral("101");
    m_acc101.password = QStringLiteral("secret");
    m_acc102 = m_acc101;
    m_acc102.id = QStringLiteral("acc102");
    m_acc102.label = QStringLiteral("Support (TCP)");
    m_acc102.user = QStringLiteral("102");
    m_acc102.transport = QStringLiteral("tcp");
    s.accounts = {m_acc101, m_acc102};
    s.currentAccountId = m_acc101.id;
    // Keep the test silent.
    s.captureDevice = QStringLiteral("null");
    s.playbackDevice = QStringLiteral("null");
    s.debugLog = qEnvironmentVariableIsSet("KKSIP_DEBUG");
    s.save();

    m_db = std::make_unique<Database>();
    QString error;
    QVERIFY2(m_db->open(&error), qPrintable(error));
    m_db->saveContact({0, QStringLiteral("Echo test"), QStringLiteral("600")});
    m_db->saveContact({0, QStringLiteral("Anna Petrova"), QStringLiteral("103")});

    m_engine = std::make_unique<SipEngine>();
    QVERIFY2(m_engine->start(&error), qPrintable(error));
    m_engine->applyAccounts(s.accounts);

    m_window = std::make_unique<MainWindow>(m_engine.get(), m_db.get());
    m_window->show();
}

void SmokeTest::cleanupTestCase()
{
    m_window.reset();
    if (m_engine)
        m_engine->shutdown();
}

void SmokeTest::settingsRoundTrip()
{
    Settings &s = Settings::instance();
    const Settings saved = s;
    s.currentAccountId = m_acc102.id;
    s.theme = QStringLiteral("light");
    s.language = QStringLiteral("uk");
    s.debugLog = true;
    s.doNotDisturb = true;
    s.codecs = {{QStringLiteral("PCMA/8000/1"), true}, {QStringLiteral("opus/48000/2"), false}};
    s.save();

    s.currentAccountId.clear();
    s.theme.clear();
    s.language.clear();
    s.debugLog = false;
    s.doNotDisturb = false;
    s.codecs.clear();
    s.load();
    QCOMPARE(s.currentAccountId, m_acc102.id);
    QCOMPARE(s.theme, QStringLiteral("light"));
    QCOMPARE(s.language, QStringLiteral("uk"));
    QVERIFY(s.debugLog);
    QVERIFY(s.doNotDisturb);
    QCOMPARE(s.codecs.size(), 2);
    QCOMPARE(s.codecs.at(1).id, QStringLiteral("opus/48000/2"));
    QVERIFY(!s.codecs.at(1).enabled);

    // Files written by 0.1.0 keep their values ([%General] group).
    const QString path = s.configDir() + QStringLiteral("/kk-sip.ini");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("[%General]\ncurrentAccount=legacy\ntheme=dark\n");
    f.close();
    s.load();
    QCOMPARE(s.currentAccountId, QStringLiteral("legacy"));
    QCOMPARE(s.theme, QStringLiteral("dark"));

    s = saved;
    s.save();
}

void SmokeTest::sipUri()
{
    AccountConfig a;
    a.server = QStringLiteral("pbx.local:5060");
    QCOMPARE(SipUri::toTarget(QStringLiteral("101"), a), QStringLiteral("sip:101@pbx.local:5060"));
    QCOMPARE(SipUri::toTarget(QStringLiteral("+38 (050) 123-45-67"), a),
             QStringLiteral("sip:+380501234567@pbx.local:5060"));
    QCOMPARE(SipUri::toTarget(QStringLiteral("bob@other.org"), a), QStringLiteral("sip:bob@other.org"));
    a.transport = QStringLiteral("tcp");
    a.domain = QStringLiteral("corp.example");
    QCOMPARE(SipUri::toTarget(QStringLiteral("tel:777"), a), QStringLiteral("sip:777@corp.example;transport=tcp"));

    const SipUri::Parsed p = SipUri::parse(QStringLiteral("\"Anna P\" <sip:103@10.0.0.1:5070;transport=tcp>"));
    QCOMPARE(p.displayName, QStringLiteral("Anna P"));
    QCOMPARE(p.user, QStringLiteral("103"));
    QCOMPARE(p.host, QStringLiteral("10.0.0.1:5070"));
    QCOMPARE(SipUri::fromLink(QStringLiteral("tel:+1%20555%20010")), QStringLiteral("+1555010"));
}

void SmokeTest::microsipImport()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("microsip.ini"));
    const QString ini = QStringLiteral(
        "[Settings]\r\naccountId=1\r\n"
        "[Account1]\r\nlabel=Офис\r\nserver=pbx.example.com\r\nproxy=\r\ndomain=pbx.example.com\r\n"
        "username=201\r\npassword=0A1B2C\r\nauthID=\r\ndisplayName=Иван\r\ntransport=tcp\r\nSRTP=\r\n"
        "[Account2]\r\nlabel=\r\nserver=10.0.0.5:5080\r\ndomain=corp.example\r\nusername=7001\r\n"
        "authID=u7001\r\ntransport=udp\r\nSRTP=optional\r\n");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("\xFF\xFE");
    f.write(reinterpret_cast<const char *>(ini.utf16()), ini.size() * 2);
    f.close();

    QString error;
    const QList<AccountConfig> list = MicrosipImport::readAccounts(path, &error);
    QCOMPARE(list.size(), 2);
    QCOMPARE(list[0].label, QStringLiteral("Офис"));
    QCOMPARE(list[0].server, QStringLiteral("pbx.example.com"));
    QVERIFY(list[0].domain.isEmpty());
    QCOMPARE(list[0].transport, QStringLiteral("tcp"));
    QCOMPARE(list[0].displayName, QStringLiteral("Иван"));
    QVERIFY(list[0].password.isEmpty());
    QVERIFY(!list[0].enabled); // waits for a password
    QCOMPARE(list[1].domain, QStringLiteral("corp.example"));
    QCOMPARE(list[1].authUser, QStringLiteral("u7001"));
    QVERIFY(list[1].srtp);
}

void SmokeTest::audioDevices()
{
    const QList<AudioDevice> devs = m_engine->audioDevices();
    QStringList keys;
    for (const AudioDevice &d : devs)
        keys << d.key;
    qInfo().noquote() << "audio devices:" << keys.join(QStringLiteral(", "));
    QVERIFY(!keys.contains(QStringLiteral("ALSA|surround51:CARD=PCH,DEV=0")));
}

void SmokeTest::registration()
{
    if (m_offline)
        QSKIP("needs the test PBX");
    QVERIFY2(QTest::qWaitFor([&] { return m_engine->regState(m_acc101.id) == RegState::Online; }, 10000),
             qPrintable(m_engine->regText(m_acc101.id)));
    QVERIFY2(QTest::qWaitFor([&] { return m_engine->regState(m_acc102.id) == RegState::Online; }, 10000),
             qPrintable(m_engine->regText(m_acc102.id)));
    QTest::qWait(200);
    shot(m_window.get(), QStringLiteral("01-main-online"));
}

void SmokeTest::outgoingEcho()
{
    if (m_offline)
        QSKIP("needs the test PBX");
    m_window->dial(QStringLiteral("600"));
    QVERIFY(QTest::qWaitFor([&] {
        const QList<CallView> calls = m_engine->calls();
        return !calls.isEmpty() && calls.first().state == CallView::Active;
    }, 10000));
    QTest::qWait(1500);
    shot(m_window.get(), QStringLiteral("02-call-active"));
    m_window->setWideView(true);
    QTest::qWait(100);
    shot(m_window.get(), QStringLiteral("02b-call-wide"));
    m_window->setWideView(false);

    const CallView call = m_engine->calls().first();
    m_engine->setMute(call.id, true);
    QVERIFY(m_engine->call(call.id).muted);
    m_engine->setHold(call.id, true);
    QVERIFY(QTest::qWaitFor([&] { return m_engine->call(call.id).onHold; }, 5000));
    m_engine->setHold(call.id, false);
    QVERIFY(QTest::qWaitFor([&] { return !m_engine->call(call.id).onHold; }, 5000));
    m_engine->sendDtmf(call.id, QStringLiteral("1"));

    m_engine->hangup(call.id);
    QVERIFY(waitNoCalls());
    const HistoryEntry h = lastHistory();
    QCOMPARE(h.number, QStringLiteral("600"));
    QCOMPARE(h.status, HistoryEntry::Answered);
    QVERIFY(!h.incoming);
    QVERIFY(h.duration >= 1);
    QCOMPARE(h.accountId, m_acc101.id);
}

void SmokeTest::outgoingBusy()
{
    if (m_offline)
        QSKIP("needs the test PBX");
    QSignalSpy ended(m_engine.get(), &SipEngine::callEnded);
    m_window->dial(QStringLiteral("601"));
    QVERIFY(ended.wait(10000));
    const CallView c = ended.takeFirst().at(0).value<CallView>();
    QCOMPARE(c.lastCode, 486);
    QCOMPARE(lastHistory().status, HistoryEntry::Failed);
    QTest::qWait(100);
    shot(m_window.get(), QStringLiteral("03-busy"));
}

void SmokeTest::incomingAnswer()
{
    if (m_offline)
        QSKIP("needs the test PBX");
    QSignalSpy incoming(m_engine.get(), &SipEngine::incomingCall);
    QProcess *remote = startRemote(QStringLiteral("sip:102@") + m_server + QStringLiteral(";transport=tcp"));
    QVERIFY(incoming.wait(10000));
    const int id = incoming.takeFirst().at(0).toInt();
    const CallView c = m_engine->call(id);
    QCOMPARE(c.accountId, m_acc102.id);
    QCOMPARE(c.number, QStringLiteral("103"));

    IncomingDialog *dlg = nullptr;
    QVERIFY(QTest::qWaitFor([&] {
        for (QWidget *w : QApplication::topLevelWidgets())
            if (auto *d = qobject_cast<IncomingDialog *>(w); d && d->isVisible())
                dlg = d;
        return dlg != nullptr;
    }, 3000));
    shot(dlg, QStringLiteral("04-incoming-dialog"));
    shot(m_window.get(), QStringLiteral("05-incoming-main"));

    // Answer from the call panel the way people do it: a quick double click. The second
    // click must not land on a button that appears in place of "Answer" (it used to open
    // the transfer dialog).
    auto *panel = m_window->findChild<CallPanel *>();
    QVERIFY(panel);
    QPushButton *answerButton = nullptr;
    for (QPushButton *b : panel->findChildren<QPushButton *>())
        if (b->isVisible() && b->text() == CallPanel::tr("Answer"))
            answerButton = b;
    QVERIFY(answerButton);
    bool modalOpened = false;
    QTimer modalWatch;
    connect(&modalWatch, &QTimer::timeout, this, [&] {
        if (QWidget *m = QApplication::activeModalWidget()) {
            modalOpened = true;
            m->close();
        }
    });
    modalWatch.start(50);
    const QPoint where = answerButton->mapTo(panel, answerButton->rect().center());
    QTest::mouseClick(answerButton, Qt::LeftButton);
    QTest::qWait(30);
    if (auto *under = qobject_cast<QPushButton *>(panel->childAt(where)))
        QTest::mouseClick(under, Qt::LeftButton);
    QVERIFY(QTest::qWaitFor([&] { return m_engine->call(id).state == CallView::Active; }, 5000));

    // Transfer only opens on a deliberate click, inline, and not right after the buttons appear.
    QPushButton *transferButton = nullptr;
    for (QPushButton *b : panel->findChildren<QPushButton *>())
        if (b->isVisible() && b->toolTip() == CallPanel::tr("Transfer call"))
            transferButton = b;
    QVERIFY(transferButton);
    QLineEdit *transferEdit = nullptr;
    for (QLineEdit *e : panel->findChildren<QLineEdit *>())
        if (e->placeholderText() == CallPanel::tr("Transfer to number…"))
            transferEdit = e;
    QVERIFY(transferEdit);
    QTest::mouseClick(transferButton, Qt::LeftButton);
    QVERIFY(!transferEdit->isVisible());
    QTest::qWait(800);
    QTest::mouseClick(transferButton, Qt::LeftButton);
    QVERIFY(transferEdit->isVisible());
    shot(m_window.get(), QStringLiteral("05b-transfer-inline"));
    QTest::keyClick(transferEdit, Qt::Key_Escape);
    QVERIFY(!transferEdit->isVisible());

    QTest::qWait(400);
    QVERIFY2(!modalOpened, "a dialog popped up while answering");
    QVERIFY(!m_engine->call(id).muted && !m_engine->call(id).onHold);
    m_engine->hangup(id);
    QVERIFY(waitNoCalls());
    remote->kill();
    remote->waitForFinished(3000);

    const HistoryEntry h = lastHistory();
    QVERIFY(h.incoming);
    QCOMPARE(h.status, HistoryEntry::Answered);
    QCOMPARE(h.name, QStringLiteral("Anna Petrova")); // resolved from contacts
}

void SmokeTest::incomingMissed()
{
    if (m_offline)
        QSKIP("needs the test PBX");
    QSignalSpy incoming(m_engine.get(), &SipEngine::incomingCall);
    QProcess *remote = startRemote(QStringLiteral("sip:101@") + m_server);
    QVERIFY(incoming.wait(10000));
    QTest::qWait(1500);
    remote->write("h\n"); // caller gives up
    QVERIFY(waitNoCalls());
    remote->kill();
    remote->waitForFinished(3000);

    const HistoryEntry h = lastHistory();
    QVERIFY(h.incoming);
    QCOMPARE(h.status, HistoryEntry::Missed);
    auto *tabs = m_window->findChild<QTabWidget *>(QStringLiteral("mainTabs"));
    QVERIFY(tabs);
    tabs->setCurrentIndex(0);
    QVERIFY(tabs->tabText(1).contains(QLatin1String("1")));
    tabs->setCurrentIndex(1);
    QTest::qWait(100);
    shot(m_window.get(), QStringLiteral("06-history"));
    tabs->setCurrentIndex(2);
    QTest::qWait(100);
    shot(m_window.get(), QStringLiteral("07-contacts"));
    tabs->setCurrentIndex(0);
}

void SmokeTest::historyViews()
{
    // Three calls with one mobile number, older than anything the other tests make.
    const QDateTime base = QDateTime::currentDateTime().addDays(-2);
    for (int i = 0; i < 3; ++i) {
        HistoryEntry e;
        e.accountId = m_acc101.id;
        e.incoming = i != 1;
        e.status = i == 2 ? HistoryEntry::Missed : HistoryEntry::Answered;
        e.number = QStringLiteral("+380671234567");
        e.startedAt = base.addSecs(-3600 * i);
        e.duration = 42 * (i + 1);
        m_db->addHistory(e);
    }

    auto *history = m_window->findChild<HistoryTab *>();
    QVERIFY(history);
    auto *search = history->findChild<QLineEdit *>(QStringLiteral("historySearch"));
    auto *tree = history->findChild<QTreeWidget *>();
    auto *listButton = history->findChild<QToolButton *>(QStringLiteral("historyList"));
    auto *groupedButton = history->findChild<QToolButton *>(QStringLiteral("historyGrouped"));
    QVERIFY(search && tree && listButton && groupedButton);
    history->setGrouped(true);

    // Grouped: every number once.
    QSet<QString> seen;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        const QString title = tree->topLevelItem(i)->text(0);
        QVERIFY2(!seen.contains(title), qPrintable(title));
        seen.insert(title);
    }

    // Typed with spaces, found without; the only match unfolds.
    search->setText(QStringLiteral("067 123"));
    QCOMPARE(tree->topLevelItemCount(), 1);
    QTreeWidgetItem *group = tree->topLevelItem(0);
    QCOMPARE(group->childCount(), 2);
    QVERIFY(group->isExpanded());
    QVERIFY(group->data(0, ListDelegate::BadgeRole).toString().startsWith(QLatin1String("3")));
    auto *tabs = m_window->findChild<QTabWidget *>(QStringLiteral("mainTabs"));
    tabs->setCurrentWidget(history);
    QTest::qWait(100);
    shot(m_window.get(), QStringLiteral("06b-history-grouped"));

    // Click folds and unfolds.
    const QRect r = tree->visualItemRect(group);
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, {}, r.center());
    QVERIFY(!group->isExpanded());

    // Flat list: one row per call, mode remembered.
    listButton->click();
    QVERIFY(!Settings::instance().historyGrouped);
    QCOMPARE(tree->topLevelItemCount(), 3);
    QCOMPARE(tree->topLevelItem(0)->childCount(), 0);

    // Name search goes through the phone book.
    search->setText(QStringLiteral("anna"));
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
        QCOMPARE(tree->topLevelItem(i)->text(0), QStringLiteral("Anna Petrova"));

    search->clear();
    groupedButton->click();
    QVERIFY(Settings::instance().historyGrouped);
    tabs->setCurrentIndex(0);

    // Quick light/dark switch in the main menu.
    auto *dark = m_window->findChild<QAction *>(QStringLiteral("darkTheme"));
    QVERIFY(dark);
    const bool wasDark = Theme::isDark();
    dark->trigger();
    QCOMPARE(Theme::isDark(), !wasDark);
    QCOMPARE(Settings::instance().theme, wasDark ? QStringLiteral("light") : QStringLiteral("dark"));
    shot(m_window.get(), QStringLiteral("06c-theme-switched"));
    dark->trigger();
    QCOMPARE(Theme::isDark(), wasDark);
}

void SmokeTest::busyLamp()
{
    if (m_offline)
        QSKIP("needs the test PBX");
    // Watch 103 (Anna) through account 101.
    Contact anna;
    for (const Contact &c : m_db->contacts())
        if (c.number == QLatin1String("103"))
            anna = c;
    QVERIFY(anna.id);
    anna.blf = true;
    anna.blfAccount = m_acc101.id;
    m_db->saveContact(anna); // MainWindow turns this into a subscription
    const auto lamp = [&] { return m_engine->blf(anna.id).state; };
    QVERIFY2(QTest::qWaitFor([&] { return lamp() == BlfState::Idle; }, 10000), "no BLF NOTIFY for 103");

    // 103 calls the echo service: busy, then free again.
    QProcess *remote = startRemote(QStringLiteral("sip:600@") + m_server);
    QVERIFY(QTest::qWaitFor([&] { return lamp() == BlfState::Busy; }, 10000));
    auto *tabs = m_window->findChild<QTabWidget *>(QStringLiteral("mainTabs"));
    tabs->setCurrentIndex(2);
    QTest::qWait(150);
    shot(m_window.get(), QStringLiteral("07b-contacts-blf"));
    tabs->setCurrentIndex(0);
    remote->write("h\n"); // 103 hangs up (sends BYE)
    QVERIFY(QTest::qWaitFor([&] { return lamp() == BlfState::Idle; }, 10000));
    remote->kill();
    remote->waitForFinished(3000);

    // Someone (we, from 101) calls 103: its lamp blinks "ringing" until we give up.
    remote = startRemote(QString(), /*registerAs103=*/true);
    QTest::qWait(1500); // let 103 register
    QSignalSpy ended(m_engine.get(), &SipEngine::callEnded);
    m_window->dial(QStringLiteral("103"));
    const bool rang = QTest::qWaitFor([&] { return lamp() == BlfState::Ringing; }, 10000);
    if (!rang && !ended.isEmpty()) {
        const CallView c = ended.first().at(0).value<CallView>();
        qWarning() << "call to 103 ended:" << c.lastCode << c.lastReason;
    }
    QVERIFY(rang);
    QVERIFY(m_engine->blf(anna.id).peer.contains(QLatin1String("101")));
    tabs->setCurrentIndex(2);
    QTest::qWait(150);
    shot(m_window.get(), QStringLiteral("07c-contacts-blf-ringing"));
    tabs->setCurrentIndex(0);
    m_engine->hangupAll();
    QVERIFY(waitNoCalls());
    QVERIFY(QTest::qWaitFor([&] { return lamp() == BlfState::Idle; }, 10000));
    remote->kill();
    remote->waitForFinished(3000);
}

void SmokeTest::manyAccounts()
{
    if (m_offline)
        QSKIP("needs the test PBX");
    // More accounts than PJSUA's default limit of 8; unknown users get rejected by the PBX.
    QList<AccountConfig> list{m_acc101, m_acc102};
    for (int i = 0; i < 20; ++i) {
        AccountConfig a = m_acc101;
        a.id = QStringLiteral("bulk%1").arg(i);
        a.user = QString::number(900 + i);
        list << a;
    }
    m_engine->applyAccounts(list);
    QVERIFY(QTest::qWaitFor([&] {
        for (const AccountConfig &a : std::as_const(list))
            if (m_engine->regState(a.id) == RegState::Registering)
                return false;
        return true;
    }, 15000));
    QCOMPARE(m_engine->regState(QStringLiteral("bulk19")), RegState::Failed);
    QCOMPARE(m_engine->regState(m_acc101.id), RegState::Online);

    for (AccountConfig &a : list)
        if (a.id.startsWith(QLatin1String("bulk")))
            a.enabled = false;
    m_engine->applyAccounts(list);
    QCOMPARE(m_engine->regState(QStringLiteral("bulk0")), RegState::Disabled);
    QCOMPARE(m_engine->regState(m_acc102.id), RegState::Online);
    m_engine->applyAccounts({m_acc101, m_acc102});
}

void SmokeTest::dialSuggestion()
{
    auto *dialer = m_window->findChild<DialerTab *>();
    QVERIFY(dialer);
    auto *field = dialer->findChild<QLineEdit *>();
    QPushButton *hint = nullptr;
    for (QPushButton *b : dialer->findChildren<QPushButton *>())
        if (b->isFlat())
            hint = b;
    QVERIFY(field && hint);

    field->setText(QStringLiteral("60"));
    QVERIFY(hint->isVisible());
    QVERIFY(hint->text().contains(QLatin1String("Echo test")));
    // The hint must sit above the keypad, not over it.
    QPushButton *key1 = nullptr;
    for (QPushButton *b : dialer->findChildren<QPushButton *>())
        if (b->text() == QLatin1String("1"))
            key1 = b;
    QVERIFY(key1);
    QVERIFY(hint->geometry().bottom() < key1->geometry().top());
    shot(m_window.get(), QStringLiteral("00-dial-suggestion"));
    QTest::mouseClick(hint, Qt::LeftButton);
    QCOMPARE(field->text(), QStringLiteral("600"));
    QVERIFY(!hint->isVisible()); // exact number, nothing more to suggest

    field->setText(QStringLiteral("ann"));
    QVERIFY(hint->text().contains(QLatin1String("Anna Petrova")));
    field->setText(QStringLiteral("05"));
    QVERIFY(!hint->isVisible()); // no contact starts with 05; 405-style substrings don't match
    field->clear();
}

void SmokeTest::autostart()
{
    // Test mode redirects ~/.config to ~/.qttest/config (Windows: a separate Run value),
    // the real autostart is untouched.
    Autostart::setEnabled(false, true);
    QVERIFY(!Autostart::isEnabled());
    Autostart::setEnabled(true, true);
    QVERIFY(Autostart::isEnabled());
    const QString line = Autostart::registeredCommandLine();
    QVERIFY2(line.contains(Autostart::command()), qPrintable(line));
    QVERIFY(line.endsWith(QLatin1String(" --minimized")));
    Autostart::setEnabled(true, false);
    QVERIFY(!Autostart::registeredCommandLine().contains(QLatin1String("--minimized")));
    Autostart::setEnabled(false, true);
    QVERIFY(!Autostart::isEnabled());
    QVERIFY(Autostart::registeredCommandLine().isEmpty());
}

void SmokeTest::kwinRule()
{
    // Someone else's rules must survive byte for byte (same layout as a real kwinrulesrc).
    const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/kwinrulesrc");
    const QByteArray original =
        "[0222d8f1-8989-4494-8bd3-4b81a8c15642]\nDescription=Aspia Console -> Pervynka\nactivityrule=2\n"
        "wmclass=aspia_console\nwmclassmatch=2\n\n[General]\ncount=2\n"
        "rules=0222d8f1-8989-4494-8bd3-4b81a8c15642,orca-minimized\n\n"
        "[orca-minimized]\nDescription=Orca -> start minimized\nminimize=true\nminimizerule=3\n";
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(original);
    f.close();

    WindowPlacement::setKWinRule(true, QSize(300, 500));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray with = f.readAll();
    f.close();
    QVERIFY(with.contains("rules=0222d8f1-8989-4494-8bd3-4b81a8c15642,orca-minimized,kk-sip-position\n"));
    QVERIFY(with.contains("count=3\n"));
    QVERIFY(with.contains("[kk-sip-position]\n"));
    QVERIFY(with.contains("positionrule=4\n"));
    QVERIFY(with.contains("[orca-minimized]\nDescription=Orca -> start minimized\nminimize=true\nminimizerule=3\n"));

    WindowPlacement::setKWinRule(true, QSize(300, 500)); // idempotent
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), with);
    f.close();

    WindowPlacement::setKWinRule(false, QSize());
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), original);
    f.close();
    QFile::remove(path);
}

void SmokeTest::accountSwitcher()
{
    auto *sw = m_window->findChild<AccountSwitcher *>();
    QVERIFY(sw);
    QCOMPARE(sw->currentId(), m_acc101.id);

    // The switch in the menu row turns registration off and on.
    emit sw->enableRequested(m_acc102.id, false);
    QVERIFY(QTest::qWaitFor([&] { return m_engine->regState(m_acc102.id) == RegState::Disabled; }, 5000));
    emit sw->enableRequested(m_acc102.id, true);
    if (!m_offline)
        QVERIFY(QTest::qWaitFor([&] { return m_engine->regState(m_acc102.id) == RegState::Online; }, 10000));

    sw->menu()->popup(sw->mapToGlobal(QPoint(0, sw->height())));
    QTest::qWait(150);
    shot(sw->menu(), QStringLiteral("10-account-menu"));
    sw->menu()->close();
}

void SmokeTest::wideView()
{
    auto *wide = m_window->findChild<QAction *>(QStringLiteral("wideView"));
    auto *tabs = m_window->findChild<QTabWidget *>(QStringLiteral("mainTabs"));
    auto *dialer = m_window->findChild<DialerTab *>();
    auto *history = m_window->findChild<HistoryTab *>();
    QVERIFY(wide && tabs && dialer && history);
    tabs->setCurrentWidget(dialer);
    QCOMPARE(m_window->size(), MainWindow::CompactSize);
    QCOMPARE(m_window->minimumSize(), m_window->maximumSize()); // fixed: no resizing by hand
    QVERIFY(!history->isVisible());

    wide->trigger();
    QVERIFY(m_window->isWideView());
    QVERIFY(Settings::instance().wideView);
    QCOMPARE(m_window->size(), MainWindow::WideSize);
    QCOMPARE(m_window->minimumSize(), m_window->maximumSize());
    QTest::qWait(100);
    QVERIFY(dialer->isVisible() && history->isVisible()); // side by side
    QCOMPARE(tabs->count(), 1);
    shot(m_window.get(), QStringLiteral("11-wide-history"));
    auto *side = m_window->findChild<QTabWidget *>(QStringLiteral("sideTabs"));
    QVERIFY(side);
    side->setCurrentIndex(1);
    QTest::qWait(100);
    shot(m_window.get(), QStringLiteral("11b-wide-contacts"));
    side->setCurrentIndex(0);

    wide->trigger();
    QVERIFY(!m_window->isWideView());
    QCOMPARE(m_window->size(), MainWindow::CompactSize);
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(tabs->currentWidget(), dialer);
    QVERIFY(!Settings::instance().wideView);
}

void SmokeTest::screenshots()
{
    SettingsDialog settings(m_engine.get(), m_window.get());
    settings.show();
    QTest::qWait(100);
    shot(&settings, QStringLiteral("08-settings"));
    if (auto *tabs = settings.findChild<QTabWidget *>()) {
        const char *names[] = {"08-settings", "08a-settings-audio", "08b-settings-codecs", "08c-settings-general"};
        for (int i = 1; i < tabs->count(); ++i) {
            tabs->setCurrentIndex(i);
            QTest::qWait(100);
            shot(&settings, QString::fromLatin1(names[i]));
        }
    }
    AccountDialog account(m_acc102, m_window.get());
    account.show();
    QTest::qWait(100);
    shot(&account, QStringLiteral("09-account"));
}

// ---------------------------------------------------------------------------

QProcess *SmokeTest::startRemote(const QString &target, bool registerAs103)
{
    const QString pjsua = qEnvironmentVariable("KKSIP_PJSUA");
    if (pjsua.isEmpty())
        qFatal("Set KKSIP_PJSUA to the pjsua binary");
    static int port = 5090;
    auto *p = new QProcess(this);
    p->setProgram(pjsua);
    p->setArguments({QStringLiteral("--null-audio"), QStringLiteral("--local-port=%1").arg(++port),
                     QStringLiteral("--id=\"Anna P\" <sip:103@%1>").arg(m_server),
                     QStringLiteral("--realm=*"), QStringLiteral("--username=103"),
                     QStringLiteral("--password=secret"), QStringLiteral("--log-level=0"),
                     QStringLiteral("--app-log-level=0"), QStringLiteral("--duration=20")});
    if (registerAs103) // a registered desk phone: rings (180) when called
        p->setArguments(p->arguments() << QStringLiteral("--registrar=sip:") + m_server
                                       << QStringLiteral("--auto-answer=180"));
    if (!target.isEmpty())
        p->setArguments(p->arguments() << target);
    p->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    connect(p, &QProcess::readyReadStandardOutput, p, [p] { p->readAllStandardOutput(); });
    p->start();
    if (!p->waitForStarted(3000))
        qFatal("cannot start pjsua");
    return p;
}

// Every visible control fits inside its parent, does not overlap a visible sibling and
// is not squeezed below its minimum size (which is where cut-off text comes from).
static QStringList layoutProblems(QWidget *root)
{
    QStringList out;
    auto describe = [](QWidget *w) {
        QString text = w->objectName();
        if (auto *b = qobject_cast<QAbstractButton *>(w))
            text = b->text().isEmpty() ? b->toolTip() : b->text();
        else if (auto *l = qobject_cast<QLabel *>(w))
            text = l->text();
        return QStringLiteral("%1 \"%2\"").arg(QLatin1String(w->metaObject()->className()), text.left(32));
    };
    auto internal = [root](QWidget *w) {
        // Parts of composite widgets (clear buttons, spin box editors, scroll bars...).
        for (QWidget *p = w->parentWidget(); p && p != root; p = p->parentWidget())
            if (qobject_cast<QLineEdit *>(p) || qobject_cast<QAbstractSpinBox *>(p) || qobject_cast<QComboBox *>(p)
                || qobject_cast<QTabBar *>(p) || qobject_cast<QAbstractScrollArea *>(p))
                return true;
        return false;
    };
    QList<QWidget *> checked;
    for (QWidget *w : root->findChildren<QWidget *>()) {
        if (w->isWindow() || !w->isVisibleTo(root) || internal(w))
            continue;
        checked << w;
        QWidget *parent = w->parentWidget();
        if (!parent->rect().contains(w->geometry()))
            out << QStringLiteral("%1 sticks out of its parent").arg(describe(w));
        const bool control = qobject_cast<QAbstractButton *>(w) || qobject_cast<QLineEdit *>(w)
                             || qobject_cast<QComboBox *>(w) || qobject_cast<QAbstractSpinBox *>(w);
        auto *label = qobject_cast<QLabel *>(w);
        if (control || (label && !label->wordWrap())) {
            const QSize min = w->minimumSizeHint();
            const bool ignoredWidth = w->sizePolicy().horizontalPolicy() == QSizePolicy::Ignored;
            if ((!ignoredWidth && w->width() < min.width()) || w->height() < min.height())
                out << QStringLiteral("%1 is %2x%3, needs %4x%5")
                           .arg(describe(w)).arg(w->width()).arg(w->height()).arg(min.width()).arg(min.height());
        }
    }
    for (int i = 0; i < checked.size(); ++i)
        for (int j = i + 1; j < checked.size(); ++j)
            if (checked[i]->parentWidget() == checked[j]->parentWidget()
                && checked[i]->geometry().intersects(checked[j]->geometry()))
                out << QStringLiteral("%1 overlaps %2").arg(describe(checked[i]), describe(checked[j]));
    return out;
}

void SmokeTest::updateVersions()
{
    QVERIFY(UpdateChecker::isNewer(QStringLiteral("v0.3.0"), QStringLiteral("0.2.1")));
    QVERIFY(UpdateChecker::isNewer(QStringLiteral("0.2.10"), QStringLiteral("0.2.9")));
    QVERIFY(UpdateChecker::isNewer(QStringLiteral("v1.0"), QStringLiteral("0.9.9")));
    QVERIFY(!UpdateChecker::isNewer(QStringLiteral("v0.2.1"), QStringLiteral("0.2.1")));
    QVERIFY(!UpdateChecker::isNewer(QStringLiteral("v0.2.0"), QStringLiteral("0.2.1")));
    QVERIFY(!UpdateChecker::isNewer(QStringLiteral("nightly"), QStringLiteral("0.2.1")));

    // A newer release shows a link in the status line.
    auto *checker = m_window->findChild<UpdateChecker *>();
    auto *link = m_window->findChild<QPushButton *>(QStringLiteral("updateLink"));
    QVERIFY(checker && link);
    QVERIFY(!link->isVisible());
    emit checker->updateAvailable(QStringLiteral("9.9.9"), UpdateChecker::releasesPage());
    QVERIFY(link->isVisible());
    QVERIFY(link->text().contains(QLatin1String("9.9.9")));
    QTest::qWait(100);
    shot(m_window.get(), QStringLiteral("12-update-available"));
    link->hide();
}

void SmokeTest::closeKeepsRunning()
{
    Settings::instance().closeToTray = true;
    QVERIFY(m_window->isVisible());
    m_window->close(); // the X in the title strip
    QVERIFY(!m_window->isVisible());
    QVERIFY(m_engine->regState(m_acc101.id) != RegState::Disabled); // still registered, takes calls
    m_window->showAndRaise(); // tray click or a second launch
    QVERIFY(m_window->isVisible());
}

void SmokeTest::layoutAudit()
{
    for (const QString &p : std::as_const(m_layoutProblems))
        qWarning().noquote() << p;
    QVERIFY2(m_layoutProblems.isEmpty(), "layout problems, see the warnings above");
}

void SmokeTest::shot(QWidget *w, const QString &name)
{
    if (!w)
        return;
    for (const QString &p : layoutProblems(w))
        m_layoutProblems << name + QStringLiteral(": ") + p;
    if (m_shots.isEmpty())
        return;
    QDir().mkpath(m_shots);
    w->grab().save(m_shots + QLatin1Char('/') + name + QStringLiteral(".png"));
}

bool SmokeTest::waitNoCalls(int ms)
{
    return QTest::qWaitFor([&] { return m_engine->calls().isEmpty(); }, ms);
}

HistoryEntry SmokeTest::lastHistory() const
{
    const QList<HistoryEntry> h = m_db->history(1);
    return h.isEmpty() ? HistoryEntry{} : h.first();
}

QTEST_MAIN(SmokeTest)
#include "smoke_test.moc"
