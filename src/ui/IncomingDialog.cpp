#include "ui/IncomingDialog.h"

#include "sip/SipEngine.h"
#include "ui/Icons.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

IncomingDialog::IncomingDialog(SipEngine *engine, int callId, const QString &caller, const QString &number,
                               const QString &accountTitle, QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::WindowStaysOnTopHint)
    , m_engine(engine)
    , m_callId(callId)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Incoming call"));
    setWindowIcon(Icons::app());

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);

    auto *who = new QLabel(caller.isEmpty() ? number : caller, this);
    QFont f = who->font();
    f.setPointSizeF(f.pointSizeF() * 1.5);
    f.setBold(true);
    who->setFont(f);
    who->setAlignment(Qt::AlignCenter);
    layout->addWidget(who);

    if (!caller.isEmpty() && caller != number) {
        auto *num = new QLabel(number, this);
        num->setAlignment(Qt::AlignCenter);
        layout->addWidget(num);
    }
    auto *acc = new QLabel(tr("to %1").arg(accountTitle), this);
    acc->setAlignment(Qt::AlignCenter);
    acc->setEnabled(false);
    layout->addWidget(acc);
    layout->addSpacing(8);

    auto *buttons = new QHBoxLayout;
    auto *answer = new QPushButton(Icons::callWhite(), tr("Answer"), this);
    auto *reject = new QPushButton(Icons::hangupWhite(), tr("Reject"), this);
    answer->setMinimumHeight(36);
    reject->setMinimumHeight(36);
    answer->setStyleSheet(QStringLiteral(
        "QPushButton { background: #2eb84b; color: white; font-weight: bold; border-radius: 6px; padding: 0 16px; }"));
    reject->setStyleSheet(QStringLiteral(
        "QPushButton { background: #d93f3f; color: white; font-weight: bold; border-radius: 6px; padding: 0 16px; }"));
    buttons->addWidget(answer);
    buttons->addWidget(reject);
    layout->addLayout(buttons);
    answer->setDefault(true);

    connect(answer, &QPushButton::clicked, this, [this] {
        m_engine->answer(m_callId);
        close();
    });
    connect(reject, &QPushButton::clicked, this, [this] {
        m_engine->hangup(m_callId);
        close();
    });
    // Answered elsewhere (call panel) or caller gave up.
    connect(m_engine, &SipEngine::callChanged, this, [this](int id) {
        if (id == m_callId && m_engine->call(id).state != CallView::Incoming)
            close();
    });
    connect(m_engine, &SipEngine::callEnded, this, [this](const CallView &c) {
        if (c.id == m_callId)
            close();
    });

    setMinimumWidth(280);
    adjustSize();
}
