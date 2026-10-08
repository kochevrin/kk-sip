#pragma once

#include "core/Database.h"

#include <QWidget>

class QListWidget;

class HistoryTab : public QWidget {
    Q_OBJECT
public:
    explicit HistoryTab(Database *db, QWidget *parent = nullptr);

    void reload();

signals:
    void callRequested(const QString &number);

private:
    void showMenu(const QPoint &pos);
    HistoryEntry entryAt(int row) const;

    Database *m_db;
    QListWidget *m_list;
    QList<HistoryEntry> m_entries;
};
