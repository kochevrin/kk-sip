#pragma once

#include "core/Database.h"

#include <QSet>
#include <QWidget>

class QLineEdit;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

// Call history. Two views: grouped (one row per number, its older calls folded under
// it, click the row to unfold) and a flat list of every call. A search box filters
// both by number or name.
class HistoryTab : public QWidget {
    Q_OBJECT
public:
    explicit HistoryTab(Database *db, QWidget *parent = nullptr);

    void reload();
    void setGrouped(bool grouped);

signals:
    void callRequested(const QString &number);

private:
    void showMenu(const QPoint &pos);
    HistoryEntry entryOf(const QTreeWidgetItem *item) const;
    void fillItem(QTreeWidgetItem *item, int entryIndex, bool child);

    Database *m_db;
    QLineEdit *m_search;
    QToolButton *m_groupedButton;
    QToolButton *m_listButton;
    QTreeWidget *m_tree;
    QList<HistoryEntry> m_entries;
    QHash<QString, QString> m_contactNames;
    QHash<QString, QString> m_accountTitles;
    QSet<QString> m_expanded; // numbers unfolded in the grouped view
};
