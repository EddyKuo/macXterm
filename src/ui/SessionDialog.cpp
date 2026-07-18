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
#include <QTabWidget>
#include <QScrollArea>
#include <QStandardPaths>
#include <QFileInfo>
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

    // Shell program for local Shell sessions (MobaXterm parity). Editable so any
    // path works; pre-populated with the shells detected on this machine. Empty =
    // the platform default (%ComSpec%/cmd on Windows, $SHELL on Unix).
    m_shell = new QComboBox(this);
    m_shell->setEditable(true);
    m_shell->addItem(QString());   // blank = default
    {
        // Add a shell if the file exists, de-duplicated, stored with native
        // separators (CreateProcess dislikes forward slashes in the program path).
        auto addPath = [this](const QString& raw) {
            if (raw.isEmpty()) return;
            const QString p = QDir::toNativeSeparators(raw);
            if (QFileInfo::exists(p) && m_shell->findText(p) < 0) m_shell->addItem(p);
        };
        auto addOnPath = [&](const QString& exe) { addPath(QStandardPaths::findExecutable(exe)); };
#if defined(Q_OS_WIN)
        // cmd — %ComSpec% (always set) with a System32 fallback.
        addPath(qEnvironmentVariable("ComSpec"));
        const QString sysroot = qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"));
        addPath(sysroot + QStringLiteral("/System32/cmd.exe"));
        // Windows PowerShell 5.x — a built-in OS component at a fixed path (may not
        // be on PATH, so add it by absolute location, not just findExecutable).
        addPath(sysroot + QStringLiteral("/System32/WindowsPowerShell/v1.0/powershell.exe"));
        addOnPath(QStringLiteral("powershell"));
        // PowerShell 7+ (pwsh) — check the usual install roots, then PATH.
        for (const QString& base : {qEnvironmentVariable("ProgramFiles"),
                                    qEnvironmentVariable("ProgramW6432"),
                                    qEnvironmentVariable("ProgramFiles(x86)")})
            for (const QString& ver : {QStringLiteral("7"), QStringLiteral("6")})
                if (!base.isEmpty()) addPath(base + QStringLiteral("/PowerShell/") + ver
                                             + QStringLiteral("/pwsh.exe"));
        addOnPath(QStringLiteral("pwsh"));
        // Git Bash.
        addPath(QStringLiteral("C:/Program Files/Git/bin/bash.exe"));
        addPath(QStringLiteral("C:/Program Files/Git/usr/bin/bash.exe"));
#else
        for (const QString& sh : {QStringLiteral("bash"), QStringLiteral("zsh"),
                                  QStringLiteral("fish"), QStringLiteral("sh")})
            addOnPath(sh);
#endif
    }

#if defined(Q_OS_WIN)
    // The Windows dark theme renders combo drop-down arrows almost invisibly, so
    // editable pickers (Shell / Folder) look like plain text fields and users can't
    // find the list. Draw a clear arrow so every dropdown is discoverable.
    setStyleSheet(QStringLiteral(
        "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: center right;"
        " width: 20px; border-left: 1px solid palette(mid); }"
        "QComboBox::down-arrow { width: 0; height: 0;"
        " border-left: 5px solid transparent; border-right: 5px solid transparent;"
        " border-top: 6px solid palette(text); margin-right: 6px; }"));
