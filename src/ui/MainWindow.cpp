#include "ui/MainWindow.h"

#include "core/Database.h"
#include "core/Settings.h"
#include "core/SipUri.h"
#include "ui/AccountDialog.h"
#include "ui/CallPanel.h"
#include "ui/ContactsTab.h"
#include "ui/DialerTab.h"
#include "ui/HistoryTab.h"
#include "ui/Icons.h"
#include "ui/IncomingDialog.h"
#include "ui/SettingsDialog.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWindow>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QShortcut>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

MainWindow::MainWindow(SipEngine *engine, Database *db, QWidget *parent)
    : QWidget(parent)
    , m_engine(engine)
    , m_db(db)
{
    setWindowTitle(QStringLiteral("kk-sip"));
    setWindowIcon(Icons::app());
    setMinimumSize(260, 400);

    // Our own slim title strip instead of the window manager's frame (the user asked for
    // no thick borders); "System window frame" in Settings brings the native one back.
    m_frameless = !Settings::instance().systemFrame;
    if (m_frameless) {
        setWindowFlag(Qt::FramelessWindowHint);
        // Transparent corners so we can draw rounded ones like the desktop's windows.
        setAttribute(Qt::WA_TranslucentBackground);
        setMouseTracking(true);
    }

    auto *layout = new QVBoxLayout(this);
    // In frameless mode the margin doubles as an invisible resize handle.
    layout->setContentsMargins(m_frameless ? QMargins(5, 0, 5, 4) : QMargins(6, 6, 6, 4));
    layout->setSpacing(6);
    if (m_frameless) {
        m_titleBar = createTitleBar();
        layout->addWidget(m_titleBar);
    }

    // Account switcher + menu
    auto *top = new QHBoxLayout;
    m_accountBox = new QComboBox(this);
    m_accountBox->setToolTip(tr("Account for outgoing calls. All enabled accounts receive calls."));
    m_accountBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_accountBox->setMinimumContentsLength(10);
    top->addWidget(m_accountBox, 1);

    auto *menuButton = new QToolButton(this);
    menuButton->setIcon(Icons::get(QStringLiteral("application-menu")));
    menuButton->setText(QStringLiteral("☰"));
    menuButton->setPopupMode(QToolButton::InstantPopup);
    menuButton->setAutoRaise(true);
    auto *menu = new QMenu(menuButton);
    menu->addAction(Icons::get(QStringLiteral("configure")), tr("Settings…"), this, &MainWindow::openSettings);
    m_dndAction = menu->addAction(Icons::get(QStringLiteral("notifications-disabled")), tr("Do not disturb"));
    m_dndAction->setCheckable(true);
    m_dndAction->setChecked(Settings::instance().doNotDisturb);
    menu->addSeparator();
    menu->addAction(tr("About kk-sip"), this, [this] {
        QMessageBox::about(this, tr("About kk-sip"),
                           tr("<b>kk-sip %1</b><br>Minimal SIP softphone for Linux.<br>"
                              "Built on PJSIP and Qt. License: GPL-2.0-or-later.")
                               .arg(QStringLiteral(KKSIP_VERSION)));
    });
    menu->addAction(Icons::get(QStringLiteral("application-exit")), tr("Quit"), this, &MainWindow::quit);
    menuButton->setMenu(menu);
    top->addWidget(menuButton);
    layout->addLayout(top);

    m_callPanel = new CallPanel(m_engine, [this](const QString &n) { return nameFor(n); }, this);
    layout->addWidget(m_callPanel);

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    m_dialer = new DialerTab(m_db, this);
    m_history = new HistoryTab(m_db, this);
    m_contacts = new ContactsTab(m_db, m_engine, this);
    m_tabs->addTab(m_dialer, tr("Dial"));
    m_tabs->addTab(m_history, tr("History"));
    m_tabs->addTab(m_contacts, tr("Contacts"));
    layout->addWidget(m_tabs, 1);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("statusLabel"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont sf = m_status->font();
    sf.setPointSizeF(sf.pointSizeF() * 0.9);
    m_status->setFont(sf);
    layout->addWidget(m_status);

    connect(m_dialer, &DialerTab::callRequested, this, &MainWindow::dial);
    connect(m_dialer, &DialerTab::keyPressed, this, &MainWindow::onKeypad);
    connect(m_history, &HistoryTab::callRequested, this, &MainWindow::dial);
    connect(m_contacts, &ContactsTab::callRequested, this, &MainWindow::dial);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (m_tabs->widget(index) == m_history)
            setMissed(0);
    });

    connect(m_accountBox, &QComboBox::currentIndexChanged, this, [this] {
        const QString id = m_accountBox->currentData().toString();
        if (!id.isEmpty() && id != Settings::instance().currentAccountId) {
            Settings::instance().currentAccountId = id;
            Settings::instance().save();
        }
        updateStatus();
        updateBlfTargets(); // lamps that follow the selected account
    });
    connect(m_dndAction, &QAction::toggled, this, [this](bool on) {
        m_engine->setDoNotDisturb(on);
        Settings::instance().doNotDisturb = on;
        Settings::instance().save();
        updateStatus();
        updateTray();
    });

    connect(m_engine, &SipEngine::regStateChanged, this, [this] {
        updateAccountIcons();
        updateStatus();
        updateTray();
    });
    connect(m_engine, &SipEngine::incomingCall, this, &MainWindow::onIncoming);
    connect(Theme::Notifier::instance(), &Theme::Notifier::changed, this, [this] {
        updateAccountIcons();
        m_history->reload();
    });
    connect(m_engine, &SipEngine::callEnded, this, &MainWindow::onCallEnded);

    auto *settingsShortcut = new QShortcut(QKeySequence::Preferences, this);
    connect(settingsShortcut, &QShortcut::activated, this, &MainWindow::openSettings);
    auto *quitShortcut = new QShortcut(QKeySequence::Quit, this);
    connect(quitShortcut, &QShortcut::activated, this, &MainWindow::quit);

    reloadAccountBox();
    setupTray();
    updateStatus();
    connect(m_db, &Database::contactsChanged, this, &MainWindow::updateBlfTargets);
    updateBlfTargets();

    // On Wayland the compositor ignores the position part; KWin's rule handles it (main.cpp).
    if (!Settings::instance().rememberPosition || !restoreGeometry(Settings::instance().windowGeometry))
        resize(300, 500);

    if (Settings::instance().accounts.isEmpty())
        QTimer::singleShot(300, this, &MainWindow::openSettings);
}

