#include "core/Settings.h"

#include "core/Policy.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>

QString AccountConfig::title() const
{
    if (!label.isEmpty())
        return label;
    return user + QLatin1Char('@') + sipDomain();
}

bool AccountConfig::sameSipConfig(const AccountConfig &o) const
{
    return server == o.server && domain == o.domain && proxy == o.proxy && user == o.user && authUser == o.authUser
        && password == o.password && displayName == o.displayName && transport == o.transport
        && srtp == o.srtp && natRewrite == o.natRewrite && enabled == o.enabled && regExpiry == o.regExpiry;
}

Settings &Settings::instance()
{
    static Settings s;
    return s;
}

QString Settings::configDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

QString Settings::dataDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString Settings::newAccountId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

static QString iniPath()
{
    return Settings::instance().configDir() + QStringLiteral("/kk-sip.ini");
}

bool Settings::anyLocked() const
{
    return !m_locked.isEmpty()
        || std::any_of(accounts.cbegin(), accounts.cend(), [](const AccountConfig &a) { return a.managed; });
}

// The policy's accounts: every group whose name starts with "Account" and has a Server.
// Fields the policy sets are locked; the rest come from what the user saved for the
// same account (`saved`) or stay empty for the user to fill in.
static QList<AccountConfig> managedAccounts(const QVariantMap &policy, const QList<AccountConfig> &saved,
                                            QStringList *pending)
{
    QStringList groups;
    for (auto it = policy.cbegin(); it != policy.cend(); ++it) {
        if (!it.key().contains(QLatin1Char('/')))
            continue;
        const QString group = it.key().section(QLatin1Char('/'), 0, 0);
        if (group.startsWith(QLatin1String("account")) && !groups.contains(group))
            groups << group;
    }
    // Account, Account2, ..., Account10: shorter names first.
    std::sort(groups.begin(), groups.end(), [](const QString &a, const QString &b) {
        return a.size() != b.size() ? a.size() < b.size() : a < b;
    });

    QList<AccountConfig> out;
    for (const QString &group : std::as_const(groups)) {
        const QString id = QStringLiteral("policy-") + group;
        auto stub = std::find_if(saved.cbegin(), saved.cend(), [&](const AccountConfig &a) { return a.id == id; });
        const bool known = stub != saved.cend();
        AccountConfig a = known ? *stub : AccountConfig{};
        const bool wasComplete = known && a.complete();
        a.id = id;
        a.managed = true;
        a.locked.clear();
        auto field = [&](const char *name) {
            const QVariant v = Policy::value(policy, group + QLatin1Char('/') + QLatin1String(name));
            if (v.isValid())
                a.locked << QLatin1String(name);
            return v;
        };
        auto text = [&](const char *name, QString &target) {
            if (const QVariant v = field(name); v.isValid())
                target = Policy::string(v);
        };
        text("label", a.label);
        text("server", a.server);
        text("domain", a.domain);
        text("proxy", a.proxy);
        text("user", a.user);
        text("authUser", a.authUser);
        text("password", a.password);
        text("displayName", a.displayName);
        if (const QVariant v = field("transport"); v.isValid()) {
            const QString t = Policy::string(v).toLower();
            if (t == QLatin1String("udp") || t == QLatin1String("tcp") || t == QLatin1String("tls"))
                a.transport = t;
        }
        if (const QVariant v = field("srtp"); v.isValid())
            a.srtp = v.toBool();
        if (const QVariant v = field("natRewrite"); v.isValid())
            a.natRewrite = v.toBool();
        if (const QVariant v = field("regExpiry"); v.isValid())
            a.regExpiry = qBound(60, v.toInt(), 3600);
        if (!a.isLocked("server"))
            continue;
        // Off until it has a user and a password; then on, unless the user turned it off.
        if (!known || (!wasComplete && a.complete()))
            a.enabled = a.complete();
        if (!known && !a.complete())
            pending->append(id);
        out << a;
    }
    return out;
}

