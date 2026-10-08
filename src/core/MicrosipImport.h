#pragma once

#include "core/Database.h"
#include "core/Settings.h"

#include <QList>
#include <QString>

class QIODevice;

namespace MicrosipImport {

// Reads accounts from MicroSIP's microsip.ini (UTF-16LE INI, [Account1], [Account2]...).
// Passwords are encrypted with a Windows key there, so they are left empty and the
// accounts come in disabled; entering a password in the account dialog enables them.
QList<AccountConfig> readAccounts(const QString &iniPath, QString *error);

// Reads MicroSIP's Contacts.xml: <contacts><contact name="..." number="..."/></contacts>.
QList<Contact> readContacts(QIODevice *xml);

// Adds accounts that are not configured yet (same user@domain). Returns how many were added.
int mergeAccounts(QList<AccountConfig> &into, const QList<AccountConfig> &found);

// Saves contacts whose number is not in the phone book yet. Returns how many were added.
int mergeContacts(Database *db, const QList<Contact> &found);

} // namespace MicrosipImport
