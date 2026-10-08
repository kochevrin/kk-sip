// End-to-end smoke test: real SIP stack against a real PBX (see tests/README.md).
//
//   KKSIP_TEST_SERVER   PBX address, default 127.0.0.1:5070 (extensions 101-103, password "secret",
//                       600 = echo, 601 = busy)
//   KKSIP_PJSUA         pjsua binary used as the remote party
//   KKSIP_SCREENSHOTS   optional directory for PNG screenshots of the UI

#include "core/Autostart.h"
#include "core/Database.h"
#include "core/MicrosipImport.h"
#include "core/Settings.h"
#include "core/SipUri.h"
#include "sip/SipEngine.h"
#include "ui/AccountDialog.h"
#include "ui/CallPanel.h"
#include "ui/DialerTab.h"
#include "ui/IncomingDialog.h"
#include "ui/MainWindow.h"
#include "ui/SettingsDialog.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QTimer>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTranslator>

class SmokeTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void sipUri();
    void microsipImport();
    void audioDevices();
    void registration();
    void outgoingEcho();
    void outgoingBusy();
    void incomingAnswer();
    void incomingMissed();
    void manyAccounts();
    void busyLamp();
    void dialSuggestion();
    void autostart();
    void screenshots();

private:
    QProcess *startRemote(const QString &target, bool registerAs103 = false);
    void shot(QWidget *w, const QString &name);
    bool waitNoCalls(int ms = 10000);
    HistoryEntry lastHistory() const;

    QString m_server = qEnvironmentVariable("KKSIP_TEST_SERVER", QStringLiteral("127.0.0.1:5070"));
    QString m_shots = qEnvironmentVariable("KKSIP_SCREENSHOTS");
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
    m_window->resize(300, 500);
    m_window->show();
}

void SmokeTest::cleanupTestCase()
{
    m_window.reset();
    if (m_engine)
        m_engine->shutdown();
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
    QVERIFY2(QTest::qWaitFor([&] { return m_engine->regState(m_acc101.id) == RegState::Online; }, 10000),
             qPrintable(m_engine->regText(m_acc101.id)));
    QVERIFY2(QTest::qWaitFor([&] { return m_engine->regState(m_acc102.id) == RegState::Online; }, 10000),
             qPrintable(m_engine->regText(m_acc102.id)));
    QTest::qWait(200);
    shot(m_window.get(), QStringLiteral("01-main-online"));
}

void SmokeTest::outgoingEcho()
{
    m_window->dial(QStringLiteral("600"));
    QVERIFY(QTest::qWaitFor([&] {
        const QList<CallView> calls = m_engine->calls();
        return !calls.isEmpty() && calls.first().state == CallView::Active;
    }, 10000));
    QTest::qWait(1500);
    shot(m_window.get(), QStringLiteral("02-call-active"));

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
    auto *tabs = m_window->findChild<QTabWidget *>();
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

void SmokeTest::busyLamp()
{
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
    auto *tabs = m_window->findChild<QTabWidget *>();
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
    // Test mode redirects ~/.config to ~/.qttest/config, the real autostart is untouched.
    Autostart::setEnabled(false, true);
    QVERIFY(!Autostart::isEnabled());
    Autostart::setEnabled(true, true);
    QVERIFY(Autostart::isEnabled());
    QFile entry(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                + QStringLiteral("/autostart/kk-sip.desktop"));
    QVERIFY(entry.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(entry.readAll());
    entry.close();
    QVERIFY(text.contains(QStringLiteral("Exec=") + Autostart::command() + QStringLiteral(" --minimized")));
    Autostart::setEnabled(true, false);
    entry.open(QIODevice::ReadOnly);
    QVERIFY(!QString::fromUtf8(entry.readAll()).contains(QLatin1String("--minimized")));
    entry.close();
    Autostart::setEnabled(false, true);
    QVERIFY(!Autostart::isEnabled());
}

void SmokeTest::screenshots()
{
    SettingsDialog settings(m_engine.get(), m_window.get());
    settings.show();
    QTest::qWait(100);
    shot(&settings, QStringLiteral("08-settings"));
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

void SmokeTest::shot(QWidget *w, const QString &name)
{
    if (m_shots.isEmpty() || !w)
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
