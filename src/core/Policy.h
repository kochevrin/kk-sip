#pragma once

#include <QString>
#include <QVariantMap>

// Settings an administrator sets for everyone (docs/provisioning.md):
//   Windows:      HKLM\SOFTWARE\Policies\kk-sip, then HKCU\SOFTWARE\Policies\kk-sip
//                 (a computer value wins over the user value of the same name)
//   Linux:        /etc/kk-sip/policy.ini
//   macOS:        /Library/Application Support/kk-sip/policy.ini
// Each value set there wins over the user's own and locks its control. Tests (test mode)
// read KKSIP_POLICY_FILE and the registry key kk-sip-test instead.
namespace Policy {

// Every value, keys in lower case: "checkupdates", "account2/server".
QVariantMap read();

// The value of a key, or an invalid QVariant when it is not set. Empty strings count
// as not set: Group Policy writes them for text fields the administrator left blank.
QVariant value(const QVariantMap &policy, const QString &key);

// A string value with %NAME% replaced by environment variables (%USERNAME%, %USER%).
// The INI reader splits "a, b" into a list; it is joined back.
QString string(const QVariant &value);

// Where the file is read from on this platform, empty on Windows (registry only).
QString filePath();

} // namespace Policy
