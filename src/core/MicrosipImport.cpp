#include "core/MicrosipImport.h"

#include "core/SipUri.h"

#include <QFile>
#include <QMap>
#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>
#include <QXmlStreamReader>

namespace MicrosipImport {

static QString decode(const QByteArray &raw)
{
    if (raw.startsWith("\xFF\xFE"))
        return QStringDecoder(QStringDecoder::Utf16LE)(raw.mid(2));
    if (raw.startsWith("\xEF\xBB\xBF"))
        return QString::fromUtf8(raw.mid(3));
    return QString::fromUtf8(raw);
}

QList<AccountConfig> readAccounts(const QString &iniPath, QString *error)
{
    QFile file(iniPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return {};
    }
    const QString text = decode(file.readAll());

    // section -> key -> value; QSettings can't read UTF-16 INI, so parse by hand.
    QMap<int, QMap<QString, QString>> sections;
    static const QRegularExpression accountSection(QStringLiteral("^\\[Account(\\d+)\\]$"),
                                                   QRegularExpression::CaseInsensitiveOption);
    int current = -1;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("\r?\n")));
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char(';')))
            continue;
        if (line.startsWith(QLatin1Char('['))) {
            const QRegularExpressionMatch m = accountSection.match(line);
            current = m.hasMatch() ? m.captured(1).toInt() : -1;
            continue;
        }
        const int eq = line.indexOf(QLatin1Char('='));
        if (current < 0 || eq <= 0)
            continue;
        sections[current].insert(line.left(eq).trimmed().toLower(), line.mid(eq + 1).trimmed());
    }

    QList<AccountConfig> out;
    for (auto it = sections.cbegin(); it != sections.cend(); ++it) {
        const QMap<QString, QString> &s = it.value();
        AccountConfig a;
        a.id = Settings::newAccountId();
        a.label = s.value(QStringLiteral("label"));
        a.server = s.value(QStringLiteral("server"));
        a.domain = s.value(QStringLiteral("domain"));
        a.proxy = s.value(QStringLiteral("proxy"));
        a.user = s.value(QStringLiteral("username"));
        a.authUser = s.value(QStringLiteral("authid"));
        a.displayName = s.value(QStringLiteral("displayname"));
        const QString tr = s.value(QStringLiteral("transport")).toLower();
        a.transport = (tr == QLatin1String("tcp") || tr == QLatin1String("tls")) ? tr : QStringLiteral("udp");
        a.srtp = !s.value(QStringLiteral("srtp")).isEmpty();
        a.enabled = false; // no password yet, don't hammer the PBX with failing REGISTERs
        if (a.server.isEmpty())
            a.server = a.domain;
        if (a.domain == a.server)
            a.domain.clear();
        if (a.server.isEmpty() || a.user.isEmpty())
            continue;
        out.append(a);
    }
    if (out.isEmpty() && error)
        *error = QObject::tr("No accounts found in this file.");
    return out;
}

QList<Contact> readContacts(QIODevice *xmlFile)
{
    QList<Contact> found;
    QXmlStreamReader xml(xmlFile);
    while (!xml.atEnd()) {
        if (xml.readNext() == QXmlStreamReader::StartElement && xml.name() == QLatin1String("contact")) {
            Contact c;
            c.name = xml.attributes().value(QLatin1String("name")).toString().trimmed();
            c.number = SipUri::cleanNumber(xml.attributes().value(QLatin1String("number")).toString());
            if (!c.number.isEmpty())
                found.append(c);
        }
    }
    return found;
}

int mergeAccounts(QList<AccountConfig> &into, const QList<AccountConfig> &found)
{
    int added = 0;
    for (const AccountConfig &a : found) {
        const bool dup = std::any_of(into.cbegin(), into.cend(), [&](const AccountConfig &e) {
            return e.user == a.user && e.sipDomain() == a.sipDomain();
        });
        if (!dup) {
            into.append(a);
            ++added;
        }
    }
    return added;
}

int mergeContacts(Database *db, const QList<Contact> &found)
{
    QSet<QString> existing;
    const QList<Contact> current = db->contacts();
    for (const Contact &c : current)
        existing.insert(c.number);
    int added = 0;
    for (Contact c : found) {
        if (existing.contains(c.number))
            continue;
        if (c.name.isEmpty())
            c.name = c.number;
        existing.insert(c.number);
        db->saveContact(c);
        ++added;
    }
    return added;
}

} // namespace MicrosipImport