void Settings::load()
{
    QSettings ini(iniPath(), QSettings::IniFormat);

    // Versions up to 0.1.0 wrote the group as "general", which QSettings stores as
    // [%General] and then reads back only as "General": the values never loaded.
    ini.beginGroup(ini.childGroups().contains(QStringLiteral("app")) ? QStringLiteral("app")
                                                                     : QStringLiteral("General"));
    currentAccountId = ini.value("currentAccount").toString();
    captureDevice = ini.value("captureDevice").toString();
    playbackDevice = ini.value("playbackDevice").toString();
    ringtoneFile = ini.value("ringtone").toString();
    closeToTray = ini.value("closeToTray", true).toBool();
    startHidden = ini.value("startHidden", true).toBool();
    doNotDisturb = ini.value("doNotDisturb", false).toBool();
    debugLog = ini.value("debugLog", false).toBool();
    theme = ini.value("theme", "system").toString();
    language = ini.value("language").toString();
    systemFrame = ini.value("systemFrame", false).toBool();
    rememberPosition = ini.value("rememberPosition", true).toBool();
    historyGrouped = ini.value("historyGrouped", true).toBool();
    wideView = ini.value("wideView", false).toBool();
    checkUpdates = ini.value("checkUpdates", true).toBool();
    trayHintShown = ini.value("trayHintShown", false).toBool();
    sipPort = ini.value("sipPort", 0).toInt();
    windowGeometry = ini.value("geometry").toByteArray();
    codecs.clear();
    const QStringList codecList = ini.value("codecs").toStringList();
    for (const QString &entry : codecList) {
        // "opus/48000/2:1"
        const int sep = entry.lastIndexOf(QLatin1Char(':'));
        if (sep <= 0)
            continue;
        codecs.append({entry.left(sep), entry.mid(sep + 1) == QLatin1String("1")});
    }

    // The administrator's values win and lock their controls; the user's own are kept
    // for save(), so they come back when the policy goes away.
    const QVariantMap policy = Policy::read();
    m_locked.clear();
    m_userValues.clear();
    autostart = false;
    auto enforced = [&](const char *key) {
        const QVariant v = Policy::value(policy, QLatin1String(key));
        if (v.isValid()) {
            m_locked.insert(QLatin1String(key));
            m_userValues.insert(QLatin1String(key), ini.value(key));
        }
        return v;
    };
    if (const QVariant v = enforced("autostart"); v.isValid())
        autostart = v.toBool();
    if (const QVariant v = enforced("closeToTray"); v.isValid())
        closeToTray = v.toBool();
    if (const QVariant v = enforced("checkUpdates"); v.isValid())
        checkUpdates = v.toBool();
    if (const QVariant v = enforced("sipPort"); v.isValid())
        sipPort = qBound(0, v.toInt(), 65535);
    if (const QVariant v = enforced("language"); v.isValid()) {
        const QString code = Policy::string(v).toLower(); // "system" too
        language = QStringList{QStringLiteral("en"), QStringLiteral("uk"), QStringLiteral("ru")}.contains(code)
            ? code
            : QString();
    }
    if (const QVariant v = enforced("codecs"); v.isValid()) {
        // "opus, G722, PCMA": in that order, anything not named is off (SipEngine::applyCodecs).
        codecs.clear();
        static const QRegularExpression separators(QStringLiteral("[,;\\s]+"));
        const QStringList names = Policy::string(v).split(separators, Qt::SkipEmptyParts);
        for (const QString &name : names)
            codecs.append({name, true});
    }
    m_userValues.remove(QStringLiteral("autostart")); // not in the file: the autostart entry itself
    ini.endGroup();

    QList<AccountConfig> saved;
    const int n = ini.beginReadArray(QStringLiteral("accounts"));
    for (int i = 0; i < n; ++i) {
        ini.setArrayIndex(i);
        AccountConfig a;
        a.id = ini.value("id").toString();
        if (a.id.isEmpty())
            a.id = newAccountId();
        a.label = ini.value("label").toString();
        a.server = ini.value("server").toString();
        a.domain = ini.value("domain").toString();
        a.proxy = ini.value("proxy").toString();
        a.user = ini.value("user").toString();
        a.authUser = ini.value("authUser").toString();
        a.password = ini.value("password").toString();
        a.displayName = ini.value("displayName").toString();
        a.transport = ini.value("transport", "udp").toString();
        a.srtp = ini.value("srtp", false).toBool();
        a.natRewrite = ini.value("natRewrite", false).toBool();
        a.enabled = ini.value("enabled", true).toBool();
        a.regExpiry = ini.value("regExpiry", 300).toInt();
        a.managed = ini.value("managed", false).toBool();
        saved.append(a);
    }
    ini.endArray();

    // Managed accounts keep the place the user moved them to; new ones go on top, and
    // ones the policy no longer has disappear.
    pendingCredentials.clear();
    const QList<AccountConfig> managed = managedAccounts(policy, saved, &pendingCredentials);
    accounts.clear();
    for (const AccountConfig &m : managed)
        if (std::none_of(saved.cbegin(), saved.cend(), [&](const AccountConfig &a) { return a.id == m.id; }))
            accounts.append(m);
    for (const AccountConfig &a : std::as_const(saved)) {
        if (!a.managed) {
            accounts.append(a);
            continue;
        }
        auto m = std::find_if(managed.cbegin(), managed.cend(), [&](const AccountConfig &x) { return x.id == a.id; });
        if (m != managed.cend())
            accounts.append(*m);
    }
}

