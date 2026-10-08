#pragma once

#include <QWidget>

class SipEngine;

// Small always-on-top window announcing an incoming call.
class IncomingDialog : public QWidget {
    Q_OBJECT
public:
    IncomingDialog(SipEngine *engine, int callId, const QString &caller, const QString &number,
                   const QString &accountTitle, QWidget *parent = nullptr);

    int callId() const { return m_callId; }

private:
    SipEngine *m_engine;
    int m_callId;
};
