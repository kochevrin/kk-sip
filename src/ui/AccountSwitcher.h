#pragma once

#include <QPushButton>

class QMenu;
class SipEngine;

// The account button at the top of the window. Its menu lists every account with a
// status lamp: click a row to call from that account, flip the switch on the right
// to register / unregister it.
class AccountSwitcher : public QPushButton {
    Q_OBJECT
public:
    AccountSwitcher(SipEngine *engine, QWidget *parent = nullptr);

    QString currentId() const { return m_current; }
    void setCurrentId(const QString &id);
    void refresh(); // accounts or states changed

signals:
    void currentChanged(const QString &id);
    void enableRequested(const QString &id, bool on);
    void settingsRequested();

private:
    void rebuildMenu();

    SipEngine *m_engine;
    QMenu *m_menu;
    QString m_current;
};
