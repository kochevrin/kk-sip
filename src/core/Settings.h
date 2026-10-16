#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>

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
    bool natRewrite = false; // rewrite Via/Contact/SDP from what the server sees (MicroSIP "allowRewrite")
    bool enabled = true;
    int regExpiry = 300;
    bool managed = false;  // comes from the administrator's policy (core/Policy.h)
    QStringList locked;    // fields the policy sets, by their ini names ("server", "user"...)

    QString title() const;
    bool isLocked(const char *field) const { return locked.contains(QLatin1String(field)); }
    bool complete() const { return !server.isEmpty() && !user.isEmpty() && !password.isEmpty(); }
    QString sipDomain() const { return domain.isEmpty() ? server : domain; }
    bool sameSipConfig(const AccountConfig &o) const;
};

struct CodecSetting {
    QString id;
    bool enabled = true;
};

// Persistent app settings in ~/.config/kk-sip/kk-sip.ini, with the administrator's
// policy (core/Policy.h) laid over them by load().
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
    QString language; // "" = system, or en | uk | ru
    bool rememberPosition = true;
    bool systemFrame = false; // native window frame instead of the slim kk-sip title strip
    bool historyGrouped = true; // history: calls grouped under each number, or one flat list
    bool wideView = false;  // wide window: dial pad with history and contacts next to it
    bool checkUpdates = true;   // ask GitHub once a day whether a newer release exists
    bool trayHintShown = false; // told once that closing the window keeps kk-sip in the tray
    int sipPort = 0;        // 0 = random local port
    bool autostart = false; // only from the policy: whether it starts kk-sip at logon

    QByteArray windowGeometry;

    // Settings the policy sets, by their ini names ("checkUpdates", "autostart"...).
    bool isLocked(const char *key) const { return m_locked.contains(QLatin1String(key)); }
    bool anyLocked() const;
    // Managed accounts that appeared with this load() and still miss a user or password.
    QStringList pendingCredentials;

    QString configDir() const;
    QString dataDir() const;
    static QString newAccountId();

private:
    Settings() = default;
    QSet<QString> m_locked;
    QVariantMap m_userValues; // the user's own values of locked settings, kept by save()
    QVariantMap m_directory;  // last answer from Active Directory for the policy's %AD:name%
};