// --- Frameless window -------------------------------------------------------

QWidget *MainWindow::createTitleBar()
{
    auto *bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("titleBar"));
    bar->setFixedHeight(30);
    auto *h = new QHBoxLayout(bar);
    h->setContentsMargins(2, 0, 0, 0);
    h->setSpacing(2);

    auto *icon = new QLabel(bar);
    icon->setPixmap(Icons::app().pixmap(16, 16));
    auto *title = new QLabel(QStringLiteral("kk-sip"), bar);
    title->setObjectName(QStringLiteral("muted"));
    h->addWidget(icon);
    h->addSpacing(4);
    h->addWidget(title);
    h->addStretch(1);

    auto makeButton = [bar](const QString &name, const QString &text, const QString &tip) {
        auto *b = new QToolButton(bar);
        b->setObjectName(name);
        b->setText(text);
        b->setToolTip(tip);
        b->setAutoRaise(true);
        b->setFocusPolicy(Qt::NoFocus);
        b->setFixedSize(30, 24);
        return b;
    };
    auto *minimize = makeButton(QStringLiteral("titleButton"), QStringLiteral("—"), tr("Minimize"));
    auto *close = makeButton(QStringLiteral("titleCloseButton"), QStringLiteral("✕"), tr("Close"));
    connect(minimize, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(close, &QToolButton::clicked, this, &QWidget::close);
    h->addWidget(minimize);
    h->addWidget(close);

    // Drag the window by the strip (and its labels).
    for (QWidget *w : {bar, static_cast<QWidget *>(icon), static_cast<QWidget *>(title)})
        w->installEventFilter(this);
    return bar;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (m_frameless && event->type() == QEvent::MouseButtonPress
        && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton && windowHandle()) {
        windowHandle()->startSystemMove();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

Qt::Edges MainWindow::edgesAt(const QPoint &pos) const
{
    constexpr int grip = 5;
    Qt::Edges e;
    if (pos.x() < grip)
        e |= Qt::LeftEdge;
    if (pos.x() >= width() - grip)
        e |= Qt::RightEdge;
    if (pos.y() < 3)
        e |= Qt::TopEdge;
    if (pos.y() >= height() - grip)
        e |= Qt::BottomEdge;
    return e;
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (m_frameless && event->button() == Qt::LeftButton && windowHandle()) {
        const Qt::Edges edges = edgesAt(event->position().toPoint());
        if (edges)
            windowHandle()->startSystemResize(edges);
        else
            windowHandle()->startSystemMove();
        return;
    }
    QWidget::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_frameless) {
        const Qt::Edges e = edgesAt(event->position().toPoint());
        Qt::CursorShape shape = Qt::ArrowCursor;
        if (e == (Qt::LeftEdge | Qt::TopEdge) || e == (Qt::RightEdge | Qt::BottomEdge))
            shape = Qt::SizeFDiagCursor;
        else if (e == (Qt::RightEdge | Qt::TopEdge) || e == (Qt::LeftEdge | Qt::BottomEdge))
            shape = Qt::SizeBDiagCursor;
        else if (e & (Qt::LeftEdge | Qt::RightEdge))
            shape = Qt::SizeHorCursor;
        else if (e & (Qt::TopEdge | Qt::BottomEdge))
            shape = Qt::SizeVerCursor;
        setCursor(shape);
    }
    QWidget::mouseMoveEvent(event);
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    if (!m_frameless)
        return;
    // Same radius as the window decoration's corners (about 10px in Breeze-like and
    // Aurorae themes); the only frame is a 1px line.
    constexpr qreal radius = 10;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Theme::colors().border, 1));
    p.setBrush(palette().color(QPalette::Window));
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
}

