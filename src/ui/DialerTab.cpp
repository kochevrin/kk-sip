#include "ui/DialerTab.h"

#include "core/Database.h"
#include "core/SipUri.h"
#include "ui/Icons.h"

#include <QGridLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QToolButton>
#include <QVBoxLayout>

DialerTab::DialerTab(Database *db, QWidget *parent)
    : QWidget(parent)
    , m_db(db)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *numberRow = new QHBoxLayout;
    m_number = new QLineEdit(this);
    m_number->setPlaceholderText(tr("Number or name"));
    QFont f = m_number->font();
    f.setPointSizeF(f.pointSizeF() * 1.4);
    m_number->setFont(f);
    m_number->setMinimumHeight(36);
    numberRow->addWidget(m_number, 1);

    auto *backspace = new QToolButton(this);
    backspace->setIcon(Icons::get(QStringLiteral("edit-clear-locationbar-rtl")));
    backspace->setText(QStringLiteral("⌫"));
    backspace->setToolTip(tr("Delete last digit (hold to clear)"));
    backspace->setAutoRepeat(true);
    backspace->setMinimumHeight(36);
    connect(backspace, &QToolButton::clicked, this, [this] { m_number->backspace(); });
    numberRow->addWidget(backspace);
    layout->addLayout(numberRow);

    // A single suggestion line instead of a completer popup: a popup covers the keypad.
    m_suggestion = new QPushButton(this);
    m_suggestion->setFlat(true);
    m_suggestion->setFocusPolicy(Qt::NoFocus);
    m_suggestion->setCursor(Qt::PointingHandCursor);
    m_suggestion->setStyleSheet(QStringLiteral("QPushButton { text-align: left; padding: 0 4px; }"));
    m_suggestion->setToolTip(tr("Click to use this number"));
    m_suggestion->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    // Keep the slot even when empty so the keypad doesn't jump while typing.
    QSizePolicy keep = m_suggestion->sizePolicy();
    keep.setRetainSizeWhenHidden(true);
    m_suggestion->setSizePolicy(keep);
    m_suggestion->hide();
    layout->addWidget(m_suggestion);
    connect(m_suggestion, &QPushButton::clicked, this, [this] {
        if (!m_suggestedNumber.isEmpty())
            m_number->setText(m_suggestedNumber);
        m_number->setFocus();
    });
    connect(m_number, &QLineEdit::textChanged, this, &DialerTab::updateSuggestion);
    auto reload = [this] {
        m_contacts = m_db->contacts();
        updateSuggestion();
    };
    reload();
    connect(m_db, &Database::contactsChanged, this, reload);

    static const char *keys[12][2] = {
        {"1", ""}, {"2", "ABC"}, {"3", "DEF"},
        {"4", "GHI"}, {"5", "JKL"}, {"6", "MNO"},
        {"7", "PQRS"}, {"8", "TUV"}, {"9", "WXYZ"},
        {"*", ""}, {"0", "+"}, {"#", ""},
    };
    auto *grid = new QGridLayout;
    grid->setSpacing(6);
    for (int i = 0; i < 12; ++i) {
        const QString key = QString::fromLatin1(keys[i][0]);
        const QString sub = QString::fromLatin1(keys[i][1]);
        auto *b = new QPushButton(sub.isEmpty() ? key : key + QLatin1Char('\n') + sub, this);
        b->setFocusPolicy(Qt::NoFocus);
        b->setMinimumHeight(46);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QFont bf = b->font();
        bf.setPointSizeF(bf.pointSizeF() * (sub.isEmpty() ? 1.3 : 1.0));
        b->setFont(bf);
        connect(b, &QPushButton::clicked, this, [this, key] { emit keyPressed(key); });
        grid->addWidget(b, i / 3, i % 3);
    }
    layout->addLayout(grid, 1);

    m_callButton = new QPushButton(Icons::callWhite(), tr("Call"), this);
    m_callButton->setMinimumHeight(42);
    m_callButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: #2eb84b; color: white; font-weight: bold; border-radius: 6px; }"
        "QPushButton:hover { background: #29a744; }"
        "QPushButton:pressed { background: #23903a; }"));
    connect(m_callButton, &QPushButton::clicked, this, &DialerTab::requestCall);
    connect(m_number, &QLineEdit::returnPressed, this, &DialerTab::requestCall);
    layout->addWidget(m_callButton);
}

QString DialerTab::number() const
{
    return m_number->text().trimmed();
}

void DialerTab::setNumber(const QString &number)
{
    m_number->setText(number);
}

void DialerTab::appendDigit(const QString &digit)
{
    m_number->insert(digit);
}

void DialerTab::focusNumber()
{
    m_number->setFocus();
    m_number->selectAll();
}

void DialerTab::requestCall()
{
    QString n = number();
    if (n.isEmpty()) {
        // Like a desk phone: Call on an empty field brings back the last number.
        m_number->setText(m_lastDialed);
        return;
    }
    // Typed a contact name instead of a number: use the suggested contact.
    static const QRegularExpression dialable(QStringLiteral("^[0-9+*#\\s\\-().]+$|@|^(sips?|tel):"));
    if (!dialable.match(n).hasMatch() && !m_suggestedNumber.isEmpty())
        n = m_suggestedNumber;
    emit callRequested(n);
}

void DialerTab::updateSuggestion()
{
    const QString text = m_number->text().trimmed();
    m_suggestedNumber.clear();
    if (text.size() >= 2) {
        const QString digits = SipUri::cleanNumber(text);
        // Number prefix beats a name match; exact number means nothing to suggest.
        for (int pass = 0; pass < 2 && m_suggestedNumber.isEmpty(); ++pass) {
            for (const Contact &c : std::as_const(m_contacts)) {
                const bool hit = pass == 0 ? c.number.startsWith(digits) && c.number != digits
                                           : c.name.contains(text, Qt::CaseInsensitive);
                if (hit) {
                    m_suggestedNumber = c.number;
                    const QFontMetrics fm(m_suggestion->font());
                    m_suggestion->setText(fm.elidedText(c.name + QStringLiteral(" — ") + c.number, Qt::ElideRight,
                                                        qMax(80, m_suggestion->width() - 12)));
                    break;
                }
            }
        }
    }
    m_suggestion->setVisible(!m_suggestedNumber.isEmpty());
}
