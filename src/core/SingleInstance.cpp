#include "core/SingleInstance.h"

#include <QLocalServer>
#include <QLocalSocket>

#include <unistd.h>

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
    , m_name(QStringLiteral("kk-sip-%1").arg(getuid()))
{
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
