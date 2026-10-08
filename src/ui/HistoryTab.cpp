#include "ui/HistoryTab.h"

#include "core/Settings.h"
#include "core/SipUri.h"
#include "ui/ContactsTab.h"
#include "ui/Icons.h"
#include "ui/ListDelegate.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

static QString formatWhen(const QDateTime &dt)
{
    const QDate today = QDate::currentDate();
    if (dt.date() == today)
        return dt.toString(QStringLiteral("HH:mm"));
    if (dt.date() == today.addDays(-1))
        return HistoryTab::tr("yesterday %1").arg(dt.toString(QStringLiteral("HH:mm")));
    if (dt.date().year() == today.year())
        return dt.toString(QStringLiteral("dd.MM HH:mm"));
    return dt.toString(QStringLiteral("dd.MM.yyyy HH:mm"));
}

static QString formatDuration(int secs)
{
    if (secs >= 3600)
        return QStringLiteral("%1:%2:%3").arg(secs / 3600).arg(secs / 60 % 60, 2, 10, QLatin1Char('0'))
            .arg(secs % 60, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(secs / 60).arg(secs % 60, 2, 10, QLatin1Char('0'));
}

static constexpr int EntryRole = Qt::UserRole + 1; // index into m_entries

HistoryTab::HistoryTab(Database *db, QWidget *parent)
    : QWidget(parent)
    , m_db(db)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *top = new QHBoxLayout;
    top->setSpacing(4);
    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("historySearch"));
    m_search->setPlaceholderText(tr("Search number or name"));
    m_search->setClearButtonEnabled(true);
    top->addWidget(m_search, 1);

    auto makeModeButton = [this](const QIcon &icon, const QString &tip) {
        auto *b = new QToolButton(this);
        b->setIcon(icon);
        b->setCheckable(true);
        b->setAutoRaise(false);
        b->setToolTip(tip);
        return b;
    };
    m_groupedButton = makeModeButton(Icons::grouped(), tr("Group by number: one row per number, click it to see all its calls"));
    m_listButton = makeModeButton(Icons::flatList(), tr("All calls in one list"));
    m_groupedButton->setObjectName(QStringLiteral("historyGrouped"));
    m_listButton->setObjectName(QStringLiteral("historyList"));
    auto *modes = new QButtonGroup(this);
    modes->addButton(m_groupedButton);
    modes->addButton(m_listButton);
    (Settings::instance().historyGrouped ? m_groupedButton : m_listButton)->setChecked(true);
    top->addWidget(m_groupedButton);
    top->addWidget(m_listButton);
    layout->addLayout(top);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(false); // the call-count badge shows what unfolds
    m_tree->setIndentation(18);
    m_tree->setExpandsOnDoubleClick(false); // double-click calls back
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setAlternatingRowColors(true);
    m_tree->setToolTip(tr("Double-click to call back"));
    m_tree->setItemDelegate(new ListDelegate(m_tree));
    layout->addWidget(m_tree, 1);

    connect(Theme::Notifier::instance(), &Theme::Notifier::changed, this, [this] {
        m_groupedButton->setIcon(Icons::grouped());
        m_listButton->setIcon(Icons::flatList());
    });
    connect(m_search, &QLineEdit::textChanged, this, &HistoryTab::reload);
    connect(m_groupedButton, &QToolButton::toggled, this, [this](bool on) { setGrouped(on); });
    connect(m_db, &Database::historyChanged, this, &HistoryTab::reload);
    connect(m_db, &Database::contactsChanged, this, &HistoryTab::reload);
    connect(m_tree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        emit callRequested(entryOf(item).number);
    });
    connect(m_tree, &QTreeWidget::itemClicked, this, [](QTreeWidgetItem *item) {
        if (item->childCount() > 0)
            item->setExpanded(!item->isExpanded());
    });
    auto remember = [this](QTreeWidgetItem *item) {
        const QString number = entryOf(item).number;
        if (item->isExpanded())
            m_expanded.insert(number);
        else
            m_expanded.remove(number);
        fillItem(item, item->data(0, EntryRole).toInt(), false); // badge arrow
    };
    connect(m_tree, &QTreeWidget::itemExpanded, this, remember);
    connect(m_tree, &QTreeWidget::itemCollapsed, this, remember);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &HistoryTab::showMenu);
    reload();
}

void HistoryTab::setGrouped(bool grouped)
{
    (grouped ? m_groupedButton : m_listButton)->setChecked(true);
    Settings &s = Settings::instance();
    if (s.historyGrouped == grouped)
        return;
    s.historyGrouped = grouped;
    s.save();
    reload();
}

HistoryEntry HistoryTab::entryOf(const QTreeWidgetItem *item) const
{
    const int i = item ? item->data(0, EntryRole).toInt() : -1;
    return i >= 0 && i < m_entries.size() ? m_entries.at(i) : HistoryEntry{};
}

