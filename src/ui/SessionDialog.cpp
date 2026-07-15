#include "ui/SessionDialog.h"
#include "core/SessionForm.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QPushButton>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
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

    // --- Advanced, per-protocol options (backends already read these params) ---
    m_advanced = new QGroupBox(QStringLiteral("Advanced"), this);
    m_advForm = new QFormLayout(m_advanced);
    m_compression  = new QCheckBox(QStringLiteral("Enable compression"), this);
    m_x11          = new QCheckBox(QStringLiteral("X11 forwarding"), this);
    m_x11->setChecked(true);
    m_agent        = new QCheckBox(QStringLiteral("Use SSH agent for authentication"), this);
    m_agentForward = new QCheckBox(QStringLiteral("Forward SSH agent"), this);
    m_gwUser       = new QLineEdit(this);
    m_gwUser->setPlaceholderText(QStringLiteral("gateway username (optional)"));
    m_gwPassword   = new QLineEdit(this);
    m_gwPassword->setEchoMode(QLineEdit::Password);
    m_gwPassphrase = new QLineEdit(this);
    m_gwPassphrase->setEchoMode(QLineEdit::Password);
    m_domain         = new QLineEdit(this);
    m_rdpResolution  = new QComboBox(this);
    m_rdpResolution->setEditable(false);
    m_rdpResolution->addItems({QStringLiteral("Default"), QStringLiteral("1920×1080"),
                               QStringLiteral("1600×900"), QStringLiteral("1440×900"),
                               QStringLiteral("1366×768"), QStringLiteral("1280×1024"),
                               QStringLiteral("1280×800"), QStringLiteral("1024×768")});
    m_rdpClipboard   = new QCheckBox(QStringLiteral("Redirect clipboard"), this);
    m_rdpClipboard->setChecked(true);
    m_rdpDrives      = new QCheckBox(QStringLiteral("Redirect drives"), this);
    m_rdpAudio       = new QCheckBox(QStringLiteral("Redirect audio"), this);
    m_rdpNla         = new QCheckBox(QStringLiteral("Network Level Authentication (NLA)"), this);
    m_rdpNla->setChecked(true);
    m_rdpIgnoreCert  = new QCheckBox(QStringLiteral("Ignore certificate warnings"), this);
    m_vncViewOnly    = new QCheckBox(QStringLiteral("View only (don't send input)"), this);

    m_advForm->addRow(QString(), m_compression);
    m_advForm->addRow(QString(), m_x11);
    m_advForm->addRow(QString(), m_agent);
    m_advForm->addRow(QString(), m_agentForward);
    m_advForm->addRow(QStringLiteral("Gateway username"), m_gwUser);
    m_advForm->addRow(QStringLiteral("Gateway password"), m_gwPassword);
    m_advForm->addRow(QStringLiteral("Gateway key passphrase"), m_gwPassphrase);
    m_advForm->addRow(QStringLiteral("Domain"), m_domain);
    m_advForm->addRow(QStringLiteral("Resolution"), m_rdpResolution);
    m_advForm->addRow(QString(), m_rdpClipboard);
    m_advForm->addRow(QString(), m_rdpDrives);
    m_advForm->addRow(QString(), m_rdpAudio);
    m_advForm->addRow(QString(), m_rdpNla);
    m_advForm->addRow(QString(), m_rdpIgnoreCert);
    m_advForm->addRow(QString(), m_vncViewOnly);

    // Show only the fields relevant to the selected session type.
    auto updateVisibility = [this, form, keyRow] {
        const core::SessionType t = core::sessionTypeFromString(m_type->currentText());
        const bool net = (t != core::SessionType::Shell && t != core::SessionType::Serial);
        const bool key = (t == core::SessionType::Ssh || t == core::SessionType::Sftp ||
                          t == core::SessionType::Mosh);
        // The SSH gateway (jump host) applies to SSH-family sessions and to
        // RDP/VNC (routed through a local SSH tunnel).
        const bool gateway = key || t == core::SessionType::Rdp || t == core::SessionType::Vnc;
        const bool ssh = (t == core::SessionType::Ssh);
        const bool rdp = (t == core::SessionType::Rdp);
        const bool vnc = (t == core::SessionType::Vnc);
        form->setRowVisible(m_password, net && t != core::SessionType::Mosh);
        form->setRowVisible(keyRow, key);
        form->setRowVisible(m_passphrase, key);
        form->setRowVisible(m_gateway, gateway);

        // Advanced rows.
        m_advForm->setRowVisible(m_compression, ssh);
        m_advForm->setRowVisible(m_x11, ssh);
        m_advForm->setRowVisible(m_agent, ssh);
        m_advForm->setRowVisible(m_agentForward, ssh);
        m_advForm->setRowVisible(m_gwUser, gateway);
        m_advForm->setRowVisible(m_gwPassword, gateway);
        m_advForm->setRowVisible(m_gwPassphrase, gateway);
        m_advForm->setRowVisible(m_domain, rdp);
        m_advForm->setRowVisible(m_rdpResolution, rdp);
        m_advForm->setRowVisible(m_rdpClipboard, rdp);
        m_advForm->setRowVisible(m_rdpDrives, rdp);
        m_advForm->setRowVisible(m_rdpAudio, rdp);
        m_advForm->setRowVisible(m_rdpNla, rdp);
        m_advForm->setRowVisible(m_rdpIgnoreCert, rdp);
        m_advForm->setRowVisible(m_vncViewOnly, vnc);
        // Hide the whole group when nothing in it applies.
        m_advanced->setVisible(ssh || rdp || vnc || gateway);
    };
    connect(m_type, &QComboBox::currentTextChanged, this, [updateVisibility](const QString&){ updateVisibility(); });
    updateVisibility();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SessionDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_advanced);
    layout->addWidget(buttons);
}

