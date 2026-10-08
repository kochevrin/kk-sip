#pragma once

#include "core/Settings.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

class AccountDialog : public QDialog {
    Q_OBJECT
public:
    explicit AccountDialog(const AccountConfig &account, QWidget *parent = nullptr);

    AccountConfig account() const;

private:
    void validate();

    AccountConfig m_account;
    QLineEdit *m_label;
    QLineEdit *m_server;
    QLineEdit *m_domain;
    QLineEdit *m_proxy;
    QLineEdit *m_user;
    QLineEdit *m_authUser;
    QLineEdit *m_password;
    QLineEdit *m_displayName;
    QComboBox *m_transport;
    QCheckBox *m_srtp;
    QCheckBox *m_enabled;
    QSpinBox *m_expiry;
    QPushButton *m_ok = nullptr;
};
