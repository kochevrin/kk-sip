#pragma once

#include <QObject>
#include <QUrl>

class QNetworkAccessManager;

// Asks GitHub for the latest kk-sip release. Nothing is downloaded or installed:
// a newer version is only announced, with a link to its release page.
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject *parent = nullptr);

    // manual = the user asked: also report "up to date" and errors.
    void check(bool manual);

    // "v0.3.0" vs "0.2.1"; a leading "v" is ignored.
    static bool isNewer(const QString &latest, const QString &current);

    static QUrl releasesPage();

signals:
    void updateAvailable(const QString &version, const QUrl &page);
    void upToDate();                     // manual checks only
    void failed(const QString &error);   // manual checks only

private:
    QNetworkAccessManager *m_net;
};
