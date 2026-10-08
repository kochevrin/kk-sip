#pragma once

#include "sip/SipEngine.h"

#include <QWidget>

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

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void reloadAccountBox();
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
    QString accountTitle(const QString &id) const;
    QString nameFor(const QString &number, const QString &sipName = {}) const;

    SipEngine *m_engine;
    Database *m_db;
    QComboBox *m_accountBox;
    QAction *m_dndAction;
    CallPanel *m_callPanel;
    QTabWidget *m_tabs;
    DialerTab *m_dialer;
    HistoryTab *m_history;
    ContactsTab *m_contacts;
    QLabel *m_status;
    QSystemTrayIcon *m_tray = nullptr;
    QString m_transientMessage;
    int m_missed = 0;
    bool m_quitting = false;
};
