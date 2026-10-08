#pragma once

#include "core/Database.h"

#include <QWidget>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QTimer;
class SipEngine;
struct BlfInfo;

// Opens the add/edit contact dialog. Returns false when cancelled.
bool editContactDialog(QWidget *parent, Contact &contact);

class ContactsTab : public QWidget {
    Q_OBJECT
public:
    ContactsTab(Database *db, SipEngine *engine, QWidget *parent = nullptr);

    static QString blfText(const BlfInfo &info);

signals:
    void callRequested(const QString &number);

private:
    void reload();
    void refreshLamps();
    void updateLamp(QListWidgetItem *item, const Contact &c);
    void addContact();
    void editContact(qint64 id);
    void showMenu(const QPoint &pos);
    void importFile();
    void exportCsv();
    Contact contactAt(int row) const;

    Database *m_db;
    SipEngine *m_engine;
    QTimer *m_blink = nullptr;
    bool m_blinkOn = true;
    bool m_anyLamp = false;
    QLineEdit *m_search;
    QListWidget *m_list;
    QList<Contact> m_contacts;
};
