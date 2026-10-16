#include "core/Directory.h"

#include <QHash>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winldap.h>

#include <string>
#include <vector>

// RFC 4515: characters that would change the meaning of a search filter.
static std::wstring filterEscaped(const QString &value)
{
    std::wstring out;
    for (const QChar c : value) {
        switch (c.unicode()) {
        case '*': out += L"\\2a"; break;
        case '(': out += L"\\28"; break;
        case ')': out += L"\\29"; break;
        case '\\': out += L"\\5c"; break;
        case 0: out += L"\\00"; break;
        default: out += wchar_t(c.unicode());
        }
    }
    return out;
}

static std::optional<QVariantMap> lookUp(const QStringList &names)
{
    // Set at logon for domain accounts: OFFICE.LAN and the sAMAccountName.
    const QString domain = qEnvironmentVariable("USERDNSDOMAIN");
    const QString login = qEnvironmentVariable("USERNAME");
    if (domain.isEmpty() || login.isEmpty())
        return std::nullopt;

    std::wstring host = domain.toStdWString();
    LDAP *ld = ldap_initW(host.data(), LDAP_PORT);
    if (!ld)
        return std::nullopt;
    std::optional<QVariantMap> result;
    ULONG version = LDAP_VERSION3;
    ldap_set_optionW(ld, LDAP_OPT_PROTOCOL_VERSION, &version);
    ldap_set_optionW(ld, LDAP_OPT_REFERRALS, LDAP_OPT_OFF);
    l_timeval timeout{3, 0};
    if (ldap_connect(ld, &timeout) == LDAP_SUCCESS
        && ldap_bind_sW(ld, nullptr, nullptr, LDAP_AUTH_NEGOTIATE) == LDAP_SUCCESS) {
        // OFFICE.LAN -> DC=OFFICE,DC=LAN
        std::wstring base = L"DC=" + domain.split(QLatin1Char('.')).join(QStringLiteral(",DC=")).toStdWString();
        std::wstring filter = L"(&(objectCategory=person)(objectClass=user)(sAMAccountName=" + filterEscaped(login)
            + L"))";
        std::vector<std::wstring> wanted;
        for (const QString &n : names)
            wanted.push_back(n.toStdWString());
        std::vector<PWCHAR> attrs;
        for (std::wstring &w : wanted)
            attrs.push_back(w.data());
        attrs.push_back(nullptr);

        LDAPMessage *res = nullptr;
        const ULONG rc = ldap_search_ext_sW(ld, base.data(), LDAP_SCOPE_SUBTREE, filter.data(), attrs.data(), 0,
                                            nullptr, nullptr, &timeout, 1, &res);
        LDAPMessage *entry = rc == LDAP_SUCCESS && res ? ldap_first_entry(ld, res) : nullptr;
        if (entry) {
            QVariantMap values;
            for (std::wstring &w : wanted) {
                PWCHAR *vals = ldap_get_valuesW(ld, entry, w.data());
                if (vals && ldap_count_valuesW(vals) > 0)
                    values.insert(QString::fromStdWString(w).toLower(), QString::fromWCharArray(vals[0]).trimmed());
                if (vals)
                    ldap_value_freeW(vals);
            }
            result = values;
        }
        if (res)
            ldap_msgfree(res);
    }
    ldap_unbind(ld);
    return result;
}
#endif

namespace Directory {

std::optional<QVariantMap> userAttributes(const QStringList &names)
{
    // Tests: "ipPhone=104;displayName=Anna" stands for the directory, unset for one out of reach.
    if (QStandardPaths::isTestModeEnabled()) {
        if (!qEnvironmentVariableIsSet("KKSIP_TEST_DIRECTORY"))
            return std::nullopt;
        QVariantMap values;
        const QStringList pairs = qEnvironmentVariable("KKSIP_TEST_DIRECTORY").split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const QString &pair : pairs)
            values.insert(pair.section(QLatin1Char('='), 0, 0).toLower(), pair.section(QLatin1Char('='), 1));
        return values;
    }
#ifdef Q_OS_WIN
    // Settings are loaded more than once at start; the directory is asked once.
    static QHash<QString, std::optional<QVariantMap>> asked;
    const QString key = names.join(QLatin1Char(','));
    if (!asked.contains(key))
        asked.insert(key, lookUp(names));
    return asked.value(key);
#else
    Q_UNUSED(names);
    return std::nullopt;
#endif
}

} // namespace Directory
