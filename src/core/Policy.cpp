#include "core/Policy.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

namespace Policy {

QString filePath()
{
    // Tests point at a file of their own; the machine's policy must not leak into them.
    if (QStandardPaths::isTestModeEnabled())
        return qEnvironmentVariable("KKSIP_POLICY_FILE");
#if defined(Q_OS_WIN)
    // Not a file under ProgramData: ordinary users may create files there.
    return {};
#elif defined(Q_OS_MACOS)
    return QStringLiteral("/Library/Application Support/kk-sip/policy.ini");
#else
    return QStringLiteral("/etc/kk-sip/policy.ini");
#endif
}

static void merge(QVariantMap &out, const QSettings &source)
{
    const QStringList keys = source.allKeys();
    for (const QString &key : keys)
        out.insert(key.toLower(), source.value(key));
}

QVariantMap read()
{
    QVariantMap out;
    const QString file = filePath();
    if (!file.isEmpty() && QFileInfo::exists(file))
        merge(out, QSettings(file, QSettings::IniFormat));
#ifdef Q_OS_WIN
    // Tests write their own key; the machine's policy must not leak into them.
    const QString key = QStandardPaths::isTestModeEnabled() ? QStringLiteral("\\Software\\Policies\\kk-sip-test")
                                                            : QStringLiteral("\\Software\\Policies\\kk-sip");
    merge(out, QSettings(QStringLiteral("HKEY_CURRENT_USER") + key, QSettings::NativeFormat));
    merge(out, QSettings(QStringLiteral("HKEY_LOCAL_MACHINE") + key, QSettings::NativeFormat));
#endif
    return out;
}

static QString raw(const QVariant &value)
{
    return value.userType() == QMetaType::QStringList ? value.toStringList().join(QStringLiteral(", "))
                                                      : value.toString();
}

// %AD:ipPhone% (directory attribute) or %USERNAME% (environment variable).
static const QRegularExpression &variable()
{
    static const QRegularExpression re(QStringLiteral("%(?:AD:([A-Za-z][A-Za-z0-9-]*)|([A-Za-z_][A-Za-z0-9_]*))%"),
                                       QRegularExpression::CaseInsensitiveOption);
    return re;
}

QVariant value(const QVariantMap &policy, const QString &key)
{
    const QVariant v = policy.value(key.toLower());
    if (!v.isValid() || raw(v).trimmed().isEmpty())
        return {};
    return v;
}

QString string(const QVariant &value, const QVariantMap &directory)
{
    const QString s = raw(value);
    QString out;
    qsizetype last = 0;
    for (auto it = variable().globalMatch(s); it.hasNext();) {
        const QRegularExpressionMatch m = it.next();
        out += s.mid(last, m.capturedStart() - last);
        if (m.hasCaptured(1)) {
            const QString attribute = directory.value(m.captured(1).toLower()).toString();
            if (attribute.isEmpty())
                return {};
            out += attribute;
        } else {
            const QByteArray name = m.captured(2).toLocal8Bit();
            out += qEnvironmentVariableIsSet(name.constData()) ? qEnvironmentVariable(name.constData()) : m.captured(0);
        }
        last = m.capturedEnd();
    }
    out += s.mid(last);
    return out.trimmed();
}

QStringList directoryNames(const QVariantMap &policy)
{
    QStringList names;
    for (auto it = policy.cbegin(); it != policy.cend(); ++it)
        for (auto m = variable().globalMatch(raw(it.value())); m.hasNext();) {
            const QString name = m.next().captured(1).toLower();
            if (!name.isEmpty() && !names.contains(name))
                names << name;
        }
    names.sort();
    return names;
}

} // namespace Policy