void SessionDialog::browseKeyFile() {
    // Start in ~/.ssh when it exists, else the home directory.
    QString start = QDir::homePath() + QStringLiteral("/.ssh");
    if (!QFileInfo::exists(start)) start = QDir::homePath();

    // Use Qt's own dialog (not the native macOS panel): SSH keys live in the
    // hidden ~/.ssh directory, and the native Open panel hides dotfiles by
    // default — which makes the button look like it "does nothing". The Qt
    // dialog with the Hidden filter shows them reliably on every platform.
    QFileDialog dlg(this, QStringLiteral("Select private key"), start);
    dlg.setFileMode(QFileDialog::ExistingFile);
    dlg.setOption(QFileDialog::DontUseNativeDialog, true);
    dlg.setFilter(QDir::Files | QDir::Hidden | QDir::AllDirs | QDir::NoDotAndDotDot);
    if (dlg.exec() == QDialog::Accepted) {
        const QStringList sel = dlg.selectedFiles();
        if (!sel.isEmpty()) m_keyfile->setText(sel.first());
    }
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

    // Advanced options. Default-on flags use a "!= 0" test so an absent param
    // reads as enabled, matching the connection backends.
    m_compression->setChecked(s.param("compression") == QLatin1String("1"));
    m_x11->setChecked(s.param("x11", QStringLiteral("1")) != QLatin1String("0"));
    m_agent->setChecked(s.param("agent") == QLatin1String("1"));
    m_agentForward->setChecked(s.param("agentforward") == QLatin1String("1"));
    m_gwUser->setText(s.param("gateway_user"));
    m_gwPassword->setText(s.param("gateway_password"));
    m_gwPassphrase->setText(s.param("gateway_passphrase"));
    m_domain->setText(s.param("domain"));
    {
        const QString w = s.param("width"), h = s.param("height");
        const QString res = (!w.isEmpty() && !h.isEmpty())
            ? QStringLiteral("%1×%2").arg(w, h) : QStringLiteral("Default");
        const int idx = m_rdpResolution->findText(res);
        m_rdpResolution->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    m_rdpClipboard->setChecked(s.param("redirect_clipboard", QStringLiteral("1")) != QLatin1String("0"));
    m_rdpDrives->setChecked(s.param("redirect_drives") == QLatin1String("1"));
    m_rdpAudio->setChecked(s.param("redirect_audio") == QLatin1String("1"));
    m_rdpNla->setChecked(s.param("nla", QStringLiteral("1")) != QLatin1String("0"));
    m_rdpIgnoreCert->setChecked(s.param("ignorecert") == QLatin1String("1"));
    m_vncViewOnly->setChecked(s.param("viewonly") == QLatin1String("1"));
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

    // Advanced options, written only for the protocols they apply to so saved
    // sessions stay minimal. Default-off flags emit "1" only when enabled;
    // default-on flags emit "0" only when disabled (absent → backend default).
    const core::SessionType t = core::sessionTypeFromString(m_type->currentText());
    const bool gateway = !m_gateway->text().isEmpty();
    if (t == core::SessionType::Ssh) {
        if (m_compression->isChecked())   f.insert("compression", "1");
        if (!m_x11->isChecked())          f.insert("x11", "0");
        if (m_agent->isChecked())         f.insert("agent", "1");
        if (m_agentForward->isChecked())  f.insert("agentforward", "1");
    }
    if (gateway) {
        if (!m_gwUser->text().isEmpty())       f.insert("gateway_user", m_gwUser->text());
        if (!m_gwPassword->text().isEmpty())   f.insert("gateway_password", m_gwPassword->text());
        if (!m_gwPassphrase->text().isEmpty()) f.insert("gateway_passphrase", m_gwPassphrase->text());
    }
    if (t == core::SessionType::Rdp) {
        if (!m_domain->text().isEmpty())     f.insert("domain", m_domain->text());
        // Resolution "W×H" → width/height params; "Default" leaves them unset.
        const QString res = m_rdpResolution->currentText();
        const int cross = res.indexOf(QChar(0x00D7));   // ×
        if (cross > 0) {
            f.insert("width", res.left(cross));
            f.insert("height", res.mid(cross + 1));
        }
        if (!m_rdpClipboard->isChecked())    f.insert("redirect_clipboard", "0");
        if (m_rdpDrives->isChecked())        f.insert("redirect_drives", "1");
        if (m_rdpAudio->isChecked())         f.insert("redirect_audio", "1");
        if (!m_rdpNla->isChecked())          f.insert("nla", "0");
        if (m_rdpIgnoreCert->isChecked())    f.insert("ignorecert", "1");
    }
    if (t == core::SessionType::Vnc) {
        if (m_vncViewOnly->isChecked())      f.insert("viewonly", "1");
    }
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
