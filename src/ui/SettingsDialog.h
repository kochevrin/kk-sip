#pragma once

#include "core/Settings.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QSpinBox;
class SipEngine;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(SipEngine *engine, QWidget *parent = nullptr);

    // Writes the edited values into Settings::instance() and saves the file.
    void commit();

private:
    QWidget *createAccountsPage();
    QWidget *createAudioPage();
    QWidget *createCodecsPage();
    QWidget *createGeneralPage();
    void reloadAccounts();
    void addAccount();
    void editAccount();
    void removeAccount();
    void moveAccount(int delta);
    void importMicrosip();
    void moveCodec(int delta);

    SipEngine *m_engine;
    QList<AccountConfig> m_accounts;

    QListWidget *m_accountList = nullptr;
    QComboBox *m_capture = nullptr;
    QComboBox *m_playback = nullptr;
    QLineEdit *m_ringtone = nullptr;
    QListWidget *m_codecList = nullptr;
    QCheckBox *m_closeToTray = nullptr;
    QCheckBox *m_startHidden = nullptr;
    QCheckBox *m_autostart = nullptr;
    QCheckBox *m_debugLog = nullptr;
    QSpinBox *m_sipPort = nullptr;
    QComboBox *m_theme = nullptr;
    QCheckBox *m_systemFrame = nullptr;
};