void Settings::save() const
{
    QDir().mkpath(configDir());
    {
        QSettings ini(iniPath(), QSettings::IniFormat);
        ini.clear();

        ini.beginGroup(QStringLiteral("app"));
        ini.setValue("currentAccount", currentAccountId);
        ini.setValue("captureDevice", captureDevice);
        ini.setValue("playbackDevice", playbackDevice);
        ini.setValue("ringtone", ringtoneFile);
        ini.setValue("closeToTray", closeToTray);
        ini.setValue("startHidden", startHidden);
        ini.setValue("doNotDisturb", doNotDisturb);
        ini.setValue("debugLog", debugLog);
        ini.setValue("theme", theme);
        ini.setValue("language", language);
        ini.setValue("systemFrame", systemFrame);
        ini.setValue("rememberPosition", rememberPosition);
        ini.setValue("historyGrouped", historyGrouped);
        ini.setValue("wideView", wideView);
        ini.setValue("checkUpdates", checkUpdates);
        ini.setValue("trayHintShown", trayHintShown);
        ini.setValue("sipPort", sipPort);
        ini.setValue("geometry", windowGeometry);
        QStringList codecList;
        for (const CodecSetting &c : codecs)
            codecList << c.id + (c.enabled ? QStringLiteral(":1") : QStringLiteral(":0"));
        ini.setValue("codecs", codecList);
        for (auto it = m_userValues.cbegin(); it != m_userValues.cend(); ++it) {
            if (it.value().isValid())
                ini.setValue(it.key(), it.value());
            else
                ini.remove(it.key());
        }
        ini.endGroup();

        ini.beginWriteArray(QStringLiteral("accounts"), int(accounts.size()));
        for (int i = 0; i < accounts.size(); ++i) {
            const AccountConfig &a = accounts.at(i);
            ini.setArrayIndex(i);
            ini.setValue("id", a.id);
            ini.setValue("label", a.label);
            ini.setValue("server", a.server);
            ini.setValue("domain", a.domain);
            ini.setValue("proxy", a.proxy);
            ini.setValue("user", a.user);
            ini.setValue("authUser", a.authUser);
            // A password from the policy stays there, not in the user's file.
            ini.setValue("password", a.isLocked("password") ? QString() : a.password);
            ini.setValue("displayName", a.displayName);
            ini.setValue("transport", a.transport);
            ini.setValue("srtp", a.srtp);
            ini.setValue("natRewrite", a.natRewrite);
            ini.setValue("enabled", a.enabled);
            ini.setValue("regExpiry", a.regExpiry);
            if (a.managed)
                ini.setValue("managed", true);
        }
        ini.endArray();
    }
    // Passwords are stored in plain text, so keep the file private.
    QFile::setPermissions(iniPath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}
