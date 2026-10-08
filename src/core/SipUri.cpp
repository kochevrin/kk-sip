#include "core/SipUri.h"

#include "core/Settings.h"

#include <QRegularExpression>
#include <QUrl>

namespace SipUri {

Parsed parse(const QString &uri)
{
    Parsed p;
    QString addr = uri.trimmed();

    const int lt = addr.indexOf(QLatin1Char('<'));
    if (lt >= 0) {
        p.displayName = addr.left(lt).trimmed();
        if (p.displayName.startsWith(QLatin1Char('"')) && p.displayName.endsWith(QLatin1Char('"'))
            && p.displayName.size() >= 2)
            p.displayName = p.displayName.mid(1, p.displayName.size() - 2);
        const int gt = addr.indexOf(QLatin1Char('>'), lt);
        addr = addr.mid(lt + 1, gt > lt ? gt - lt - 1 : -1);
    }

    static const QRegularExpression scheme(QStringLiteral("^(sips?|tel):"),
                                           QRegularExpression::CaseInsensitiveOption);
    addr.remove(scheme);
    const int semi = addr.indexOf(QLatin1Char(';'));
    if (semi >= 0)
        addr.truncate(semi);

    const int at = addr.indexOf(QLatin1Char('@'));
    if (at >= 0) {
        p.user = addr.left(at);
        p.host = addr.mid(at + 1);
    } else {
        p.user = addr;
    }
    p.user = QUrl::fromPercentEncoding(p.user.toUtf8());
    return p;
}

QString cleanNumber(const QString &input)
{
    QString s = input.trimmed();
    // Typical pasted phone formatting: spaces, dashes, dots, brackets.
    static const QRegularExpression junk(QStringLiteral("[\\s\\-\\.\\(\\)]"));
    if (!s.contains(QLatin1Char('@')))
        s.remove(junk);
    return s;
}

QString fromLink(const QString &link)
{
    QString s = QUrl::fromPercentEncoding(link.trimmed().toUtf8());
    static const QRegularExpression scheme(QStringLiteral("^(sips?|tel|callto):(//)?"),
                                           QRegularExpression::CaseInsensitiveOption);
    s.remove(scheme);
    return cleanNumber(s);
}

QString toTarget(const QString &input, const AccountConfig &account)
{
    QString s = cleanNumber(input);
    if (s.isEmpty())
        return {};

    const bool hasScheme = s.startsWith(QLatin1String("sip:"), Qt::CaseInsensitive)
        || s.startsWith(QLatin1String("sips:"), Qt::CaseInsensitive);
    if (!hasScheme) {
        s.remove(QRegularExpression(QStringLiteral("^(tel|callto):(//)?"),
                                    QRegularExpression::CaseInsensitiveOption));
        if (!s.contains(QLatin1Char('@')))
            s += QLatin1Char('@') + account.sipDomain();
        s.prepend(QStringLiteral("sip:"));
    }

    if (account.transport != QLatin1String("udp") && !s.contains(QLatin1String(";transport="), Qt::CaseInsensitive))
        s += QStringLiteral(";transport=") + account.transport;
    return s;
}

} // namespace SipUri
