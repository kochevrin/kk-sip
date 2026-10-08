#include "ui/SettingsDialog.h"

#include "core/Autostart.h"
#include "core/MicrosipImport.h"
#include "sip/SipEngine.h"
#include "ui/AccountDialog.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(SipEngine *engine, QWidget *parent)
    : QDialog(parent)
    , m_engine(engine)
    , m_accounts(Settings::instance().accounts)
{
    setWindowTitle(tr("Settings"));
    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    tabs->addTab(createAccountsPage(), tr("Accounts"));
    tabs->addTab(createAudioPage(), tr("Audio"));
    tabs->addTab(createCodecsPage(), tr("Codecs"));
    tabs->addTab(createGeneralPage(), tr("General"));
    layout->addWidget(tabs);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    resize(440, 420);
}

// --- Accounts ---------------------------------------------------------------

static QToolButton *toolButton(const QString &icon, const QString &text, QWidget *parent)
{
    auto *b = new QToolButton(parent);
    b->setIcon(Icons::get(icon));
    b->setText(text);
    b->setToolTip(text);
    b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return b;
}

QWidget *SettingsDialog::createAccountsPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QHBoxLayout(page);

    m_accountList = new QListWidget(page);
    layout->addWidget(m_accountList, 1);

    auto *side = new QVBoxLayout;
    auto *add = toolButton(QStringLiteral("list-add"), tr("Add…"), page);
    auto *edit = toolButton(QStringLiteral("document-edit"), tr("Edit…"), page);
    auto *remove = toolButton(QStringLiteral("list-remove"), tr("Remove"), page);
    auto *up = toolButton(QStringLiteral("go-up"), tr("Up"), page);
    auto *down = toolButton(QStringLiteral("go-down"), tr("Down"), page);
    auto *import = toolButton(QStringLiteral("document-import"), tr("From MicroSIP…"), page);
    import->setToolTip(tr("Import accounts from MicroSIP's microsip.ini"));
    for (QToolButton *b : {add, edit, remove, up, down})
        side->addWidget(b);
    side->addStretch(1);
    side->addWidget(import);
    layout->addLayout(side);

    connect(add, &QToolButton::clicked, this, &SettingsDialog::addAccount);
    connect(edit, &QToolButton::clicked, this, &SettingsDialog::editAccount);
    connect(remove, &QToolButton::clicked, this, &SettingsDialog::removeAccount);
    connect(up, &QToolButton::clicked, this, [this] { moveAccount(-1); });
    connect(down, &QToolButton::clicked, this, [this] { moveAccount(1); });
    connect(import, &QToolButton::clicked, this, &SettingsDialog::importMicrosip);
    connect(m_accountList, &QListWidget::itemActivated, this, &SettingsDialog::editAccount);

    reloadAccounts();
    return page;
}

void SettingsDialog::reloadAccounts()
{
    const int row = m_accountList->currentRow();
    m_accountList->clear();
    for (const AccountConfig &a : std::as_const(m_accounts)) {
        QString text = a.title();
        if (!a.label.isEmpty())
            text += QStringLiteral("  (") + a.user + QLatin1Char('@') + a.sipDomain() + QLatin1Char(')');
        auto *item = new QListWidgetItem(Icons::status(a.enabled ? m_engine->regState(a.id) : RegState::Disabled),
                                         text);
        if (!a.enabled)
            item->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
        m_accountList->addItem(item);
    }
    m_accountList->setCurrentRow(qBound(0, row, int(m_accounts.size()) - 1));
}

