#pragma once

#include <QString>

struct AccountConfig;

namespace SipUri {

struct Parsed {
    QString displayName; // "Alice" from "Alice" <sip:101@pbx>
    QString user;        // 101
    QString host;        // pbx
};

// Parses a From/To/remote URI such as `"Alice" <sip:101@pbx;transport=tcp>`.
Parsed parse(const QString &uri);

// Builds a dialable SIP URI from whatever the user typed (101, +380..., sip:a@b, a@b),
// using the account's domain and transport. Returns empty string for unusable input.
QString toTarget(const QString &input, const AccountConfig &account);

// Strips the scheme and pretty characters from a sip:/tel:/callto: link.
QString fromLink(const QString &link);

// Keeps only characters that make sense in a phone number typed or pasted by the user.
QString cleanNumber(const QString &input);

} // namespace SipUri
