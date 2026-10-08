#pragma once

#include "sip/SipEngine.h"

#include <QFrame>
#include <QHash>
#include <functional>

class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;
class QVBoxLayout;

// Strip above the tabs listing current calls with their controls.
class CallPanel : public QFrame {
    Q_OBJECT
public:
    using NameLookup = std::function<QString(const QString &number)>;

    CallPanel(SipEngine *engine, NameLookup lookup, QWidget *parent = nullptr);

    void refresh();
    static QString stateText(const CallView &call);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Row {
        QWidget *widget = nullptr;
        QLabel *title = nullptr;
        QLabel *status = nullptr;
        QPushButton *answer = nullptr;
        QPushButton *mute = nullptr;
        QPushButton *hold = nullptr;
        QPushButton *transfer = nullptr;
        QPushButton *hangup = nullptr;
        QWidget *transferBox = nullptr;   // inline "transfer to" field, shown on request
        QLineEdit *transferEdit = nullptr;
        CallView::State shownState = CallView::Ended;
        qint64 armedAt = 0;               // when the current set of buttons appeared
    };

    Row *createRow(int callId);
    void updateRow(Row *row, const CallView &call);
    void doTransfer(int callId, Row *row);
    static bool justAppeared(const Row *row);

    SipEngine *m_engine;
    NameLookup m_lookup;
    QVBoxLayout *m_layout;
    QHash<int, Row *> m_rows;
    QTimer *m_clock;
};