// --- Accounts / status ------------------------------------------------------

QString MainWindow::currentAccountId() const
{
    return m_accountBox->currentData().toString();
}

QString MainWindow::accountTitle(const QString &id) const
{
    for (const AccountConfig &a : Settings::instance().accounts)
        if (a.id == id)
            return a.title();
    return {};
}

void MainWindow::reloadAccountBox()
{
    const Settings &s = Settings::instance();
    QSignalBlocker block(m_accountBox);
    m_accountBox->clear();
    // Disabled accounts stay in the list (grey) so they can be switched on right from here.
    for (const AccountConfig &a : s.accounts) {
        const QString title = a.enabled ? a.title() : tr("%1 (off)").arg(a.title());
        m_accountBox->addItem(Icons::status(m_engine->regState(a.id)), title, a.id);
    }
    if (m_accountBox->count() == 0) {
        m_accountBox->addItem(Icons::status(RegState::Disabled), tr("No accounts"), QString());
        m_accountBox->setEnabled(false);
    } else {
        m_accountBox->setEnabled(true);
        const int idx = m_accountBox->findData(s.currentAccountId);
        m_accountBox->setCurrentIndex(qMax(0, idx));
    }
}

void MainWindow::updateAccountIcons()
{
    for (int i = 0; i < m_accountBox->count(); ++i) {
        const QString id = m_accountBox->itemData(i).toString();
        if (!id.isEmpty())
            m_accountBox->setItemIcon(i, Icons::status(m_engine->regState(id)));
    }
}

void MainWindow::updateStatus()
{
    if (!m_transientMessage.isEmpty()) {
        m_status->setText(m_transientMessage);
        return;
    }
    const QString id = currentAccountId();
    QString text = id.isEmpty() ? tr("Add an account in Settings") : m_engine->regText(id);
    const AccountConfig *cfg = accountConfig(id);
    if (cfg && !cfg->enabled)
        text = cfg->password.isEmpty() ? tr("No password: press Call to enter it") : tr("Account is off: press Call to turn it on");
    if (m_engine->doNotDisturb())
        text = tr("Do not disturb") + QStringLiteral(" · ") + text;
    m_status->setText(text);
}

