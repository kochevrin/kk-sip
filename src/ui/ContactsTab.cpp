#include "ui/ContactsTab.h"

#include "core/MicrosipImport.h"
#include "core/Settings.h"
#include "core/SipUri.h"
#include "ui/Icons.h"
#include "ui/ListDelegate.h"
#include "ui/Theme.h"
#include "sip/SipEngine.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTextStream>
#include <QRegularExpression>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

static constexpr int IdRole = Qt::UserRole + 1;

bool editContactDialog(QWidget *parent, Contact &contact)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(contact.id ? QObject::tr("Edit contact") : QObject::tr("New contact"));
    auto *form = new QFormLayout(&dlg);
    auto *name = new QLineEdit(contact.name, &dlg);
    auto *number = new QLineEdit(contact.number, &dlg);
    form->addRow(QObject::tr("Name:"), name);
    form->addRow(QObject::tr("Number:"), number);
    auto *blf = new QCheckBox(QObject::tr("Show busy lamp (BLF)"), &dlg);
    blf->setToolTip(QObject::tr("Green: free, orange: ringing, red: on a call. Works for extensions on your PBX."));
    blf->setChecked(contact.blf);
    auto *blfAccount = new QComboBox(&dlg);
    blfAccount->addItem(QObject::tr("Selected account"), QString());
    for (const AccountConfig &a : Settings::instance().accounts)
        blfAccount->addItem(a.title(), a.id);
    blfAccount->setCurrentIndex(qMax(0, blfAccount->findData(contact.blfAccount)));
    form->addRow(QString(), blf);
    form->addRow(QObject::tr("Watch via:"), blfAccount);
    // Which account subscribes only matters with a lamp, and only with several accounts;
    // a greyed-out combo here looked broken.
    const bool severalAccounts = Settings::instance().accounts.size() > 1;
    auto showWatchVia = [&dlg, form, blfAccount, severalAccounts](bool lamp) {
        form->setRowVisible(blfAccount, lamp && severalAccounts);
        dlg.adjustSize();
    };
    QObject::connect(blf, &QCheckBox::toggled, &dlg, showWatchVia);
    Theme::tidyForm(form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    auto validate = [&] {
        buttons->button(QDialogButtonBox::Save)->setEnabled(!number->text().trimmed().isEmpty());
    };
    QObject::connect(number, &QLineEdit::textChanged, &dlg, validate);
    validate();
    (contact.name.isEmpty() ? name : number)->setFocus();
    showWatchVia(contact.blf);
    dlg.resize(qMax(320, dlg.sizeHint().width()), dlg.sizeHint().height());

    if (dlg.exec() != QDialog::Accepted)
        return false;
    contact.number = SipUri::cleanNumber(number->text());
    contact.name = name->text().trimmed();
    contact.blf = blf->isChecked();
    contact.blfAccount = blfAccount->currentData().toString();
    if (contact.name.isEmpty())
        contact.name = contact.number;
    return true;
}

ContactsTab::ContactsTab(Database *db, SipEngine *engine, QWidget *parent)
    : QWidget(parent)
    , m_db(db)
    , m_engine(engine)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *top = new QHBoxLayout;
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search"));
    m_search->setClearButtonEnabled(true);
    top->addWidget(m_search, 1);

    auto *add = new QToolButton(this);
    Icons::setThemed(add, QStringLiteral("plus"));
    add->setToolTip(tr("Add contact"));
    add->setAccessibleName(tr("Add contact"));
    connect(add, &QToolButton::clicked, this, &ContactsTab::addContact);
    top->addWidget(add);

    auto *more = new QToolButton(this);
    Icons::setThemed(more, QStringLiteral("more"));
    more->setToolTip(tr("Import, export, busy lamps"));
    more->setAccessibleName(more->toolTip());
    more->setPopupMode(QToolButton::InstantPopup);
    auto *moreMenu = new QMenu(more);
    moreMenu->addAction(Icons::get(QStringLiteral("document-import")), tr("Import (CSV or MicroSIP Contacts.xml)…"),
                        this, &ContactsTab::importFile);
    moreMenu->addAction(Icons::get(QStringLiteral("document-export")), tr("Export to CSV…"), this,
                        &ContactsTab::exportCsv);
    moreMenu->addSeparator();
    moreMenu->addAction(tr("Busy lamps for all internal numbers"), this, [this] { setLampsForInternal(true); });
    moreMenu->addAction(tr("Turn all busy lamps off"), this, [this] { setLampsForInternal(false); });
    more->setMenu(moreMenu);
    top->addWidget(more);
    layout->addLayout(top);

    m_list = new QListWidget(this);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setItemDelegate(new ListDelegate(m_list));
    m_list->setUniformItemSizes(false);

    m_blink = new QTimer(this);
    m_blink->setInterval(500);
    connect(m_blink, &QTimer::timeout, this, [this] {
        m_blinkOn = !m_blinkOn;
        refreshLamps();
    });
    connect(m_engine, &SipEngine::blfChanged, this, [this] { refreshLamps(); });
    connect(Theme::Notifier::instance(), &Theme::Notifier::changed, this, &ContactsTab::refreshLamps);
    layout->addWidget(m_list, 1);

    connect(m_search, &QLineEdit::textChanged, this, &ContactsTab::reload);
    connect(m_db, &Database::contactsChanged, this, &ContactsTab::reload);
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        emit callRequested(contactAt(m_list->row(item)).number);
    });
    connect(m_list, &QListWidget::customContextMenuRequested, this, &ContactsTab::showMenu);
    reload();
}

