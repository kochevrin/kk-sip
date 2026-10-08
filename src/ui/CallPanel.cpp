#include "ui/CallPanel.h"

#include "ui/Icons.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QKeyEvent>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

CallPanel::CallPanel(SipEngine *engine, NameLookup lookup, QWidget *parent)
    : QFrame(parent)
    , m_engine(engine)
    , m_lookup(std::move(lookup))
{
    setObjectName(QStringLiteral("callPanel"));
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(8, 6, 8, 6);
    m_layout->setSpacing(6);

    m_clock = new QTimer(this);
    m_clock->setInterval(500);
    connect(m_clock, &QTimer::timeout, this, &CallPanel::refresh);

    connect(m_engine, &SipEngine::callChanged, this, &CallPanel::refresh);
    connect(m_engine, &SipEngine::incomingCall, this, &CallPanel::refresh);
    connect(m_engine, &SipEngine::callEnded, this, &CallPanel::refresh);
    hide();
}

QString CallPanel::stateText(const CallView &c)
{
    switch (c.state) {
    case CallView::Calling:
        return tr("Calling…");
    case CallView::Incoming:
        return tr("Incoming call");
    case CallView::Ringing:
        return tr("Ringing…");
    case CallView::Connecting:
        return tr("Connecting…");
    case CallView::Active: {
        const qint64 secs = c.connectedAt.secsTo(QDateTime::currentDateTime());
        QString t = QStringLiteral("%1:%2").arg(secs / 60).arg(secs % 60, 2, 10, QLatin1Char('0'));
        if (c.onHold)
            t += QStringLiteral(" · ") + tr("on hold");
        if (c.muted)
            t += QStringLiteral(" · ") + tr("muted");
        return t;
    }
    case CallView::Ended:
        return tr("Ended");
    }
    return {};
}

static QPushButton *smallButton(const QIcon &icon, const QString &text, const QString &tip, QWidget *parent)
{
    auto *b = new QPushButton(icon, icon.isNull() ? text : QString(), parent);
    b->setToolTip(tip);
    b->setFocusPolicy(Qt::NoFocus);
    b->setMinimumHeight(30);
    return b;
}

CallPanel::Row *CallPanel::createRow(int callId)
{
    auto *row = new Row;
    row->widget = new QWidget(this);
    connect(row->widget, &QObject::destroyed, this, [row] { delete row; });
    auto *v = new QVBoxLayout(row->widget);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(2);

    auto *top = new QHBoxLayout;
    row->title = new QLabel(row->widget);
    QFont f = row->title->font();
    f.setBold(true);
    row->title->setFont(f);
    row->title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    row->status = new QLabel(row->widget);
    top->addWidget(row->title, 1);
    top->addWidget(row->status);
    v->addLayout(top);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(4);
    row->answer = smallButton(Icons::callWhite(), tr("Answer"),
                              tr("Answer"), row->widget);
    row->answer->setText(tr("Answer"));
    row->mute = smallButton(Icons::get(QStringLiteral("audio-input-microphone")), tr("Mute"), tr("Mute microphone"),
                            row->widget);
    row->mute->setCheckable(true);
    row->hold = smallButton(Icons::get(QStringLiteral("media-playback-pause")), tr("Hold"), tr("Hold"), row->widget);
    row->hold->setCheckable(true);
    row->transfer = smallButton(Icons::get(QStringLiteral("mail-forward")), tr("Transfer"), tr("Transfer call"),
                                row->widget);
    row->hangup = smallButton(Icons::hangupWhite(), tr("Hang up"),
                              tr("Hang up"), row->widget);
    row->hangup->setObjectName(QStringLiteral("hangupButton"));
    row->answer->setObjectName(QStringLiteral("answerButton"));

    buttons->addWidget(row->answer);
    buttons->addWidget(row->mute);
    buttons->addWidget(row->hold);
    buttons->addWidget(row->transfer);
    buttons->addStretch(1);
    buttons->addWidget(row->hangup);
    v->addLayout(buttons);

    row->transferBox = new QWidget(row->widget);
    auto *tl = new QHBoxLayout(row->transferBox);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(4);
    row->transferEdit = new QLineEdit(row->transferBox);
    row->transferEdit->setPlaceholderText(tr("Transfer to number…"));
    row->transferEdit->setClearButtonEnabled(true);
    auto *go = new QPushButton(Icons::get(QStringLiteral("mail-forward")), QString(), row->transferBox);
    if (go->icon().isNull())
        go->setText(QStringLiteral("→"));
    go->setToolTip(tr("Transfer"));
    tl->addWidget(row->transferEdit, 1);
    tl->addWidget(go);
    row->transferBox->hide();
    v->addWidget(row->transferBox);
    row->transfer->setCheckable(true);

    // Buttons swap places when a call changes state (Answer -> Mute/Hold/Transfer), so a
    // double click or a click that falls through a closing popup must not hit the new ones.
    auto guarded = [this, row](auto fn) {
        return [this, row, fn] {
            if (justAppeared(row))
                QTimer::singleShot(0, this, &CallPanel::refresh); // undo the button's own toggle
            else
                fn();
        };
    };
    connect(row->answer, &QPushButton::clicked, this, [this, callId] { m_engine->answer(callId); });
    connect(row->hangup, &QPushButton::clicked, this, [this, callId] { m_engine->hangup(callId); });
    // Toggle from the call's real state, not from the button's checked flag.
    connect(row->mute, &QPushButton::clicked, this,
            guarded([this, callId] { m_engine->setMute(callId, !m_engine->call(callId).muted); }));
    connect(row->hold, &QPushButton::clicked, this,
            guarded([this, callId] { m_engine->setHold(callId, !m_engine->call(callId).onHold); }));
    connect(row->transfer, &QPushButton::clicked, this, guarded([row] {
        const bool show = !row->transferBox->isVisible();
        row->transferBox->setVisible(show);
        row->transfer->setChecked(show);
        if (show)
            row->transferEdit->setFocus();
    }));
    connect(row->transferEdit, &QLineEdit::returnPressed, this, [this, callId, row] { doTransfer(callId, row); });
    connect(go, &QPushButton::clicked, this, [this, callId, row] { doTransfer(callId, row); });
    row->transferEdit->installEventFilter(this); // Esc closes the transfer field

    m_layout->addWidget(row->widget);
    return row;
}