void SettingsDialog::addAccount()
{
    AccountDialog dlg(AccountConfig{}, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    m_accounts.append(dlg.account());
    reloadAccounts();
    m_accountList->setCurrentRow(int(m_accounts.size()) - 1);
}

void SettingsDialog::editAccount()
{
    const int row = m_accountList->currentRow();
    if (row < 0 || row >= m_accounts.size())
        return;
    AccountDialog dlg(m_accounts.at(row), this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    m_accounts[row] = dlg.account();
    reloadAccounts();
}

void SettingsDialog::removeAccount()
{
    const int row = m_accountList->currentRow();
    if (row < 0 || row >= m_accounts.size())
        return;
    if (QMessageBox::question(this, tr("Remove account"), tr("Remove account %1?").arg(m_accounts.at(row).title()))
        != QMessageBox::Yes)
        return;
    m_accounts.removeAt(row);
    reloadAccounts();
}

void SettingsDialog::moveAccount(int delta)
{
    const int row = m_accountList->currentRow();
    const int to = row + delta;
    if (row < 0 || to < 0 || to >= m_accounts.size())
        return;
    m_accounts.move(row, to);
    reloadAccounts();
    m_accountList->setCurrentRow(to);
}

void SettingsDialog::importMicrosip()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("MicroSIP settings"), QString(),
                                                      tr("microsip.ini (*.ini);;All files (*)"));
    if (path.isEmpty())
        return;
    QString error;
    const QList<AccountConfig> found = MicrosipImport::readAccounts(path, &error);
    if (found.isEmpty()) {
        QMessageBox::warning(this, tr("Import"), error);
        return;
    }
    const int added = MicrosipImport::mergeAccounts(m_accounts, found);
    reloadAccounts();
    QMessageBox::information(this, tr("Import"),
                             tr("Imported %1 account(s). MicroSIP keeps passwords encrypted with a Windows key, "
                                "so the accounts are disabled until you enter a password with Edit.").arg(added));
}

// --- Audio ------------------------------------------------------------------

QWidget *SettingsDialog::createAudioPage()
{
    const Settings &s = Settings::instance();
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);

    m_capture = new QComboBox(page);
    m_playback = new QComboBox(page);
    m_capture->addItem(tr("System default"), QString());
    m_playback->addItem(tr("System default"), QString());
    const QList<AudioDevice> devices = m_engine->audioDevices();
    for (const AudioDevice &d : devices) {
        if (d.input)
            m_capture->addItem(d.name, d.key);
        if (d.output)
            m_playback->addItem(d.name, d.key);
    }
    m_capture->setCurrentIndex(qMax(0, m_capture->findData(s.captureDevice)));
    m_playback->setCurrentIndex(qMax(0, m_playback->findData(s.playbackDevice)));
    for (QComboBox *box : {m_capture, m_playback}) {
        box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        box->setMinimumContentsLength(20);
    }
    form->addRow(tr("Microphone:"), m_capture);
    form->addRow(tr("Speaker:"), m_playback);

    auto *ringRow = new QHBoxLayout;
    m_ringtone = new QLineEdit(s.ringtoneFile, page);
    m_ringtone->setPlaceholderText(tr("built-in tone"));
    m_ringtone->setClearButtonEnabled(true);
    auto *browse = new QToolButton(page);
    browse->setText(QStringLiteral("…"));
    connect(browse, &QToolButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(this, tr("Ringtone"), QString(), tr("WAV files (*.wav)"));
        if (!f.isEmpty())
            m_ringtone->setText(f);
    });
    ringRow->addWidget(m_ringtone, 1);
    ringRow->addWidget(browse);
    form->addRow(tr("Ringtone:"), ringRow);

    auto *hint = new QLabel(tr("“System default” follows the PipeWire/PulseAudio default device."), page);
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("muted"));
    form->addRow(hint);
    return page;
}

// --- Codecs -----------------------------------------------------------------

