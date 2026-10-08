#include "core/SingleInstance.h"

#include <QLocalServer>
#include <QLocalSocket>

#ifdef Q_OS_WIN
// Named pipes are visible to the whole machine (terminal servers: many users at once).
static QString userKey()
{
    return qEnvironmentVariable("USERDOMAIN") + QLatin1Char('-') + qEnvironmentVariable("USERNAME");
}
#else
#include <unistd.h>

static QString userKey()
{
    return QString::number(getuid());
}
#endif

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
    , m_name(QStringLiteral("kk-sip-%1").arg(userKey()))
{
    // A second, separate instance (testing, a second profile with its own XDG dirs).
    const QString instance = qEnvironmentVariable("KKSIP_INSTANCE");
    if (!instance.isEmpty())
        m_name += QLatin1Char('-') + instance;
}

bool SingleInstance::forwardToRunning(const QString &message)
{
    QLocalSocket socket;
    socket.connectToServer(m_name);
    if (!socket.waitForConnected(500))
        return false;
    socket.write(message.toUtf8() + '\n');
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();
    return true;
}

bool SingleInstance::listen()
{
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(m_name)) {
        // Stale socket after a crash.
        QLocalServer::removeServer(m_name);
        if (!m_server->listen(m_name))
            return false;
    }
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *s = m_server->nextPendingConnection()) {
            connect(s, &QLocalSocket::readyRead, s, [this, s] {
                while (s->canReadLine())
                    emit messageReceived(QString::fromUtf8(s->readLine()).trimmed());
            });
            connect(s, &QLocalSocket::disconnected, s, &QObject::deleteLater);
        }
    });
    return true;
}
