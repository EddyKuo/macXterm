#include "ui/SessionDialog.h"
#include "core/SessionForm.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QFileDialog>
#include <QDir>
#include <QDialogButtonBox>
#include <QMessageBox>

namespace macxterm::ui {

SessionDialog::SessionDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("Session"));
    auto* form = new QFormLayout;
    m_name = new QLineEdit(this);
    m_type = new QComboBox(this);
    for (auto t : {core::SessionType::Ssh, core::SessionType::Telnet, core::SessionType::Serial,
                   core::SessionType::Mosh, core::SessionType::Rdp, core::SessionType::Vnc,
                   core::SessionType::Rsh, core::SessionType::Rlogin, core::SessionType::Xdmcp,
                   core::SessionType::Ftp, core::SessionType::S3, core::SessionType::Browser,
                   core::SessionType::Shell}) {
        m_type->addItem(core::sessionTypeToString(t));
    }
    m_host = new QLineEdit(this);
    m_port = new QSpinBox(this);
    m_port->setRange(0, 65535);
    m_user = new QLineEdit(this);

    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);

    // Private-key file with a Browse button.
    m_keyfile = new QLineEdit(this);
    auto* browse = new QPushButton(QStringLiteral("Browse…"), this);
    connect(browse, &QPushButton::clicked, this, &SessionDialog::browseKeyFile);
    auto* keyRow = new QWidget(this);
    auto* keyLayout = new QHBoxLayout(keyRow);
    keyLayout->setContentsMargins(0, 0, 0, 0);
    keyLayout->addWidget(m_keyfile);
    keyLayout->addWidget(browse);

    m_passphrase = new QLineEdit(this);
    m_passphrase->setEchoMode(QLineEdit::Password);

    m_gateway = new QLineEdit(this);
    m_gateway->setPlaceholderText(QStringLiteral("[user@]host[:port] — optional jump host"));

    form->addRow(QStringLiteral("Name"), m_name);
    form->addRow(QStringLiteral("Type"), m_type);
    form->addRow(QStringLiteral("Host"), m_host);
    form->addRow(QStringLiteral("Port"), m_port);
    form->addRow(QStringLiteral("Username"), m_user);
    form->addRow(QStringLiteral("Password"), m_password);
    form->addRow(QStringLiteral("Private key"), keyRow);
    form->addRow(QStringLiteral("Key passphrase"), m_passphrase);
    form->addRow(QStringLiteral("SSH gateway"), m_gateway);

    // Show only the auth fields relevant to the selected session type.
    auto updateVisibility = [this, form, keyRow] {
        const core::SessionType t = core::sessionTypeFromString(m_type->currentText());
        const bool net = (t != core::SessionType::Shell && t != core::SessionType::Serial);
        const bool key = (t == core::SessionType::Ssh || t == core::SessionType::Sftp ||
                          t == core::SessionType::Mosh);
        form->setRowVisible(m_password, net && t != core::SessionType::Mosh);
        form->setRowVisible(keyRow, key);
        form->setRowVisible(m_passphrase, key);
        form->setRowVisible(m_gateway, key);
    };
    connect(m_type, &QComboBox::currentTextChanged, this, [updateVisibility](const QString&){ updateVisibility(); });
    updateVisibility();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SessionDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

void SessionDialog::browseKeyFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Select private key"),
        QDir::homePath() + QStringLiteral("/.ssh"));
    if (!path.isEmpty()) m_keyfile->setText(path);
}

void SessionDialog::setSession(const core::Session& s) {
    const QVariantMap f = core::SessionForm::fromSession(s);
    m_name->setText(f.value("name").toString());
    m_type->setCurrentText(f.value("type").toString());
    m_host->setText(f.value("host").toString());
    m_port->setValue(f.value("port").toInt());
    m_user->setText(f.value("username").toString());
    m_password->setText(s.param("password"));
    m_keyfile->setText(s.param("keyfile"));
    m_passphrase->setText(s.param("passphrase"));
    m_gateway->setText(s.param("gateway"));
}

core::Session SessionDialog::session() const {
    QVariantMap f;
    f.insert("name", m_name->text());
    f.insert("type", m_type->currentText());
    f.insert("host", m_host->text());
    if (m_port->value() > 0) f.insert("port", m_port->value());
    f.insert("username", m_user->text());
    if (!m_password->text().isEmpty())   f.insert("password", m_password->text());
    if (!m_keyfile->text().isEmpty())    f.insert("keyfile", m_keyfile->text());
    if (!m_passphrase->text().isEmpty()) f.insert("passphrase", m_passphrase->text());
    if (!m_gateway->text().isEmpty())    f.insert("gateway", m_gateway->text());
    return core::SessionForm::toSession(f);
}

void SessionDialog::onAccept() {
    QVariantMap f;
    f.insert("name", m_name->text());
    f.insert("type", m_type->currentText());
    f.insert("host", m_host->text());
    f.insert("port", m_port->value() > 0 ? QString::number(m_port->value()) : QString());
    const QString err = core::SessionForm::validate(f);
    if (!err.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Invalid session"), err);
        return;
    }
    accept();
}

} // namespace macxterm::ui