void MainWindow::showMessage(const QString &text)
{
    m_transientMessage = text;
    updateStatus();
    QTimer::singleShot(6000, this, [this, text] {
        if (m_transientMessage == text) {
            m_transientMessage.clear();
            updateStatus();
        }
    });
}

QString MainWindow::nameFor(const QString &number, const QString &sipName) const
{
    const QString contact = m_db->nameForNumber(number);
    return contact.isEmpty() ? sipName : contact;
}

// --- Calls -------------------------------------------------------------------

void MainWindow::dial(const QString &input)
{
    const QString number = SipUri::cleanNumber(input);
    if (number.isEmpty())
        return;
    const QString accountId = currentAccountId();
    if (accountId.isEmpty()) {
        showMessage(tr("No account to call from"));
        return;
    }
    if (!ensureEnabled(accountId))
        return;
    QString error;
    if (m_engine->makeCall(accountId, number, &error) < 0) {
        showMessage(tr("Call failed: %1").arg(error));
        return;
    }
    m_dialer->setLastDialed(number);
    m_dialer->setNumber(QString());
    m_tabs->setCurrentWidget(m_dialer);
}

const AccountConfig *MainWindow::accountConfig(const QString &id) const
{
    for (const AccountConfig &a : Settings::instance().accounts)
        if (a.id == id)
            return &a;
    return nullptr;
}

bool MainWindow::ensureEnabled(const QString &id)
{
    const AccountConfig *cfg = accountConfig(id);
    if (!cfg || cfg->enabled)
        return cfg != nullptr;

    // Imported accounts come without a password: ask for it right here instead of sending
    // the user into Settings.
    AccountConfig edited = *cfg;
    edited.enabled = true;
    AccountDialog dlg(edited, this);
    if (dlg.exec() != QDialog::Accepted)
        return false;
    edited = dlg.account();
    if (!edited.enabled)
        return false;

    Settings &s = Settings::instance();
    for (AccountConfig &a : s.accounts)
        if (a.id == id)
            a = edited;
    s.save();
    m_engine->applyAccounts(s.accounts);
    reloadAccountBox();
    updateStatus();
    return true;
}

void MainWindow::updateBlfTargets()
{
    QList<BlfTarget> targets;
    const QString current = currentAccountId();
    for (const Contact &c : m_db->contacts()) {
        if (!c.blf)
            continue;
        const QString account = c.blfAccount.isEmpty() ? current : c.blfAccount;
        if (!account.isEmpty())
            targets.append({c.id, account, c.number});
    }
    m_engine->setBlfTargets(targets);
}

void MainWindow::onKeypad(const QString &key)
{
    // During a live call the keypad sends DTMF (IVR menus), otherwise it types the number.
    for (const CallView &c : m_engine->calls()) {
        if (c.state == CallView::Active && !c.onHold) {
            m_engine->sendDtmf(c.id, key);
            return;
        }
    }
    m_dialer->appendDigit(key);
}