#endif

    // Bookmark organisation: folder (editable — existing folders are seeded via
    // setKnownFolders) and an optional display icon.
    m_folder = new QComboBox(this);
    m_folder->setEditable(true);
    m_folder->addItem(QString());   // blank = top-level (no folder)
    m_icon = new QComboBox(this);
    m_icon->addItem(QStringLiteral("(default for type)"), QString());
    for (const auto& e : {QStringLiteral("🔑"), QStringLiteral("🖥️"), QStringLiteral("🌐"),
                          QStringLiteral("📁"), QStringLiteral("⭐"), QStringLiteral("🐚"),
                          QStringLiteral("🔒"), QStringLiteral("🧪")})
        m_icon->addItem(e, e);

    form->addRow(QStringLiteral("Name"), m_name);
    form->addRow(QStringLiteral("Type"), m_type);
    form->addRow(QStringLiteral("Host"), m_host);
    form->addRow(QStringLiteral("Port"), m_port);
    form->addRow(QStringLiteral("Username"), m_user);
    form->addRow(QStringLiteral("Password"), m_password);
    form->addRow(QStringLiteral("Private key"), keyRow);
    form->addRow(QStringLiteral("Key passphrase"), m_passphrase);
    form->addRow(QStringLiteral("SSH gateway"), m_gateway);
    form->addRow(QStringLiteral("Shell"), m_shell);
    form->addRow(QStringLiteral("Folder"), m_folder);
    form->addRow(QStringLiteral("Icon"), m_icon);

    // --- Advanced, per-protocol options (backends already read these params) ---
    m_advanced = new QWidget(this);
    m_advForm = new QFormLayout(m_advanced);
    m_compression  = new QCheckBox(QStringLiteral("Enable compression"), this);
    m_x11          = new QCheckBox(QStringLiteral("X11 forwarding"), this);
    m_x11->setChecked(true);
    m_agent        = new QCheckBox(QStringLiteral("Use SSH agent for authentication"), this);
    m_agentForward = new QCheckBox(QStringLiteral("Forward SSH agent"), this);
    m_sshKeepalive = new QSpinBox(this);
    m_sshKeepalive->setRange(0, 86400);
    m_sshKeepalive->setSuffix(QStringLiteral(" s"));
    m_sshKeepalive->setSpecialValueText(QStringLiteral("off"));   // 0 = off
    m_sshRemoteCmd = new QLineEdit(this);
    m_sshRemoteCmd->setPlaceholderText(QStringLiteral("run a command instead of a login shell"));
    m_sshStayOpen  = new QCheckBox(QStringLiteral("Keep pane open after command exits"), this);
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
    m_advForm->addRow(QStringLiteral("Keepalive interval"), m_sshKeepalive);
    m_advForm->addRow(QStringLiteral("Remote command"), m_sshRemoteCmd);
    m_advForm->addRow(QString(), m_sshStayOpen);
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

    // --- Terminal: per-session overrides of the global appearance/behaviour ---
    m_terminal = new QWidget(this);
    auto* termForm = new QFormLayout(m_terminal);
    m_termFont = new QLineEdit(this);
    m_termFont->setPlaceholderText(QStringLiteral("inherit global font"));
    m_termFontSize = new QSpinBox(this);
    m_termFontSize->setRange(0, 72);
    m_termFontSize->setSpecialValueText(QStringLiteral("inherit"));   // 0 shows "inherit"
    m_termScheme = new QComboBox(this);
    m_termScheme->addItem(QStringLiteral("(inherit)"), QString());
    for (const auto& n : {QStringLiteral("Dark"), QStringLiteral("Light"),
                          QStringLiteral("Solarized Dark")})
        m_termScheme->addItem(n, n);
    m_termScrollback = new QSpinBox(this);
    m_termScrollback->setRange(-1, 1000000);
    m_termScrollback->setValue(-1);
    m_termScrollback->setSpecialValueText(QStringLiteral("inherit"));  // -1 shows "inherit"
    m_termBackspace = new QComboBox(this);
    m_termBackspace->addItem(QStringLiteral("Control-? (DEL, default)"), QString());
    m_termBackspace->addItem(QStringLiteral("Control-H"), QStringLiteral("ctrl-h"));
    termForm->addRow(QStringLiteral("Font"), m_termFont);
    termForm->addRow(QStringLiteral("Font size"), m_termFontSize);
    termForm->addRow(QStringLiteral("Color scheme"), m_termScheme);
    termForm->addRow(QStringLiteral("Scrollback lines"), m_termScrollback);
    termForm->addRow(QStringLiteral("Backspace sends"), m_termBackspace);

    // Organise the three sections into tabs so the dialog stays short instead of
    // one tall stacked column. Each page is wrapped in a scroll area so it can
    // never exceed the screen height on a small display.
    auto wrapScroll = [this](QWidget* content) -> QScrollArea* {
        auto* sa = new QScrollArea(this);
        sa->setWidgetResizable(true);
        sa->setFrameShape(QFrame::NoFrame);
        sa->setWidget(content);
        return sa;
    };
    auto* generalPage = new QWidget(this);
    generalPage->setLayout(form);
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(wrapScroll(generalPage), QStringLiteral("General"));
    m_advTabIndex  = m_tabs->addTab(wrapScroll(m_advanced), QStringLiteral("Advanced"));
    m_termTabIndex = m_tabs->addTab(wrapScroll(m_terminal), QStringLiteral("Terminal"));

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
        form->setRowVisible(m_shell, t == core::SessionType::Shell);   // local shell picker
        form->setRowVisible(keyRow, key);
        form->setRowVisible(m_passphrase, key);
        form->setRowVisible(m_gateway, gateway);

        // Advanced rows.
        m_advForm->setRowVisible(m_compression, ssh);
        m_advForm->setRowVisible(m_x11, ssh);
        m_advForm->setRowVisible(m_agent, ssh);
        m_advForm->setRowVisible(m_agentForward, ssh);
        m_advForm->setRowVisible(m_sshKeepalive, ssh);
        m_advForm->setRowVisible(m_sshRemoteCmd, ssh);
        m_advForm->setRowVisible(m_sshStayOpen, ssh);
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
        // Hide the whole Advanced tab when nothing in it applies.
        m_tabs->setTabVisible(m_advTabIndex, ssh || rdp || vnc || gateway);

        // Terminal overrides only apply to session types that host a terminal
        // pane (not the pure remote-desktop / file-transfer protocols).
        const bool terminalPane = (t == core::SessionType::Ssh || t == core::SessionType::Telnet ||
                                   t == core::SessionType::Serial || t == core::SessionType::Mosh ||
                                   t == core::SessionType::Rsh || t == core::SessionType::Rlogin ||
                                   t == core::SessionType::Shell);
        m_tabs->setTabVisible(m_termTabIndex, terminalPane);
    };
    connect(m_type, &QComboBox::currentTextChanged, this, [updateVisibility](const QString&){ updateVisibility(); });
    updateVisibility();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SessionDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_tabs);
    layout->addWidget(buttons);
    // Compact default; the scroll areas keep it within the screen if a tab is
    // taller than this on a small display.
    resize(460, 440);
}