void CallPanel::updateRow(Row *row, const CallView &c)
{
    const QString contactName = m_lookup ? m_lookup(c.number) : QString();
    const QString name = !contactName.isEmpty() ? contactName : c.name;
    row->title->setText(name.isEmpty() || name == c.number ? c.number : name + QStringLiteral("  ") + c.number);
    row->status->setText(stateText(c));

    if (c.state != row->shownState) {
        row->shownState = c.state;
        row->armedAt = QDateTime::currentMSecsSinceEpoch();
    }
    const bool active = c.state == CallView::Active;
    row->answer->setVisible(c.state == CallView::Incoming);
    row->mute->setVisible(active);
    row->hold->setVisible(active);
    row->transfer->setVisible(active);
    row->mute->setChecked(c.muted);
    row->hold->setChecked(c.onHold);
    if (!active)
        row->transferBox->hide();
    row->transfer->setChecked(row->transferBox->isVisible());
    row->hangup->setText(c.state == CallView::Incoming ? tr("Reject") : QString());
}

void CallPanel::refresh()
{
    const QList<CallView> calls = m_engine->calls();
    QSet<int> alive;
    for (const CallView &c : calls) {
        alive.insert(c.id);
        Row *row = m_rows.value(c.id);
        if (!row) {
            row = createRow(c.id);
            m_rows.insert(c.id, row);
        }
        updateRow(row, c);
    }
    for (auto it = m_rows.begin(); it != m_rows.end();) {
        if (!alive.contains(it.key())) {
            // The Row goes away with its widget (see createRow), never before its buttons.
            it.value()->widget->hide();
            it.value()->widget->deleteLater();
            it = m_rows.erase(it);
        } else {
            ++it;
        }
    }

    setVisible(!calls.isEmpty());
    if (calls.isEmpty())
        m_clock->stop();
    else if (!m_clock->isActive())
        m_clock->start();
}

bool CallPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
        for (Row *row : std::as_const(m_rows)) {
            if (row->transferEdit == watched) {
                row->transferBox->hide();
                row->transfer->setChecked(false);
                return true;
            }
        }
    }
    return QFrame::eventFilter(watched, event);
}

bool CallPanel::justAppeared(const Row *row)
{
    return QDateTime::currentMSecsSinceEpoch() - row->armedAt < 700;
}

void CallPanel::doTransfer(int callId, Row *row)
{
    const QString target = row->transferEdit->text().trimmed();
    if (target.isEmpty())
        return;
    QString error;
    if (!m_engine->transfer(callId, target, &error)) {
        row->transferEdit->clear();
        row->transferEdit->setPlaceholderText(tr("Transfer failed: %1").arg(error));
        return;
    }
    row->transferEdit->setEnabled(false);
    row->transferEdit->setText(tr("Transferring to %1…").arg(target));
}