Contact ContactsTab::contactAt(int row) const
{
    QListWidgetItem *item = m_list->item(row);
    if (!item)
        return {};
    const qint64 id = item->data(IdRole).toLongLong();
    for (const Contact &c : m_contacts)
        if (c.id == id)
            return c;
    return {};
}

QString ContactsTab::blfText(const BlfInfo &info)
{
    switch (info.state) {
    case BlfState::Idle:
        return tr("free");
    case BlfState::Ringing:
        return info.peer.isEmpty() ? tr("ringing") : tr("ringing: %1").arg(info.peer);
    case BlfState::Busy:
        return info.peer.isEmpty() ? tr("on a call") : tr("on a call with %1").arg(info.peer);
    case BlfState::Unknown:
        break;
    }
    return tr("status unknown");
}

void ContactsTab::updateLamp(QListWidgetItem *item, const Contact &c)
{
    item->setData(ListDelegate::HasLampSlotRole, m_anyLamp);
    if (!c.blf) {
        item->setData(ListDelegate::LampRole, QVariant());
        item->setData(ListDelegate::SubtitleRole, c.name == c.number ? QString() : c.number);
        return;
    }
    const BlfInfo info = m_engine->blf(c.id);
    const Theme::Colors &tc = Theme::colors();
    QColor lamp = tc.muted;
    if (info.state == BlfState::Idle)
        lamp = tc.ok;
    else if (info.state == BlfState::Busy)
        lamp = tc.danger;
    else if (info.state == BlfState::Ringing)
        lamp = m_blinkOn ? tc.warn : tc.warn.darker(170);
    item->setData(ListDelegate::LampRole, lamp);
    const QString number = c.name == c.number ? QString() : c.number + QStringLiteral(" · ");
    item->setData(ListDelegate::SubtitleRole, number + blfText(info));
    item->setToolTip(c.number + QStringLiteral(" — ") + blfText(info));
}

void ContactsTab::refreshLamps()
{
    bool ringing = false;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        const Contact c = contactAt(i);
        updateLamp(item, c);
        ringing |= c.blf && m_engine->blf(c.id).state == BlfState::Ringing;
    }
    // A ringing colleague blinks, like the lamp on a desk phone.
    if (ringing && !m_blink->isActive())
        m_blink->start();
    else if (!ringing)
        m_blink->stop();
}

void ContactsTab::reload()
{
    m_contacts = m_db->contacts();
    m_anyLamp = std::any_of(m_contacts.cbegin(), m_contacts.cend(), [](const Contact &c) { return c.blf; });
    const QString filter = m_search->text().trimmed();
    m_list->clear();
    for (const Contact &c : std::as_const(m_contacts)) {
        if (!filter.isEmpty() && !c.name.contains(filter, Qt::CaseInsensitive)
            && !c.number.contains(filter))
            continue;
        auto *item = new QListWidgetItem(c.name.isEmpty() ? c.number : c.name);
        item->setData(IdRole, c.id);
        m_list->addItem(item);
    }
    refreshLamps();
}

void ContactsTab::setLampsForInternal(bool on)
{
    // Internal = short extension numbers; city and mobile numbers have no lamp on a PBX.
    static const QRegularExpression internal(QStringLiteral("^\\d{2,5}$"));
    int changed = 0;
    for (Contact c : m_db->contacts()) {
        const bool want = on && internal.match(c.number).hasMatch();
        if (c.blf == want)
            continue;
        c.blf = want;
        m_db->saveContact(c);
        ++changed;
    }
    if (on && changed == 0)
        QMessageBox::information(this, tr("Busy lamps"), tr("No internal numbers (2–5 digits) without a lamp."));
}

