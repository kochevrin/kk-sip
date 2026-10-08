#pragma once

#include "core/Database.h"

#include <QWidget>

class Database;
class QLineEdit;
class QPushButton;

class DialerTab : public QWidget {
    Q_OBJECT
public:
    explicit DialerTab(Database *db, QWidget *parent = nullptr);

    QString number() const;
    void setNumber(const QString &number);
    void appendDigit(const QString &digit);
    void setLastDialed(const QString &number) { m_lastDialed = number; }
    void focusNumber();

signals:
    void callRequested(const QString &number);
    void keyPressed(const QString &key); // keypad button; MainWindow decides digit vs DTMF

private:
    void updateSuggestion();
    void requestCall();

    Database *m_db;
    QLineEdit *m_number;
    QPushButton *m_callButton;
    QPushButton *m_suggestion;   // one-line "Name — number" hint under the field
    QString m_suggestedNumber;
    QList<Contact> m_contacts;
    QString m_lastDialed;
};