// Grouped-view children skip the name (the parent row has it) and lead with the time.
void HistoryTab::fillItem(QTreeWidgetItem *item, int entryIndex, bool child)
{
    const HistoryEntry &e = m_entries.at(entryIndex);
    QString name = m_contactNames.value(e.number);
    if (name.isEmpty())
        name = e.name;

    QString outcome;
    if (e.status == HistoryEntry::Answered)
        outcome = formatDuration(e.duration);
    else if (e.status == HistoryEntry::Missed)
        outcome = tr("missed");
    else if (e.status == HistoryEntry::Declined)
        outcome = tr("declined");
    else
        outcome = tr("no answer");
    const QString account = m_accountTitles.size() > 1 ? m_accountTitles.value(e.accountId) : QString();

    QString title;
    QStringList details;
    if (child) {
        title = formatWhen(e.startedAt);
        details << (e.incoming ? tr("incoming") : tr("outgoing")) << outcome;
    } else {
        title = name.isEmpty() ? e.number : name;
        if (!name.isEmpty() && name != e.number)
            details << e.number;
        details << formatWhen(e.startedAt) << outcome;
    }
    if (!account.isEmpty())
        details << account;

    item->setData(0, EntryRole, entryIndex);
    item->setText(0, title);
    item->setData(0, ListDelegate::SubtitleRole, details.join(QStringLiteral(" · ")));
    // Own arrows, not the icon theme: same look on every desktop and on Windows.
    item->setIcon(0, QIcon(e.status == HistoryEntry::Missed ? QStringLiteral(":/icons/call-missed.svg")
                           : e.incoming                     ? QStringLiteral(":/icons/call-in.svg")
                                                            : QStringLiteral(":/icons/call-out.svg")));
    item->setData(0, Qt::ForegroundRole,
                  e.status == HistoryEntry::Missed ? QVariant(QBrush(Theme::colors().danger)) : QVariant());
    if (item->childCount() > 0)
        item->setData(0, ListDelegate::BadgeRole,
                      QStringLiteral("%1 %2").arg(item->childCount() + 1).arg(item->isExpanded() ? QChar(0x25B4)
                                                                                                 : QChar(0x25BE)));
}

void HistoryTab::reload()
{
    m_entries = m_db->history();

    m_contactNames.clear();
    const QList<Contact> contacts = m_db->contacts();
    for (const Contact &c : contacts)
        m_contactNames.insert(c.number, c.name);

    m_accountTitles.clear();
    const QList<AccountConfig> &accounts = Settings::instance().accounts;
    for (const AccountConfig &a : accounts)
        m_accountTitles.insert(a.id, a.title());

    // Typed "067 123-45" should find "+38067123..." too.
    const QString query = m_search->text().trimmed();
    const QString digits = SipUri::cleanNumber(query);
    auto matches = [&](const HistoryEntry &e) {
        if (query.isEmpty())
            return true;
        if (!digits.isEmpty() && SipUri::cleanNumber(e.number).contains(digits, Qt::CaseInsensitive))
            return true;
        const QString name = m_contactNames.value(e.number, e.name);
        return name.contains(query, Qt::CaseInsensitive) || e.name.contains(query, Qt::CaseInsensitive);
    };

    const bool grouped = Settings::instance().historyGrouped;
    m_tree->clear();
    QHash<QString, QTreeWidgetItem *> groups;
    QList<QTreeWidgetItem *> top;
    for (int i = 0; i < m_entries.size(); ++i) { // newest first
        const HistoryEntry &e = m_entries.at(i);
        if (!matches(e))
            continue;
        QTreeWidgetItem *group = grouped ? groups.value(e.number) : nullptr;
        if (group) {
            auto *child = new QTreeWidgetItem(group);
            fillItem(child, i, true);
            continue;
        }
        auto *item = new QTreeWidgetItem;
        item->setData(0, EntryRole, i);
        top << item;
        if (grouped)
            groups.insert(e.number, item);
    }
    m_tree->addTopLevelItems(top);

    // One number left after a search: show all its calls straight away.
    const bool single = grouped && top.size() == 1 && !query.isEmpty();
    for (QTreeWidgetItem *item : std::as_const(top)) {
        const int i = item->data(0, EntryRole).toInt();
        if (item->childCount() > 0 && (single || m_expanded.contains(m_entries.at(i).number))) {
            const QSignalBlocker block(m_tree);
            item->setExpanded(true);
        }
        fillItem(item, i, false);
    }
}

void HistoryTab::showMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    QMenu menu(this);
    if (item) {
        const HistoryEntry e = entryOf(item);
        menu.addAction(Icons::get(QStringLiteral("call-start"), QStringLiteral("call.svg")), tr("Call"), this,
                       [this, e] { emit callRequested(e.number); });
        menu.addAction(Icons::get(QStringLiteral("contact-new")), tr("Add to contacts…"), this, [this, e] {
            Contact c{0, e.name, e.number};
            if (editContactDialog(this, c))
                m_db->saveContact(c);
        });
        menu.addAction(Icons::get(QStringLiteral("edit-copy")), tr("Copy number"), this,
                       [e] { QApplication::clipboard()->setText(e.number); });
        menu.addSeparator();
        if (item->childCount() > 0) {
            QList<qint64> ids{e.id};
            for (int i = 0; i < item->childCount(); ++i)
                ids << entryOf(item->child(i)).id;
            menu.addAction(Icons::get(QStringLiteral("edit-delete")), tr("Delete all %n call(s) with this number", nullptr, int(ids.size())),
                           this, [this, ids] { m_db->removeHistory(ids); });
        } else {
            menu.addAction(Icons::get(QStringLiteral("edit-delete")), tr("Delete"), this,
                           [this, e] { m_db->removeHistory(e.id); });
        }
    }
    QAction *clear = menu.addAction(Icons::get(QStringLiteral("edit-clear-history")), tr("Clear history"), this, [this] {
        if (QMessageBox::question(this, tr("Clear history"), tr("Delete all call history?")) == QMessageBox::Yes)
            m_db->clearHistory();
    });
    clear->setEnabled(!m_entries.isEmpty());
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}
