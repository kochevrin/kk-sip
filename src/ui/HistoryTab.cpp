#include "ui/HistoryTab.h"

#include "core/Settings.h"
#include "ui/ContactsTab.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
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

HistoryTab::HistoryTab(Database *db, QWidget *parent)
    : QWidget(parent)
    , m_db(db)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    m_list = new QListWidget(this);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setAlternatingRowColors(true);
    m_list->setToolTip(tr("Double-click to call back"));
    layout->addWidget(m_list);

    connect(m_db, &Database::historyChanged, this, &HistoryTab::reload);
    connect(m_db, &Database::contactsChanged, this, &HistoryTab::reload);
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        emit callRequested(entryAt(m_list->row(item)).number);
    });
    connect(m_list, &QListWidget::customContextMenuRequested, this, &HistoryTab::showMenu);
    reload();
}

HistoryEntry HistoryTab::entryAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row) : HistoryEntry{};
}

void HistoryTab::reload()
{
    m_entries = m_db->history();

    QHash<QString, QString> contactNames;
    const QList<Contact> contacts = m_db->contacts();
    for (const Contact &c : contacts)
        contactNames.insert(c.number, c.name);

    QHash<QString, QString> accountTitles;
    const QList<AccountConfig> &accounts = Settings::instance().accounts;
    for (const AccountConfig &a : accounts)
        accountTitles.insert(a.id, a.title());
    const bool showAccount = accounts.size() > 1;

    const QIcon inIcon = Icons::get(QStringLiteral("call-incoming"));
    const QIcon outIcon = Icons::get(QStringLiteral("call-outgoing"));
    const QIcon missedIcon = Icons::get(QStringLiteral("call-missed"));

    m_list->clear();
    for (const HistoryEntry &e : std::as_const(m_entries)) {
        QString name = contactNames.value(e.number);
        if (name.isEmpty())
            name = e.name;

        QStringList details;
        if (!name.isEmpty() && name != e.number)
            details << e.number;
        details << formatWhen(e.startedAt);
        if (e.status == HistoryEntry::Answered)
            details << formatDuration(e.duration);
        else if (e.status == HistoryEntry::Missed)
            details << tr("missed");
        else if (e.status == HistoryEntry::Declined)
            details << tr("declined");
        else
            details << tr("no answer");
        if (showAccount && accountTitles.contains(e.accountId))
            details << accountTitles.value(e.accountId);

        auto *item = new QListWidgetItem(
            (name.isEmpty() ? e.number : name) + QLatin1Char('\n') + details.join(QStringLiteral(" · ")));
        item->setIcon(e.status == HistoryEntry::Missed ? missedIcon : e.incoming ? inIcon : outIcon);
        if (e.status == HistoryEntry::Missed)
            item->setForeground(Theme::colors().danger);
        m_list->addItem(item);
    }
}

void HistoryTab::showMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_list->itemAt(pos);
    QMenu menu(this);
    if (item) {
        const HistoryEntry e = entryAt(m_list->row(item));
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
        menu.addAction(Icons::get(QStringLiteral("edit-delete")), tr("Delete"), this,
                       [this, e] { m_db->removeHistory(e.id); });
    }
    QAction *clear = menu.addAction(Icons::get(QStringLiteral("edit-clear-history")), tr("Clear history"), this, [this] {
        if (QMessageBox::question(this, tr("Clear history"), tr("Delete all call history?")) == QMessageBox::Yes)
            m_db->clearHistory();
    });
    clear->setEnabled(m_list->count() > 0);
    menu.exec(m_list->viewport()->mapToGlobal(pos));
}
