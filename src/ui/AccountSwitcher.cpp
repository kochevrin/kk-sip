#include "ui/AccountSwitcher.h"

#include "core/Settings.h"
#include "sip/SipEngine.h"
#include "ui/Icons.h"

#include "ui/Theme.h"

#include <QAbstractButton>
#include <QPainter>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <functional>

namespace {

// iOS/Android-style toggle: pill with a knob, green when on.
class ToggleSwitch : public QAbstractButton {
public:
    explicit ToggleSwitch(QWidget *parent)
        : QAbstractButton(parent)
    {
        setCheckable(true);
        setFixedSize(34, 20);
        setCursor(Qt::ArrowCursor);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Theme::Colors &c = Theme::colors();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(isChecked() ? c.ok : c.input);
        p.drawRoundedRect(QRectF(rect()), height() / 2.0, height() / 2.0);
        const qreal d = height() - 4;
        const qreal x = isChecked() ? width() - d - 2 : 2;
        p.setBrush(QColor(Qt::white));
        p.drawEllipse(QRectF(x, 2, d, d));
    }
};

// One menu row. A plain QWidget (not a QAction) so the switch can be clicked
// without closing the menu, while a click elsewhere on the row selects.
class AccountRow : public QWidget {
public:
    AccountRow(const AccountConfig &a, SipEngine *engine, bool current, QWidget *parent)
        : QWidget(parent), id(a.id)
    {
        setObjectName(QStringLiteral("accountRow"));
        setAttribute(Qt::WA_Hover);
        setAttribute(Qt::WA_StyledBackground); // :hover background from the style sheet
        setCursor(Qt::PointingHandCursor);
        auto *h = new QHBoxLayout(this);
        h->setContentsMargins(8, 4, 8, 4);
        h->setSpacing(8);

        auto *lamp = new QLabel(this);
        lamp->setPixmap(Icons::status(a.enabled ? engine->regState(a.id) : RegState::Disabled).pixmap(16, 16));
        h->addWidget(lamp);

        auto *text = new QVBoxLayout;
        text->setSpacing(0);
        auto *title = new QLabel(a.title(), this);
        if (current) {
            QFont f = title->font();
            f.setBold(true);
            title->setFont(f);
        }
        auto *sub = new QLabel(a.user + QLatin1Char('@') + a.sipDomain()
                                   + (a.enabled ? QStringLiteral(" · ") + engine->regText(a.id) : QString()),
                               this);
        sub->setObjectName(QStringLiteral("muted"));
        QFont sf = sub->font();
        sf.setPointSizeF(sf.pointSizeF() * 0.88);
        sub->setFont(sf);
        text->addWidget(title);
        text->addWidget(sub);
        h->addLayout(text, 1);

        toggle = new ToggleSwitch(this);
        toggle->setChecked(a.enabled);
        toggle->setToolTip(a.enabled ? AccountSwitcher::tr("Turn off (unregister)")
                                     : AccountSwitcher::tr("Turn on (register)"));
        h->addWidget(toggle);
    }

    QString id;
    QAbstractButton *toggle = nullptr;
    std::function<void()> onSelect;

protected:
    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton && onSelect)
            onSelect();
    }
};

} // namespace

AccountSwitcher::AccountSwitcher(SipEngine *engine, QWidget *parent)
    : QPushButton(parent)
    , m_engine(engine)
    , m_menu(new QMenu(this))
{
    setObjectName(QStringLiteral("accountSwitcher"));
    setToolTip(tr("Account for outgoing calls. All enabled accounts receive calls."));
    setMenu(m_menu);
    connect(m_menu, &QMenu::aboutToShow, this, &AccountSwitcher::rebuildMenu);
}

void AccountSwitcher::setCurrentId(const QString &id)
{
    m_current = id;
    refresh();
}

void AccountSwitcher::refresh()
{
    const QList<AccountConfig> &accounts = Settings::instance().accounts;
    const AccountConfig *current = nullptr;
    for (const AccountConfig &a : accounts)
        if (a.id == m_current)
            current = &a;
    if (!current) {
        setIcon(Icons::status(RegState::Disabled));
        setText(accounts.isEmpty() ? tr("No accounts") : tr("Choose an account"));
        return;
    }
    setIcon(Icons::status(current->enabled ? m_engine->regState(current->id) : RegState::Disabled));
    setText(current->enabled ? current->title() : tr("%1 (off)").arg(current->title()));
}

void AccountSwitcher::rebuildMenu()
{
    m_menu->clear();
    m_menu->setMinimumWidth(qMax(width(), 260));
    const QList<AccountConfig> accounts = Settings::instance().accounts;
    for (const AccountConfig &a : accounts) {
        auto *row = new AccountRow(a, m_engine, a.id == m_current, m_menu);
        const QString id = a.id;
        row->onSelect = [this, id] {
            m_menu->close();
            if (id != m_current) {
                m_current = id;
                refresh();
                emit currentChanged(id);
            }
        };
        connect(row->toggle, &QAbstractButton::clicked, this, [this, id](bool on) {
            m_menu->close(); // enabling may ask for a password
            emit enableRequested(id, on);
        });
        auto *action = new QWidgetAction(m_menu);
        action->setDefaultWidget(row);
        m_menu->addAction(action);
    }
    if (!accounts.isEmpty())
        m_menu->addSeparator();
    m_menu->addAction(Icons::get(QStringLiteral("configure")), tr("Account settings…"), this,
                      &AccountSwitcher::settingsRequested);
}
