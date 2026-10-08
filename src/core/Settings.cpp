#include "core/Settings.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>

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
        && srtp == o.srtp && enabled == o.enabled && regExpiry == o.regExpiry;
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

void Settings::load()
{
    QSettings ini(iniPath(), QSettings::IniFormat);

    ini.beginGroup(QStringLiteral("general"));
    currentAccountId = ini.value("currentAccount").toString();
    captureDevice = ini.value("captureDevice").toString();
    playbackDevice = ini.value("playbackDevice").toString();
    ringtoneFile = ini.value("ringtone").toString();
    closeToTray = ini.value("closeToTray", true).toBool();
    startHidden = ini.value("startHidden", false).toBool();
    doNotDisturb = ini.value("doNotDisturb", false).toBool();
    debugLog = ini.value("debugLog", false).toBool();
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
    ini.endGroup();

    accounts.clear();
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
        a.enabled = ini.value("enabled", true).toBool();
        a.regExpiry = ini.value("regExpiry", 300).toInt();
        accounts.append(a);
    }
    ini.endArray();
}

void Settings::save() const
{
    QDir().mkpath(configDir());
    {
        QSettings ini(iniPath(), QSettings::IniFormat);
        ini.clear();

        ini.beginGroup(QStringLiteral("general"));
        ini.setValue("currentAccount", currentAccountId);
        ini.setValue("captureDevice", captureDevice);
        ini.setValue("playbackDevice", playbackDevice);
        ini.setValue("ringtone", ringtoneFile);
        ini.setValue("closeToTray", closeToTray);
        ini.setValue("startHidden", startHidden);
        ini.setValue("doNotDisturb", doNotDisturb);
        ini.setValue("debugLog", debugLog);
        ini.setValue("sipPort", sipPort);
        ini.setValue("geometry", windowGeometry);
        QStringList codecList;
        for (const CodecSetting &c : codecs)
            codecList << c.id + (c.enabled ? QStringLiteral(":1") : QStringLiteral(":0"));
        ini.setValue("codecs", codecList);
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
            ini.setValue("password", a.password);
            ini.setValue("displayName", a.displayName);
            ini.setValue("transport", a.transport);
            ini.setValue("srtp", a.srtp);
            ini.setValue("enabled", a.enabled);
            ini.setValue("regExpiry", a.regExpiry);
        }
        ini.endArray();
    }
    // Passwords are stored in plain text, so keep the file private.
    QFile::setPermissions(iniPath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}