QWidget *SettingsDialog::createCodecsPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QHBoxLayout(page);
    m_codecList = new QListWidget(page);
    layout->addWidget(m_codecList, 1);

    // Saved order first, then whatever else PJSIP offers with its default state.
    const QList<CodecSetting> &saved = Settings::instance().codecs;
    const QList<CodecEntry> available = m_engine->codecs();
    QStringList seen;
    auto addItem = [this](const QString &id, bool on) {
        auto *item = new QListWidgetItem(id, m_codecList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(on ? Qt::Checked : Qt::Unchecked);
    };
    for (const CodecSetting &c : saved) {
        if (std::any_of(available.cbegin(), available.cend(), [&](const CodecEntry &e) { return e.id == c.id; })) {
            addItem(c.id, c.enabled);
            seen << c.id;
        }
    }
    for (const CodecEntry &e : available)
        if (!seen.contains(e.id))
            addItem(e.id, e.priority > 0);

    auto *side = new QVBoxLayout;
    auto *up = toolButton(QStringLiteral("go-up"), tr("Up"), page);
    auto *down = toolButton(QStringLiteral("go-down"), tr("Down"), page);
    side->addWidget(up);
    side->addWidget(down);
    side->addStretch(1);
    layout->addLayout(side);
    connect(up, &QToolButton::clicked, this, [this] { moveCodec(-1); });
    connect(down, &QToolButton::clicked, this, [this] { moveCodec(1); });
    return page;
}

void SettingsDialog::moveCodec(int delta)
{
    const int row = m_codecList->currentRow();
    const int to = row + delta;
    if (row < 0 || to < 0 || to >= m_codecList->count())
        return;
    QListWidgetItem *item = m_codecList->takeItem(row);
    m_codecList->insertItem(to, item);
    m_codecList->setCurrentRow(to);
}

// --- General ----------------------------------------------------------------

QWidget *SettingsDialog::createGeneralPage()
{
    const Settings &s = Settings::instance();
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);
    m_closeToTray = new QCheckBox(tr("Closing the window keeps kk-sip in the tray"), page);
    m_closeToTray->setChecked(s.closeToTray);
    m_autostart = new QCheckBox(tr("Start with the system"), page);
    m_autostart->setChecked(Autostart::isEnabled());
    m_startHidden = new QCheckBox(tr("…straight to the tray, without the window"), page);
    m_startHidden->setChecked(s.startHidden);
    m_startHidden->setEnabled(m_autostart->isChecked());
    m_startHidden->setContentsMargins(22, 0, 0, 0);
    connect(m_autostart, &QCheckBox::toggled, m_startHidden, &QWidget::setEnabled);
    m_debugLog = new QCheckBox(tr("Write SIP debug log (restart needed)"), page);
    m_debugLog->setChecked(s.debugLog);
    m_sipPort = new QSpinBox(page);
    m_sipPort->setRange(0, 65535);
    m_sipPort->setSpecialValueText(tr("random"));
    m_sipPort->setValue(s.sipPort);
    m_theme = new QComboBox(page);
    m_theme->addItem(tr("Follow system"), QStringLiteral("system"));
    m_theme->addItem(tr("Light"), QStringLiteral("light"));
    m_theme->addItem(tr("Dark"), QStringLiteral("dark"));
    m_theme->setCurrentIndex(qMax(0, m_theme->findData(s.theme)));
    form->addRow(tr("Theme:"), m_theme);
    form->addRow(m_closeToTray);
    form->addRow(m_autostart);
    form->addRow(m_startHidden);
    form->addRow(m_debugLog);
    form->addRow(tr("Local SIP port (restart needed):"), m_sipPort);
    return page;
}

void SettingsDialog::commit()
{
    Settings &s = Settings::instance();
    s.accounts = m_accounts;
    s.captureDevice = m_capture->currentData().toString();
    s.playbackDevice = m_playback->currentData().toString();
    s.ringtoneFile = m_ringtone->text().trimmed();
    s.codecs.clear();
    for (int i = 0; i < m_codecList->count(); ++i) {
        QListWidgetItem *item = m_codecList->item(i);
        s.codecs.append({item->text(), item->checkState() == Qt::Checked});
    }
    s.closeToTray = m_closeToTray->isChecked();
    s.startHidden = m_startHidden->isChecked();
    s.debugLog = m_debugLog->isChecked();
    if (s.theme != m_theme->currentData().toString()) {
        s.theme = m_theme->currentData().toString();
        Theme::apply(Theme::modeFromString(s.theme));
    }
    Autostart::setEnabled(m_autostart->isChecked(), s.startHidden);
    s.sipPort = m_sipPort->value();
    s.save();
}
