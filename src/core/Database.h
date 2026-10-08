#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

struct HistoryEntry {
    enum Status { Answered = 0, Missed = 1, Failed = 2, Declined = 3 };

    qint64 id = 0;
    QString accountId;
    bool incoming = false;
    Status status = Answered;
    QString number;
    QString name;
    QDateTime startedAt;
    int duration = 0; // seconds
};

struct Contact {
    qint64 id = 0;
    QString name;
    QString number;
};

// SQLite store for call history and the phone book (~/.local/share/kk-sip/kk-sip.db).
class Database : public QObject {
    Q_OBJECT
public:
    explicit Database(QObject *parent = nullptr);

    bool open(QString *error);

    void addHistory(const HistoryEntry &e);
    QList<HistoryEntry> history(int limit = 500) const;
    void removeHistory(qint64 id);
    void clearHistory();

    QList<Contact> contacts() const;
    qint64 saveContact(const Contact &c); // insert when id == 0
    void removeContact(qint64 id);
    QString nameForNumber(const QString &number) const;

signals:
    void historyChanged();
    void contactsChanged();
};
