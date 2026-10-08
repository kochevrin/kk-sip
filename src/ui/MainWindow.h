#pragma once

#include "sip/SipEngine.h"

#include <QWidget>

class AccountSwitcher;
class QAbstractButton;
class CallPanel;
class ContactsTab;
class Database;
class DialerTab;
class HistoryTab;
class QAction;
class QComboBox;
class QLabel;
class QSystemTrayIcon;
class QTabWidget;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow(SipEngine *engine, Database *db, QWidget *parent = nullptr);

    void dial(const QString &number);
    // Message from a second launch: "show" or "dial <number or link>".
    void handleExternal(const QString &message);
    void showAndRaise();
    // Compact (dial pad, history and contacts as tabs) or wide (history and contacts
    // beside the dial pad). Both are fixed sizes: the window is never resized by hand.
    void setWideView(bool wide);
    bool isWideView() const { return m_wide; }

    static constexpr QSize CompactSize{300, 500};
    static constexpr QSize WideSize{680, 500};

protected:
    void closeEvent(QCloseEvent *event) override;
    // Frameless mode: 1px border, drag by the title strip or any empty spot.
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *createTitleBar();
    QTabWidget *tabsOf(QWidget *page) const;
    bool isShowing(QWidget *page) const;
    void showPage(QWidget *page);
    QString historyTitle() const;
    void reloadAccountBox();
    void updateBlfTargets();
    void updateAccountIcons();
    void updateStatus();
    void showMessage(const QString &text);
    void onKeypad(const QString &key);
    void onIncoming(int callId);
    void onCallEnded(const CallView &call);
    void openSettings();
    void setupTray();
    void updateTray();
    void setMissed(int count);
    bool quit();
    QString currentAccountId() const;
    const AccountConfig *accountConfig(const QString &id) const;
    bool ensureEnabled(const QString &id);
    void setAccountEnabled(const QString &id, bool on);
    QString accountTitle(const QString &id) const;
    QString nameFor(const QString &number, const QString &sipName = {}) const;

    SipEngine *m_engine;
    Database *m_db;
    AccountSwitcher *m_accountBox;
    QAction *m_dndAction;
    CallPanel *m_callPanel;
    QTabWidget *m_tabs;      // Dial (+ History, Contacts when compact)
    QTabWidget *m_sideTabs;  // History, Contacts in the wide view
    QWidget *m_leftColumn;
    QWidget *m_divider;
    QAction *m_wideAction;
    QAbstractButton *m_wideButton = nullptr;
    DialerTab *m_dialer;
    HistoryTab *m_history;
    ContactsTab *m_contacts;
    QLabel *m_status;
    QSystemTrayIcon *m_tray = nullptr;
    QString m_transientMessage;
    int m_missed = 0;
    bool m_quitting = false;
    bool m_frameless = false;
    bool m_wide = false;
    QWidget *m_titleBar = nullptr;
};
