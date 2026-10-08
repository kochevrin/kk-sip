#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

// Keeps one kk-sip per user. A second launch forwards its argument
// (a number or sip:/tel: link) to the running instance and exits.
class SingleInstance : public QObject {
    Q_OBJECT
public:
    explicit SingleInstance(QObject *parent = nullptr);

    // True when another instance received the message; the caller should quit.
    bool forwardToRunning(const QString &message);
    bool listen();

signals:
    void messageReceived(const QString &message);

private:
    QString m_name;
    QLocalServer *m_server = nullptr;
};
