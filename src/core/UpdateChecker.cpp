#include "core/UpdateChecker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QVersionNumber>

static const char *const Repo = "kochevrin/kk-sip";

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

QUrl UpdateChecker::releasesPage()
{
    return QUrl(QStringLiteral("https://github.com/%1/releases/latest").arg(QLatin1String(Repo)));
}

bool UpdateChecker::isNewer(const QString &latest, const QString &current)
{
    auto parse = [](QString v) {
        if (v.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
            v.remove(0, 1);
        return QVersionNumber::fromString(v);
    };
    const QVersionNumber l = parse(latest);
    return !l.isNull() && l > parse(current);
}

void UpdateChecker::check(bool manual)
{
    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(QLatin1String(Repo))));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("kk-sip/" KKSIP_VERSION));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(15000);
    QNetworkReply *reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, manual] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (manual)
                emit failed(reply->errorString());
            return;
        }
        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = release.value(QLatin1String("tag_name")).toString();
        if (tag.isEmpty()) {
            if (manual)
                emit failed(tr("Unexpected answer from GitHub"));
            return;
        }
        if (isNewer(tag, QStringLiteral(KKSIP_VERSION))) {
            QUrl page(release.value(QLatin1String("html_url")).toString());
            emit updateAvailable(tag.startsWith(QLatin1Char('v')) ? tag.mid(1) : tag,
                                 page.isValid() && !page.isEmpty() ? page : releasesPage());
        } else if (manual) {
            emit upToDate();
        }
    });
}
