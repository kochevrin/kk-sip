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
    if (!QStandardPaths::isTestModeEnabled()) {
        merge(out, QSettings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Policies\\kk-sip"), QSettings::NativeFormat));
        merge(out, QSettings(QStringLiteral("HKEY_LOCAL_MACHINE\\Software\\Policies\\kk-sip"), QSettings::NativeFormat));
    }
#endif
    return out;
}

QVariant value(const QVariantMap &policy, const QString &key)
{
    const QVariant v = policy.value(key.toLower());
    if (!v.isValid() || string(v).trimmed().isEmpty())
        return {};
    return v;
}

QString string(const QVariant &value)
{
    const QString s = value.userType() == QMetaType::QStringList ? value.toStringList().join(QStringLiteral(", "))
                                                                 : value.toString();
    static const QRegularExpression var(QStringLiteral("%([A-Za-z_][A-Za-z0-9_]*)%"));
    QString out;
    qsizetype last = 0;
    for (auto it = var.globalMatch(s); it.hasNext();) {
        const QRegularExpressionMatch m = it.next();
        out += s.mid(last, m.capturedStart() - last);
        const QByteArray name = m.captured(1).toLocal8Bit();
        out += qEnvironmentVariableIsSet(name.constData()) ? qEnvironmentVariable(name.constData()) : m.captured(0);
        last = m.capturedEnd();
    }
    out += s.mid(last);
    return out.trimmed();
}

} // namespace Policy