void SessionDialog::setKnownFolders(const QStringList& folders) {
    const QString current = m_folder->currentText();
    m_folder->clear();
    m_folder->addItem(QString());   // blank = top-level
    for (const QString& f : folders)
        if (m_folder->findText(f) < 0) m_folder->addItem(f);
    m_folder->setCurrentText(current);
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
    m_shell->setCurrentText(s.param("shell"));
    m_folder->setCurrentText(s.param("folder"));
    {
        const QString ic = s.param("icon");
        const int idx = ic.isEmpty() ? 0 : m_icon->findData(ic);
        if (idx < 0) { m_icon->addItem(ic, ic); m_icon->setCurrentText(ic); }
        else m_icon->setCurrentIndex(idx);
    }

    // Advanced options. Default-on flags use a "!= 0" test so an absent param
    // reads as enabled, matching the connection backends.
    m_compression->setChecked(s.param("compression") == QLatin1String("1"));
    m_x11->setChecked(s.param("x11", QStringLiteral("1")) != QLatin1String("0"));
    m_agent->setChecked(s.param("agent") == QLatin1String("1"));
    m_agentForward->setChecked(s.param("agentforward") == QLatin1String("1"));
    m_sshKeepalive->setValue(s.param("keepalive").toInt());
    m_sshRemoteCmd->setText(s.param("remotecommand"));
    m_sshStayOpen->setChecked(s.param("stayopen") == QLatin1String("1"));
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

    // Terminal overrides (blank/sentinel = inherit).
    m_termFont->setText(s.param("term.font"));
    m_termFontSize->setValue(s.param("term.fontSize").toInt());   // absent → 0 = inherit
    {
        const QString sc = s.param("term.scheme");
        const int idx = sc.isEmpty() ? 0 : m_termScheme->findData(sc);
        m_termScheme->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    {
        const QString sb = s.param("term.scrollback");
        m_termScrollback->setValue(sb.isEmpty() ? -1 : sb.toInt());
    }
    m_termBackspace->setCurrentIndex(
        s.param("term.backspace") == QLatin1String("ctrl-h") ? 1 : 0);
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
    if (core::sessionTypeFromString(m_type->currentText()) == core::SessionType::Shell
            && !m_shell->currentText().trimmed().isEmpty())
        f.insert("shell", m_shell->currentText().trimmed());
    if (!m_folder->currentText().trimmed().isEmpty())
        f.insert("folder", m_folder->currentText().trimmed());
    if (const QString ic = m_icon->currentData().toString(); !ic.isEmpty())
        f.insert("icon", ic);

    // Advanced options: collect the widget state into a plain struct and let the
    // (unit-tested) SessionForm decide which params to serialize per type.
    const core::SessionType t = core::sessionTypeFromString(m_type->currentText());
    core::SessionForm::AdvancedOptions o;
    o.compression   = m_compression->isChecked();
    o.x11           = m_x11->isChecked();
    o.agent         = m_agent->isChecked();
    o.agentForward  = m_agentForward->isChecked();
    o.sshKeepalive     = m_sshKeepalive->value();
    o.sshRemoteCommand = m_sshRemoteCmd->text();
    o.sshStayOpen      = m_sshStayOpen->isChecked();
    o.gatewayUser       = m_gwUser->text();
    o.gatewayPassword   = m_gwPassword->text();
    o.gatewayPassphrase = m_gwPassphrase->text();
    o.domain        = m_domain->text();
    // Resolution "W×H" → width/height; "Default" leaves them 0.
    const QString res = m_rdpResolution->currentText();
    if (const int cross = res.indexOf(QChar(0x00D7)); cross > 0) {
        o.rdpWidth  = res.left(cross).toInt();
        o.rdpHeight = res.mid(cross + 1).toInt();
    }
    o.rdpClipboard  = m_rdpClipboard->isChecked();
    o.rdpDrives     = m_rdpDrives->isChecked();
    o.rdpAudio      = m_rdpAudio->isChecked();
    o.rdpNla        = m_rdpNla->isChecked();
    o.rdpIgnoreCert = m_rdpIgnoreCert->isChecked();
    o.vncViewOnly   = m_vncViewOnly->isChecked();
    core::SessionForm::applyAdvanced(f, t, /*hasGateway=*/!m_gateway->text().isEmpty(), o);

    // Terminal overrides: serialize only the fields the user actually changed,
    // so inherited bookmarks stay minimal.
    if (!m_termFont->text().isEmpty())  f.insert("term.font", m_termFont->text());
    if (m_termFontSize->value() > 0)    f.insert("term.fontSize", m_termFontSize->value());
    if (const QString sc = m_termScheme->currentData().toString(); !sc.isEmpty())
        f.insert("term.scheme", sc);
    if (m_termScrollback->value() >= 0) f.insert("term.scrollback", m_termScrollback->value());
    if (const QString bs = m_termBackspace->currentData().toString(); !bs.isEmpty())
        f.insert("term.backspace", bs);
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
