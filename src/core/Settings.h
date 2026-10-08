#pragma once

#include <QList>
#include <QString>
#include <QStringList>

struct AccountConfig {
    QString id;           // stable internal id, never shown
    QString label;        // name shown in the account switcher
    QString server;       // SIP server / registrar, e.g. pbx.example.com:5060
    QString domain;       // optional SIP domain for the address, defaults to server
    QString proxy;        // optional outbound proxy
    QString user;         // SIP user (extension)
    QString authUser;     // optional auth id, defaults to user
    QString password;
    QString displayName;
    QString transport = QStringLiteral("udp"); // udp | tcp | tls
    bool srtp = false;
    bool enabled = true;
    int regExpiry = 300;

    QString title() const;
    QString sipDomain() const { return domain.isEmpty() ? server : domain; }
    bool sameSipConfig(const AccountConfig &o) const;
};

struct CodecSetting {
    QString id;
    bool enabled = true;
};

// Persistent app settings in ~/.config/kk-sip/kk-sip.ini.
class Settings {
public:
    static Settings &instance();

    void load();
    void save() const;

    QList<AccountConfig> accounts;
    QString currentAccountId;

    QString captureDevice;  // "driver|name", empty = system default
    QString playbackDevice;
    QString ringtoneFile;   // optional WAV, empty = built-in tone
    QList<CodecSetting> codecs; // ordered by priority, empty = PJSIP defaults

    bool closeToTray = true;
    bool startHidden = true;   // autostart goes straight to the tray
    bool doNotDisturb = false;
    bool debugLog = false;
    QString theme = QStringLiteral("system"); // system | light | dark
    bool systemFrame = false; // native window frame instead of the slim kk-sip title strip
    int sipPort = 0;        // 0 = random local port

    QByteArray windowGeometry;

    QString configDir() const;
    QString dataDir() const;
    static QString newAccountId();

private:
    Settings() = default;
};