void MainWindow::onIncoming(int callId)
{
    const CallView c = m_engine->call(callId);
    auto *dlg = new IncomingDialog(m_engine, callId, nameFor(c.number, c.name), c.number,
                                   accountTitle(c.accountId));
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void MainWindow::onCallEnded(const CallView &c)
{
    HistoryEntry e;
    e.accountId = c.accountId;
    e.incoming = c.incoming;
    e.number = c.number;
    e.name = nameFor(c.number, c.name);
    e.startedAt = c.startedAt;
    if (c.answered) {
        e.status = HistoryEntry::Answered;
        e.duration = int(c.connectedAt.secsTo(QDateTime::currentDateTime()));
    } else if (c.incoming) {
        e.status = c.declined ? HistoryEntry::Declined : HistoryEntry::Missed;
    } else {
        e.status = HistoryEntry::Failed;
    }
    m_db->addHistory(e);

    if (e.status == HistoryEntry::Missed) {
        if (!(isVisible() && m_tabs->currentWidget() == m_history))
            setMissed(m_missed + 1);
        if (m_tray)
            m_tray->showMessage(tr("Missed call"), e.name.isEmpty() ? e.number : e.name + QLatin1Char(' ') + e.number,
                                Icons::app(), 10000);
    } else if (!c.incoming && !c.answered && c.lastCode >= 300 && c.lastCode != 487) {
        // 487 = we cancelled ourselves; anything else is worth telling (busy, not found...).
        showMessage(tr("Call failed: %1 %2").arg(c.lastCode).arg(c.lastReason));
    }
}

void MainWindow::setMissed(int count)
{
    m_missed = count;
    m_tabs->setTabText(m_tabs->indexOf(m_history),
                       count > 0 ? tr("History (%1)").arg(count) : tr("History"));
    updateTray();
}

// --- Settings ----------------------------------------------------------------

void MainWindow::openSettings()
{
    SettingsDialog dlg(m_engine, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    dlg.commit();
    const Settings &s = Settings::instance();
    m_engine->applyAccounts(s.accounts);
    m_engine->applyAudioDevices(s.captureDevice, s.playbackDevice);
    m_engine->applyCodecs(s.codecs);
    m_engine->setRingtone(s.ringtoneFile);
    reloadAccountBox();
    m_history->reload();
    updateStatus();
    updateBlfTargets();
}

// --- Window / tray -----------------------------------------------------------

void MainWindow::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;
    m_tray = new QSystemTrayIcon(Icons::app(), this);
    auto *menu = new QMenu(this);
    menu->addAction(tr("Show"), this, &MainWindow::showAndRaise);
    menu->addAction(m_dndAction);
    menu->addSeparator();
    menu->addAction(Icons::get(QStringLiteral("application-exit")), tr("Quit"), this, &MainWindow::quit);
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason != QSystemTrayIcon::Trigger)
            return;
        if (isVisible() && isActiveWindow())
            hide();
        else
            showAndRaise();
    });
    connect(m_tray, &QSystemTrayIcon::messageClicked, this, [this] {
        showAndRaise();
        m_tabs->setCurrentWidget(m_history);
    });
    updateTray();
    m_tray->show();
}

void MainWindow::updateTray()
{
    if (!m_tray)
        return;
    QStringList lines{QStringLiteral("kk-sip")};
    int online = 0;
    const QList<AccountConfig> &accounts = Settings::instance().accounts;
    for (const AccountConfig &a : accounts)
        if (m_engine->regState(a.id) == RegState::Online)
            ++online;
    lines << tr("Online: %1 of %2").arg(online).arg(accounts.size());
    if (m_engine->doNotDisturb())
        lines << tr("Do not disturb");
    if (m_missed > 0)
        lines << tr("Missed calls: %1").arg(m_missed);
    m_tray->setToolTip(lines.join(QLatin1Char('\n')));
}

void MainWindow::showAndRaise()
{
    show();
    if (isMinimized())
        showNormal();
    raise();
    activateWindow();
}

void MainWindow::handleExternal(const QString &message)
{
    if (message.startsWith(QLatin1String("dial "))) {
        showAndRaise();
        dial(SipUri::fromLink(message.mid(5)));
    } else {
        showAndRaise();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    Settings::instance().windowGeometry = saveGeometry();
    Settings::instance().save();
    if (!m_quitting && m_tray && Settings::instance().closeToTray) {
        hide();
        event->ignore();
        return;
    }
    if (quit())
        event->accept();
    else
        event->ignore();
}

bool MainWindow::quit()
{
    if (!m_engine->calls().isEmpty()
        && QMessageBox::question(this, tr("Quit"), tr("There are active calls. Hang up and quit?"))
            != QMessageBox::Yes)
        return false;
    m_quitting = true;
    if (isVisible())
        Settings::instance().windowGeometry = saveGeometry();
    Settings::instance().save();
    if (m_tray)
        m_tray->hide();
    qApp->quit();
    return true;
}
