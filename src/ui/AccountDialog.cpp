#include "ui/AccountDialog.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

AccountDialog::AccountDialog(const AccountConfig &account, QWidget *parent)
    : QDialog(parent)
    , m_account(account)
{
    setWindowTitle(account.server.isEmpty() ? tr("New account") : tr("Edit account"));
    auto *form = new QFormLayout(this);

    m_label = new QLineEdit(account.label, this);
    m_label->setPlaceholderText(tr("e.g. Office"));
    m_server = new QLineEdit(account.server, this);
    m_server->setPlaceholderText(tr("pbx.example.com or 10.0.0.1:5060"));
    m_domain = new QLineEdit(account.domain, this);
    m_domain->setPlaceholderText(tr("same as server"));
    m_proxy = new QLineEdit(account.proxy, this);
    m_proxy->setPlaceholderText(tr("optional"));
    m_user = new QLineEdit(account.user, this);
    m_authUser = new QLineEdit(account.authUser, this);
    m_authUser->setPlaceholderText(tr("same as username"));
    m_password = new QLineEdit(account.password, this);
    m_password->setEchoMode(QLineEdit::Password);
    QAction *reveal = m_password->addAction(QIcon::fromTheme(QStringLiteral("view-visible")), QLineEdit::TrailingPosition);
    reveal->setCheckable(true);
    reveal->setToolTip(tr("Show password"));
    connect(reveal, &QAction::toggled, this, [this](bool on) {
        m_password->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
    });
    if (account.password.isEmpty()) {
        // Imported accounts arrive disabled without a password; typing one switches them on.
        connect(m_password, &QLineEdit::textEdited, this, [this] {
            if (!m_password->text().isEmpty())
                m_enabled->setChecked(true);
        });
    }
    m_displayName = new QLineEdit(account.displayName, this);
    m_displayName->setPlaceholderText(tr("optional"));

    m_transport = new QComboBox(this);
    m_transport->addItem(QStringLiteral("UDP"), QStringLiteral("udp"));
    m_transport->addItem(QStringLiteral("TCP"), QStringLiteral("tcp"));
    m_transport->addItem(QStringLiteral("TLS"), QStringLiteral("tls"));
    m_transport->setCurrentIndex(qMax(0, m_transport->findData(account.transport)));

    m_srtp = new QCheckBox(tr("Encrypt media (SRTP, optional)"), this);
    m_srtp->setChecked(account.srtp);
    m_natRewrite = new QCheckBox(tr("Adapt addresses to NAT (rewrite Contact/Via/SDP)"), this);
    m_natRewrite->setToolTip(tr("Turn on only if calls fail behind a home router and the PBX does not handle NAT."));
    m_natRewrite->setChecked(account.natRewrite);
    m_enabled = new QCheckBox(tr("Enabled"), this);
    m_enabled->setChecked(account.enabled);
    m_expiry = new QSpinBox(this);
    m_expiry->setRange(60, 3600);
    m_expiry->setSuffix(tr(" s"));
    m_expiry->setValue(account.regExpiry);

    form->addRow(tr("Account name:"), m_label);
    form->addRow(tr("SIP server:"), m_server);
    form->addRow(tr("Username:"), m_user);
    form->addRow(tr("Password:"), m_password);
    form->addRow(tr("Display name:"), m_displayName);
    form->addRow(tr("Transport:"), m_transport);
    form->addRow(tr("SIP domain:"), m_domain);
    form->addRow(tr("SIP proxy:"), m_proxy);
    form->addRow(tr("Login (auth ID):"), m_authUser);
    form->addRow(tr("Re-register every:"), m_expiry);
    form->addRow(QString(), m_srtp);
    form->addRow(QString(), m_natRewrite);
    form->addRow(QString(), m_enabled);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_server, &QLineEdit::textChanged, this, &AccountDialog::validate);
    connect(m_user, &QLineEdit::textChanged, this, &AccountDialog::validate);
    validate();

    setMinimumWidth(380);
    if (!account.server.isEmpty() && account.password.isEmpty())
        m_password->setFocus();
}

void AccountDialog::validate()
{
    m_ok->setEnabled(!m_server->text().trimmed().isEmpty() && !m_user->text().trimmed().isEmpty());
}

AccountConfig AccountDialog::account() const
{
    AccountConfig a = m_account;
    if (a.id.isEmpty())
        a.id = Settings::newAccountId();
    auto strip = [](const QString &s) {
        QString t = s.trimmed();
        if (t.startsWith(QLatin1String("sip:"), Qt::CaseInsensitive))
            t = t.mid(4);
        return t;
    };
    a.label = m_label->text().trimmed();
    a.server = strip(m_server->text());
    a.domain = strip(m_domain->text());
    a.proxy = strip(m_proxy->text());
    a.user = m_user->text().trimmed();
    a.authUser = m_authUser->text().trimmed();
    a.password = m_password->text();
    a.displayName = m_displayName->text().trimmed();
    a.transport = m_transport->currentData().toString();
    a.srtp = m_srtp->isChecked();
    a.natRewrite = m_natRewrite->isChecked();
    a.enabled = m_enabled->isChecked();
    a.regExpiry = m_expiry->value();
    return a;
}