void ContactsTab::addContact()
{
    Contact c;
    if (editContactDialog(this, c))
        m_db->saveContact(c);
}

void ContactsTab::editContact(qint64 id)
{
    for (Contact c : std::as_const(m_contacts)) {
        if (c.id == id) {
            if (editContactDialog(this, c))
                m_db->saveContact(c);
            return;
        }
    }
}

void ContactsTab::showMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_list->itemAt(pos);
    QMenu menu(this);
    if (item) {
        const Contact c = contactAt(m_list->row(item));
        menu.addAction(Icons::get(QStringLiteral("call-start"), QStringLiteral("call.svg")), tr("Call"), this,
                       [this, c] { emit callRequested(c.number); });
        if (c.blf && m_engine->blf(c.id).state == BlfState::Ringing) {
            // FreePBX / Asterisk directed pickup feature code.
            const QString code = QStringLiteral("**") + c.number;
            menu.addAction(Icons::get(QStringLiteral("call-incoming"), QStringLiteral("call-in.svg")), tr("Pick up the call (%1)").arg(code), this,
                           [this, code] { emit callRequested(code); });
        }
        QAction *lamp = menu.addAction(tr("Busy lamp (BLF)"), this, [this, c](bool on) {
            Contact edited = c;
            edited.blf = on;
            m_db->saveContact(edited);
        });
        lamp->setCheckable(true);
        lamp->setChecked(c.blf);
        menu.addAction(Icons::get(QStringLiteral("document-edit")), tr("Edit…"), this,
                       [this, c] { editContact(c.id); });
        menu.addAction(Icons::get(QStringLiteral("edit-copy")), tr("Copy number"), this,
                       [c] { QApplication::clipboard()->setText(c.number); });
        menu.addSeparator();
        menu.addAction(Icons::get(QStringLiteral("edit-delete")), tr("Delete"), this, [this, c] {
            if (QMessageBox::question(this, tr("Delete contact"), tr("Delete %1?").arg(c.name))
                == QMessageBox::Yes)
                m_db->removeContact(c.id);
        });
    } else {
        menu.addAction(Icons::get(QStringLiteral("contact-new")), tr("Add contact…"), this,
                       &ContactsTab::addContact);
    }
    menu.exec(m_list->viewport()->mapToGlobal(pos));
}

static QStringList parseCsvLine(const QString &line)
{
    QStringList out;
    QString cur;
    bool quoted = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (quoted) {
            if (ch == QLatin1Char('"')) {
                if (i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                    cur += ch;
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                cur += ch;
            }
        } else if (ch == QLatin1Char('"')) {
            quoted = true;
        } else if (ch == QLatin1Char(',') || ch == QLatin1Char(';')) {
            out << cur.trimmed();
            cur.clear();
        } else {
            cur += ch;
        }
    }
    out << cur.trimmed();
    return out;
}

void ContactsTab::importFile()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Import contacts"), QString(),
                                                      tr("Contacts (*.csv *.xml);;All files (*)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Import"), file.errorString());
        return;
    }

    QList<Contact> found;
    if (path.endsWith(QLatin1String(".xml"), Qt::CaseInsensitive)) {
        found = MicrosipImport::readContacts(&file);
    } else {
        QTextStream in(&file);
        while (!in.atEnd()) {
            const QStringList cols = parseCsvLine(in.readLine());
            if (cols.size() < 2)
                continue;
            Contact c{0, cols.at(0), SipUri::cleanNumber(cols.at(1))};
            // Skip a header row like "name,number".
            if (c.number.isEmpty() || c.number.compare(QLatin1String("number"), Qt::CaseInsensitive) == 0)
                continue;
            found.append(c);
        }
    }

    const int added = MicrosipImport::mergeContacts(m_db, found);
    QMessageBox::information(this, tr("Import"), tr("Imported %1 of %2 contacts.").arg(added).arg(found.size()));
}

void ContactsTab::exportCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export contacts"), QStringLiteral("contacts.csv"),
                                                      tr("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Export"), file.errorString());
        return;
    }
    QTextStream out(&file);
    auto quote = [](QString s) { return QLatin1Char('"') + s.replace(QLatin1Char('"'), QStringLiteral("\"\"")) + QLatin1Char('"'); };
    out << "name,number\n";
    for (const Contact &c : std::as_const(m_contacts))
        out << quote(c.name) << ',' << quote(c.number) << '\n';
}
