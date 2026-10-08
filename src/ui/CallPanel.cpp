#include "ui/CallPanel.h"

#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
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
    m_layout->setContentsMargins(10, 8, 8, 8);
    m_layout->setSpacing(6);

    m_clock = new QTimer(this);
    m_clock->setInterval(500);
    connect(m_clock, &QTimer::timeout, this, &CallPanel::refresh);

    connect(m_engine, &SipEngine::callChanged, this, &CallPanel::refresh);
    connect(m_engine, &SipEngine::incomingCall, this, &CallPanel::refresh);
    connect(m_engine, &SipEngine::callEnded, this, &CallPanel::refresh);
    connect(Theme::Notifier::instance(), &Theme::Notifier::changed, this, [this] {
        for (Row *row : std::as_const(m_rows))
            row->muteIcon = -1; // repaint the mute icon in the new text colour
        refresh();
    });
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

namespace {

// One line of text cut with "…" when it does not fit; never widens the panel.
class ElidedLabel : public QLabel {
public:
    using QLabel::QLabel;
    QSize minimumSizeHint() const override { return {16, QLabel::minimumSizeHint().height()}; }
    QSize sizeHint() const override { return {16, QLabel::sizeHint().height()}; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setPen(palette().color(foregroundRole()));
        p.drawText(contentsRect(), Qt::AlignLeft | Qt::AlignVCenter,
                   fontMetrics().elidedText(text(), Qt::ElideRight, contentsRect().width()));
    }
};

// Square icon button for the call actions; the text lives in the tooltip.
QPushButton *actionButton(const QString &icon, const QString &tip, QWidget *parent)
{
    auto *b = new QPushButton(parent);
    b->setObjectName(QStringLiteral("callAction"));
    Icons::setThemed(b, icon);
    b->setIconSize(QSize(18, 18));
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    b->setFocusPolicy(Qt::NoFocus);
    b->setFixedSize(34, 32);
    return b;
}

} // namespace

CallPanel::Row *CallPanel::createRow(int callId)
{
    auto *row = new Row;
    row->widget = new QWidget(this);
    connect(row->widget, &QObject::destroyed, this, [row] { delete row; });
    auto *v = new QVBoxLayout(row->widget);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);

    // Who and how long on the left, the actions on the right, in one line.
    auto *top = new QHBoxLayout;
    top->setSpacing(4);
    auto *text = new QVBoxLayout;
    text->setSpacing(0);
    row->title = new ElidedLabel(row->widget);
    QFont f = row->title->font();
    f.setBold(true);
    row->title->setFont(f);
    row->status = new ElidedLabel(row->widget);
    row->status->setObjectName(QStringLiteral("callStatus"));
    QFont sf = row->status->font();
    sf.setPointSizeF(sf.pointSizeF() * 0.9);
    row->status->setFont(sf);
    text->addWidget(row->title);
    text->addWidget(row->status);
    top->addLayout(text, 1);
    top->addSpacing(4);

    row->mute = actionButton(QStringLiteral("mic"), tr("Mute microphone"), row->widget);
    row->mute->setCheckable(true);
    row->hold = actionButton(QStringLiteral("pause"), tr("Hold"), row->widget);
    row->hold->setCheckable(true);
    row->transfer = actionButton(QStringLiteral("transfer"), tr("Transfer call"), row->widget);
    row->transfer->setCheckable(true);
    row->hangup = new QPushButton(Icons::hangupWhite(), QString(), row->widget);
    row->hangup->setObjectName(QStringLiteral("hangupButton"));
    row->hangup->setToolTip(tr("Hang up"));
    row->hangup->setAccessibleName(tr("Hang up"));
    row->hangup->setFocusPolicy(Qt::NoFocus);
    row->hangup->setIconSize(QSize(20, 20));
    row->hangup->setFixedSize(46, 32);
    top->addWidget(row->mute);
    top->addWidget(row->hold);
    top->addWidget(row->transfer);
    top->addSpacing(2);
    top->addWidget(row->hangup);
    v->addLayout(top);

    // Ringing: two wide buttons, there is room for the words.
    row->incomingBox = new QWidget(row->widget);
    auto *ib = new QHBoxLayout(row->incomingBox);
    ib->setContentsMargins(0, 0, 0, 0);
    ib->setSpacing(6);
    row->answer = new QPushButton(Icons::callWhite(), tr("Answer"), row->incomingBox);
    row->answer->setObjectName(QStringLiteral("answerButton"));
    row->reject = new QPushButton(Icons::hangupWhite(), tr("Reject"), row->incomingBox);
    row->reject->setObjectName(QStringLiteral("hangupButton"));
    for (QPushButton *b : {row->answer, row->reject}) {
        b->setFocusPolicy(Qt::NoFocus);
        b->setFixedHeight(32);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        ib->addWidget(b);
    }
    row->incomingBox->hide();
    v->addWidget(row->incomingBox);

    row->transferBox = new QWidget(row->widget);
    auto *tl = new QHBoxLayout(row->transferBox);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(4);
    row->transferEdit = new QLineEdit(row->transferBox);
    row->transferEdit->setPlaceholderText(tr("Transfer to number…"));
    row->transferEdit->setClearButtonEnabled(true);
    auto *go = new QPushButton(row->transferBox);
    go->setObjectName(QStringLiteral("callAction"));
    Icons::setThemed(go, QStringLiteral("transfer"));
    go->setToolTip(tr("Transfer"));
    go->setFixedSize(34, 32);
    tl->addWidget(row->transferEdit, 1);
    tl->addWidget(go);
    row->transferBox->hide();
    v->addWidget(row->transferBox);

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
    connect(row->reject, &QPushButton::clicked, this, [this, callId] { m_engine->hangup(callId); });
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
    // Name on top, number and call state under it; just the number when there's no name.
    const bool named = !name.isEmpty() && name != c.number;
    row->title->setText(named ? name : c.number);
    row->status->setText(named ? c.number + QStringLiteral(" · ") + stateText(c) : stateText(c));

    if (c.state != row->shownState) {
        row->shownState = c.state;
        row->armedAt = QDateTime::currentMSecsSinceEpoch();
    }
    const bool active = c.state == CallView::Active;
    const bool incoming = c.state == CallView::Incoming;
    row->incomingBox->setVisible(incoming);
    row->hangup->setVisible(!incoming);
    row->mute->setVisible(active);
    row->hold->setVisible(active);
    row->transfer->setVisible(active);
    row->mute->setChecked(c.muted);
    row->hold->setChecked(c.onHold);
    if (row->muteIcon != int(c.muted)) {
        row->muteIcon = int(c.muted);
        row->mute->setIcon(Icons::tinted(c.muted ? QStringLiteral("mic-off") : QStringLiteral("mic"),
                                         Theme::colors().foreground));
        row->mute->setToolTip(c.muted ? tr("Unmute microphone") : tr("Mute microphone"));
    }
    if (!active)
        row->transferBox->hide();
    row->transfer->setChecked(row->transferBox->isVisible());
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
