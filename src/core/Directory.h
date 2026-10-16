#pragma once

#include <QStringList>
#include <QVariantMap>

#include <optional>

// The signed-in user's own entry in Active Directory, for %AD:ipPhone%-style values in
// the policy (docs/provisioning.md).
namespace Directory {

// Attribute values by lower-case name, {"ipphone": "104", "displayname": "Ivan Petrov"};
// attributes the account does not have are left out. std::nullopt when the directory
// could not be asked: not Windows, a local account, no domain controller in reach.
// Windows asks LDAP as the signed-in user, with a 3 second limit to reach the server.
std::optional<QVariantMap> userAttributes(const QStringList &names);

} // namespace Directory
