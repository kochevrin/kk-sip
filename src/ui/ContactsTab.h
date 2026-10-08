#pragma once

#include "core/Database.h"

#include <QWidget>

class QLineEdit;
class QListWidget;

// Opens the add/edit contact dialog. Returns false when cancelled.
bool editContactDialog(QWidget *parent, Contact &contact);

class ContactsTab : public QWidget {
    Q_OBJECT
public:
    explicit ContactsTab(Database *db, QWidget *parent = nullptr);

signals:
    void callRequested(const QString &number);

private:
    void reload();
    void addContact();
    void editContact(qint64 id);
    void showMenu(const QPoint &pos);
    void importFile();
    void exportCsv();
    Contact contactAt(int row) const;

    Database *m_db;
    QLineEdit *m_search;
    QListWidget *m_list;
    QList<Contact> m_contacts;
};
