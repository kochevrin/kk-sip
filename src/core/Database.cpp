#include "core/Database.h"

#include "core/Settings.h"

#include <QDebug>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

Database::Database(QObject *parent)
    : QObject(parent)
{
}

bool Database::open(QString *error)
{
    const QString dir = Settings::instance().dataDir();
    QDir().mkpath(dir);
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(dir + QStringLiteral("/kk-sip.db"));
    if (!db.open()) {
        if (error)
            *error = db.lastError().text();
        return false;
    }

    QSqlQuery q;
    q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    const bool ok = q.exec(QStringLiteral(
                        "CREATE TABLE IF NOT EXISTS calls ("
                        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                        " account_id TEXT, incoming INTEGER, status INTEGER,"
                        " number TEXT, name TEXT, started_at INTEGER, duration INTEGER)"))
        && q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS contacts ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, number TEXT)"))
        && q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS calls_started ON calls(started_at)"));
    if (!ok) {
        if (error)
            *error = q.lastError().text();
        return false;
    }

    // Schema migrations, tracked in PRAGMA user_version.
    q.exec(QStringLiteral("PRAGMA user_version"));
    const int version = q.next() ? q.value(0).toInt() : 0;
    if (version < 1) {
        const bool migrated = q.exec(QStringLiteral("ALTER TABLE contacts ADD COLUMN blf INTEGER NOT NULL DEFAULT 0"))
            && q.exec(QStringLiteral("ALTER TABLE contacts ADD COLUMN blf_account TEXT NOT NULL DEFAULT ''"))
            && q.exec(QStringLiteral("PRAGMA user_version = 1"));
        if (!migrated) {
            if (error)
                *error = q.lastError().text();
            return false;
        }
    }
    return true;
}

void Database::addHistory(const HistoryEntry &e)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("INSERT INTO calls (account_id, incoming, status, number, name, started_at, duration)"
                             " VALUES (?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(e.accountId);
    q.addBindValue(e.incoming ? 1 : 0);
    q.addBindValue(int(e.status));
    q.addBindValue(e.number);
    q.addBindValue(e.name);
    q.addBindValue(e.startedAt.toSecsSinceEpoch());
    q.addBindValue(e.duration);
    q.exec();
    emit historyChanged();
}

QList<HistoryEntry> Database::history(int limit) const
{
    QList<HistoryEntry> out;
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT id, account_id, incoming, status, number, name, started_at, duration"
                             " FROM calls ORDER BY started_at DESC, id DESC LIMIT ?"));
    q.addBindValue(limit);
    q.exec();
    while (q.next()) {
        HistoryEntry e;
        e.id = q.value(0).toLongLong();
        e.accountId = q.value(1).toString();
        e.incoming = q.value(2).toInt() != 0;
        e.status = HistoryEntry::Status(q.value(3).toInt());
        e.number = q.value(4).toString();
        e.name = q.value(5).toString();
        e.startedAt = QDateTime::fromSecsSinceEpoch(q.value(6).toLongLong());
        e.duration = q.value(7).toInt();
        out.append(e);
    }
    return out;
}

void Database::removeHistory(qint64 id)
{
    removeHistory(QList<qint64>{id});
}

void Database::removeHistory(const QList<qint64> &ids)
{
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM calls WHERE id = ?"));
    for (qint64 id : ids) {
        q.addBindValue(id);
        q.exec();
    }
    db.commit();
    emit historyChanged();
}

void Database::clearHistory()
{
    QSqlQuery q;
    q.exec(QStringLiteral("DELETE FROM calls"));
    emit historyChanged();
}

QList<Contact> Database::contacts() const
{
    QList<Contact> out;
    QSqlQuery q(QStringLiteral("SELECT id, name, number, blf, blf_account FROM contacts ORDER BY name COLLATE NOCASE"));
    while (q.next())
        out.append({q.value(0).toLongLong(), q.value(1).toString(), q.value(2).toString(), q.value(3).toInt() != 0,
                    q.value(4).toString()});
    return out;
}

qint64 Database::saveContact(const Contact &c)
{
    QSqlQuery q;
    qint64 id = c.id;
    if (id == 0) {
        q.prepare(QStringLiteral("INSERT INTO contacts (name, number, blf, blf_account) VALUES (?, ?, ?, ?)"));
        q.addBindValue(c.name);
        q.addBindValue(c.number);
        q.addBindValue(c.blf ? 1 : 0);
        q.addBindValue(c.blfAccount.isEmpty() ? QStringLiteral("") : c.blfAccount); // null QString binds as NULL
        if (!q.exec())
            qWarning() << "save contact:" << q.lastError().text();
        id = q.lastInsertId().toLongLong();
    } else {
        q.prepare(QStringLiteral("UPDATE contacts SET name = ?, number = ?, blf = ?, blf_account = ? WHERE id = ?"));
        q.addBindValue(c.name);
        q.addBindValue(c.number);
        q.addBindValue(c.blf ? 1 : 0);
        q.addBindValue(c.blfAccount.isEmpty() ? QStringLiteral("") : c.blfAccount); // null QString binds as NULL
        q.addBindValue(c.id);
        if (!q.exec())
            qWarning() << "save contact:" << q.lastError().text();
    }
    emit contactsChanged();
    return id;
}

void Database::removeContact(qint64 id)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("DELETE FROM contacts WHERE id = ?"));
    q.addBindValue(id);
    q.exec();
    emit contactsChanged();
}

QString Database::nameForNumber(const QString &number) const
{
    if (number.isEmpty())
        return {};
    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT name FROM contacts WHERE number = ? LIMIT 1"));
    q.addBindValue(number);
    q.exec();
    return q.next() ? q.value(0).toString() : QString();
}
