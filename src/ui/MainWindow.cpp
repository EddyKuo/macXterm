#include "ui/MainWindow.h"
#include "ui/TerminalWidget.h"
#include "connect/LocalShellConnection.h"
#include "connect/SshConnection.h"
#include "connect/TelnetConnection.h"
#include "connect/SerialConnection.h"
#include "connect/MoshConnection.h"
#include "connect/SimpleTcpConnection.h"
#include "connect/FtpConnection.h"
#include "connect/RdpConnection.h"
#include "connect/VncConnection.h"
#include "ui/SessionDialog.h"
#include "ui/SettingsDialog.h"
#include "ui/TunnelDialog.h"
#include "ui/VaultDialog.h"
#include "ui/SftpPanel.h"
#include "ui/RdpSurfaceWidget.h"
#include "ui/PortScannerDialog.h"
#include "ui/KeyGenDialog.h"
#include "ui/TextEditorDialog.h"
#include "ui/ServersDialog.h"
#include "ui/ImageViewerDialog.h"
#include "ui/FolderDiffDialog.h"
#include "ui/ColorSchemeDialog.h"
#include "ui/PacketCaptureDialog.h"
#include "tools/NetProbe.h"
#include <QSpinBox>
#include <QHBoxLayout>
#include <QPushButton>
#if defined(MACXTERM_HAVE_WEBENGINE)
#include "ui/BrowserTab.h"
#endif
#include "ui/RemoteMonitorBar.h"
#include "tools/S3Client.h"
#include <QListWidget>
#include <QVBoxLayout>
#include "core/Settings.h"
#include "core/SshConfigImporter.h"
#include "core/IniStore.h"
#include "x11/X11Server.h"
#include <QTimer>
#include <QImage>
#include <QMenuBar>
#include <QStatusBar>
#include <QFileInfo>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QKeySequenceEdit>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QTabBar>
#include <QTreeWidget>
#include <QDockWidget>
#include <QToolBar>
#include <QAction>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QSettings>
#include <QFont>
#include <QSplitter>
#include "term/ColorScheme.h"

namespace macxterm::ui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("macXterm"));
    resize(1100, 720);

    m_tabs = new QTabWidget(this);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    // Left-align the tab row (some styles center or stretch tabs by default).
    m_tabs->tabBar()->setExpanding(false);
    m_tabs->setStyleSheet(QStringLiteral("QTabWidget::tab-bar { alignment: left; }"));
    connect(m_tabs, &QTabWidget::tabCloseRequested, m_tabs, [this](int i) {
        QWidget* w = m_tabs->widget(i);
        // Drop session mappings for this tab's pane(s) — the tab may be a single
        // pane or a splitter holding several.
        m_tabSessions.remove(w);
        for (QObject* child : w->findChildren<QObject*>())
            m_tabSessions.remove(qobject_cast<QWidget*>(child));
        m_tabs->removeTab(i);
        w->deleteLater();
    });
    setCentralWidget(m_tabs);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabel(QStringLiteral("Sessions"));
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::itemActivated, this, &MainWindow::onTreeActivated);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &MainWindow::showTreeContextMenu);
    auto* dock = new QDockWidget(QStringLiteral("Sessions"), this);
    dock->setWidget(m_tree);
    addDockWidget(Qt::LeftDockWidgetArea, dock);

    // Open the persistent session store (per-user app data dir) and load saved
    // sessions so they survive restarts.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    if (m_store.open(dir + QStringLiteral("/sessions.db"))) {
        m_sessions = m_store.loadTree();
    }
    loadSettings();

    buildMenus();
    buildToolbar();
    buildShortcuts();
    reloadSessionTree();
    openLocalShell();
}

void MainWindow::buildShortcuts() {
    // Real, registry-driven actions. Adding them to the window activates their
    // QKeySequence globally; the editor rebinds them at runtime.
    auto mk = [this](const QString& id, auto fn) {
        auto* a = new QAction(this);
        connect(a, &QAction::triggered, this, fn);
        addAction(a);
        m_shortcutActions.insert(id, a);
    };
    mk(QStringLiteral("terminal.new"), [this] { openLocalShell(); });
    mk(QStringLiteral("terminal.close"), [this] {
        const int i = m_tabs->currentIndex();
        if (i >= 0) { QWidget* w = m_tabs->widget(i); m_tabs->removeTab(i); w->deleteLater(); }
    });
    mk(QStringLiteral("tab.next"), [this] {
        if (m_tabs->count()) m_tabs->setCurrentIndex((m_tabs->currentIndex() + 1) % m_tabs->count());
    });
    mk(QStringLiteral("tab.prev"), [this] {
        if (m_tabs->count()) m_tabs->setCurrentIndex((m_tabs->currentIndex() + m_tabs->count() - 1) % m_tabs->count());
    });
    mk(QStringLiteral("view.fullscreen"), [this] { setWindowState(windowState() ^ Qt::WindowFullScreen); });

    // Load any saved overrides.
    QSettings qs(QStringLiteral("macXterm"), QStringLiteral("macXterm"));
    for (const QString& id : m_shortcutActions.keys()) {
        const QString saved = qs.value(QStringLiteral("shortcut/") + id).toString();
        if (!saved.isEmpty()) m_shortcuts.rebind(id, QKeySequence(saved));
    }
    applyShortcuts();
}

void MainWindow::applyShortcuts() {
    for (auto it = m_shortcutActions.constBegin(); it != m_shortcutActions.constEnd(); ++it)
        it.value()->setShortcut(m_shortcuts.sequence(it.key()));
}

void MainWindow::editShortcuts() {
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Keyboard Shortcuts"));
    auto* form = new QFormLayout(&dlg);
    QHash<QString, QKeySequenceEdit*> edits;
    // Only the actions that are actually wired are editable.
    const QStringList ids = m_shortcutActions.keys();
    for (const QString& id : ids) {
        auto* edit = new QKeySequenceEdit(m_shortcuts.sequence(id), &dlg);
        form->addRow(id, edit);
        edits.insert(id, edit);
    }
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    if (dlg.exec() != QDialog::Accepted) return;
    QSettings qs(QStringLiteral("macXterm"), QStringLiteral("macXterm"));
    for (auto it = edits.constBegin(); it != edits.constEnd(); ++it) {
        m_shortcuts.rebind(it.key(), it.value()->keySequence());   // conflicts are rejected
        qs.setValue(QStringLiteral("shortcut/") + it.key(), it.value()->keySequence().toString());
    }
    applyShortcuts();
}

void MainWindow::buildMenus() {
    // Keep the menu bar docked inside the main window on every platform. On
    // macOS Qt would otherwise hoist it into the desktop global menu bar; we
    // want all commands to live in the application window itself.
    menuBar()->setNativeMenuBar(false);

    QMenu* file = menuBar()->addMenu(QStringLiteral("&File"));
    file->addAction(QStringLiteral("New Shell"), this, [this] { openLocalShell(); });
    file->addAction(QStringLiteral("New Session…"), this, [this] {
        SessionDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) { core::Session s = dlg.session(); addAndSaveSession(s); openSession(s); }
    });
    file->addSeparator();
    file->addAction(QStringLiteral("Import from ~/.ssh/config"), this, &MainWindow::importSshConfig);
    file->addAction(QStringLiteral("Export Sessions…"), this, &MainWindow::exportSessions);
    file->addAction(QStringLiteral("Import Shared Sessions…"), this, &MainWindow::importSharedSessions);
    file->addSeparator();
    file->addAction(QStringLiteral("Quit"), this, [this] { close(); });

    QMenu* view = menuBar()->addMenu(QStringLiteral("&View"));
    view->addAction(QStringLiteral("Split Right"), this, [this] { splitCurrent(Qt::Horizontal); });
    view->addAction(QStringLiteral("Split Down"), this, [this] { splitCurrent(Qt::Vertical); });
    view->addAction(QStringLiteral("Split 2×2 Grid"), this, [this] { splitQuad(); });
    view->addSeparator();
    view->addAction(QStringLiteral("Full Screen"), this, [this] {
        setWindowState(windowState() ^ Qt::WindowFullScreen);
    })->setShortcut(Qt::Key_F11);
    QMenu* trans = view->addMenu(QStringLiteral("Transparency"));
    for (int pct : {100, 95, 90, 85, 80}) {
        trans->addAction(QStringLiteral("%1%").arg(pct), this,
                         [this, pct] { setWindowOpacity(pct / 100.0); });
    }
    QMenu* pasteMenu = view->addMenu(QStringLiteral("Paste Delay"));
    for (int ms : {0, 10, 50, 100, 250}) {
        pasteMenu->addAction(ms == 0 ? QStringLiteral("Off") : QStringLiteral("%1 ms/line").arg(ms),
                             this, [this, ms] {
            m_pasteDelay = ms;
            for (TerminalWidget* p : m_tabs->findChildren<TerminalWidget*>()) p->setPasteDelay(ms);
        });
    }
    view->addSeparator();
    auto* hlAction = view->addAction(QStringLiteral("Syntax Highlighting"));
    hlAction->setCheckable(true);
    connect(hlAction, &QAction::toggled, this, [this](bool on) {
        for (TerminalWidget* p : m_tabs->findChildren<TerminalWidget*>())
            p->setSyntaxHighlighting(on);
        m_syntaxHighlight = on;
    });
    view->addSeparator();
    view->addAction(QStringLiteral("Detach Current Tab"), this, &MainWindow::detachCurrentTab);
    view->addAction(QStringLiteral("Keyboard Shortcuts…"), this, &MainWindow::editShortcuts);

    QMenu* tools = menuBar()->addMenu(QStringLiteral("&Tools"));
    tools->addAction(QStringLiteral("Port Scanner…"), this, [this] {
        auto* dlg = new PortScannerDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    tools->addAction(QStringLiteral("SSH Key Generator…"), this, [this] {
        auto* dlg = new KeyGenDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    tools->addAction(QStringLiteral("Packet Capture…"), this, [this] {
        auto* dlg = new PacketCaptureDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    tools->addAction(QStringLiteral("Network Tools (ping / httping)…"), this, [this] {
        auto* dlg = new QDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowTitle(QStringLiteral("Network Tools"));
        auto* lay = new QFormLayout(dlg);
        auto* host = new QLineEdit(QStringLiteral("localhost"), dlg);
        auto* portSpin = new QSpinBox(dlg); portSpin->setRange(1, 65535); portSpin->setValue(80);
        auto* result = new QLabel(dlg);
        result->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto* tcpBtn = new QPushButton(QStringLiteral("TCP ping"), dlg);
        auto* httpBtn = new QPushButton(QStringLiteral("httping"), dlg);
        connect(tcpBtn, &QPushButton::clicked, dlg, [=] {
            const auto r = tools::NetProbe::tcpPing(host->text(),
                static_cast<quint16>(portSpin->value()));
            result->setText(r.ok ? QStringLiteral("TCP %1 ms — %2").arg(r.ms).arg(r.detail)
                                 : QStringLiteral("failed: %1").arg(r.detail));
        });
        connect(httpBtn, &QPushButton::clicked, dlg, [=] {
            const auto r = tools::NetProbe::httping(host->text());
            result->setText(r.ok ? QStringLiteral("HTTP %1 ms — %2").arg(r.ms).arg(r.detail)
                                 : QStringLiteral("failed: %1").arg(r.detail));
        });
        lay->addRow(QStringLiteral("Host"), host);
        lay->addRow(QStringLiteral("Port"), portSpin);
        auto* btns = new QHBoxLayout; btns->addWidget(tcpBtn); btns->addWidget(httpBtn);
        lay->addRow(btns);
        lay->addRow(result);
        dlg->show();
    });
    tools->addSeparator();
    tools->addAction(QStringLiteral("Text Editor…"), this, [this] {
        auto* dlg = new TextEditorDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    tools->addAction(QStringLiteral("Compare Files…"), this, [this] {
        const QString a = QFileDialog::getOpenFileName(this, QStringLiteral("First file"));
        if (a.isEmpty()) return;
        const QString b = QFileDialog::getOpenFileName(this, QStringLiteral("Second file"));
        if (b.isEmpty()) return;
        auto* dlg = new TextEditorDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->showDiff(a, b);
        dlg->show();
    });
    tools->addAction(QStringLiteral("Compare Folders…"), this, [this] {
        auto* dlg = new FolderDiffDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    tools->addAction(QStringLiteral("Image Viewer…"), this, [this] {
        const QString p = QFileDialog::getOpenFileName(this, QStringLiteral("Open image"),
            QString(), QStringLiteral("Images (*.png *.jpg *.jpeg *.gif *.bmp *.webp *.svg)"));
        if (p.isEmpty()) return;
        auto* dlg = new ImageViewerDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->openImage(p);
        dlg->show();
    });
    tools->addAction(QStringLiteral("Color Scheme Editor…"), this, [this] {
        TerminalWidget* pane = currentPane();
        const term::ColorScheme start = term::ColorScheme::byName(m_settings.colorScheme());
        auto* dlg = new ColorSchemeDialog(start, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        if (pane)
            connect(dlg, &ColorSchemeDialog::schemeChosen, pane, &TerminalWidget::setColorScheme);
        dlg->show();
    });
    tools->addAction(QStringLiteral("Light Servers…"), this, [this] {
        auto* dlg = new ServersDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    tools->addSeparator();
    tools->addAction(QStringLiteral("Start X Server (for X11 forwarding)"), this, [this] {
        QString msg;
        const bool ok = x11::X11Server::ensureRunning(msg);
        if (ok) statusBar()->showMessage(msg, 5000);
        else QMessageBox::information(this, QStringLiteral("X Server"), msg);
    });
    tools->addAction(QStringLiteral("Log Session to File…"), this, [this] {
        TerminalWidget* pane = currentPane();
        if (!pane) return;
        if (pane->isLogging()) {
            pane->stopLogging();
            statusBar()->showMessage(QStringLiteral("Session logging stopped"), 3000);
            return;
        }
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Log session to"));
        if (path.isEmpty()) return;
        if (pane->startLogging(path))
            statusBar()->showMessage(QStringLiteral("Logging session → %1").arg(path), 3000);
        else
            QMessageBox::warning(this, windowTitle(), QStringLiteral("Cannot open log file"));
    });

    QMenu* macros = menuBar()->addMenu(QStringLiteral("&Macros"));
    macros->addAction(QStringLiteral("Start/Stop Recording"), this, &MainWindow::toggleMacroRecording)
        ->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+R")));
    macros->addAction(QStringLiteral("Play Macro"), this, &MainWindow::playMacro)
        ->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")));

    QMenu* help = menuBar()->addMenu(QStringLiteral("&Help"));
    help->addAction(QStringLiteral("About macXterm"), this, [this] {
        QMessageBox::about(this, QStringLiteral("About macXterm"),
            QStringLiteral("<b>macXterm</b><br>A native, cross-platform, MIT-licensed "
                           "MobaXterm-style remote toolbox.<br>Built with Qt 6 + C/C++."));
    });
}

void MainWindow::detachCurrentTab() {
    const int idx = m_tabs->currentIndex();
    if (idx < 0) return;
    QWidget* w = m_tabs->widget(idx);
    const QString title = m_tabs->tabText(idx);
    m_tabs->removeTab(idx);
    auto* win = new QMainWindow;   // top-level floating window
    win->setAttribute(Qt::WA_DeleteOnClose);
    win->setWindowTitle(QStringLiteral("macXterm — ") + title);
    win->setCentralWidget(w);
    w->show();
    win->resize(820, 520);
    win->show();
}

void MainWindow::toggleMacroRecording() {
    TerminalWidget* pane = currentPane();
    if (!pane) return;
    if (!m_recordingMacro) {
        m_recordingMacro = true;
        m_macro = core::Macro(QStringLiteral("recorded"));
        m_macro.beginRecording();
        // Record keystrokes while still sending them to the pane.
        pane->setInputHandler([this, pane](const QByteArray& b) {
            m_macro.record(b);
            pane->feedInput(b);
        });
        statusBar()->showMessage(QStringLiteral("Recording macro… (Macros ▸ Start/Stop to finish)"));
    } else {
        m_recordingMacro = false;
        m_macro.endRecording();
        // Restore normal routing (respect MultiExec).
        if (m_multiExec) pane->setInputHandler([this](const QByteArray& b) { broadcastInput(b); });
        else pane->setInputHandler(nullptr);
        statusBar()->showMessage(QStringLiteral("Recorded %1 keystroke event(s).").arg(m_macro.eventCount()), 4000);
    }
}

void MainWindow::playMacro() {
    TerminalWidget* pane = currentPane();
    if (!pane || m_macro.eventCount() == 0) return;
    m_macro.replay([pane](const QByteArray& b) { pane->feedInput(b); });
}

QString MainWindow::vaultPath() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/vault.bin");
}

void MainWindow::openVault() {
    const bool exists = QFileInfo::exists(vaultPath());
    VaultDialog dlg(exists ? VaultDialog::Mode::Unlock : VaultDialog::Mode::Create, this);
    if (dlg.exec() != QDialog::Accepted) return;
    const QString pw = dlg.password();
    if (exists) {
        if (!m_vault.load(vaultPath(), pw)) {
            QMessageBox::warning(this, QStringLiteral("Vault"), QStringLiteral("Wrong master password."));
            return;
        }
    } else {
        m_vault.clear();
        m_vault.save(vaultPath(), pw);   // create an empty vault
    }
    m_masterPassword = pw;
    m_vaultUnlocked = true;
    statusBar()->showMessage(QStringLiteral("Vault unlocked — session passwords are now stored encrypted."), 4000);
}

core::Session MainWindow::resolveSecrets(core::Session s) const {
    // If the session references a vault secret and the vault is unlocked, inject
    // the real password before connecting (kept out of the SQLite store).
    const QString ref = s.param("vault_ref");
    if (!ref.isEmpty() && m_vaultUnlocked && m_vault.hasSecret(ref)) {
        s.setParam("password", m_vault.secret(ref));
    }
    return s;
}

void MainWindow::importSshConfig() {
    const QString path = QDir::homePath() + QStringLiteral("/.ssh/config");
    if (!QFileInfo::exists(path)) {
        QMessageBox::information(this, QStringLiteral("Import"),
            QStringLiteral("No ~/.ssh/config found."));
        return;
    }
    core::SessionFolder imported = core::SshConfigImporter::importFile(path);
    int added = 0;
    for (const core::Session& s : imported.sessions()) {
        if (!m_sessions.findSession(s.name())) { m_sessions.addSession(s); ++added; }
    }
    persistSessions();
    reloadSessionTree();
    QMessageBox::information(this, QStringLiteral("Import"),
        QStringLiteral("Imported %1 new session(s) from ~/.ssh/config.").arg(added));
}

void MainWindow::exportSessions() {
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export sessions"), QStringLiteral("sessions.mxsess"),
        QStringLiteral("macXterm sessions (*.mxsess *.ini)"));
    if (path.isEmpty()) return;
    if (core::IniStore::save(m_sessions, path))
        statusBar()->showMessage(QStringLiteral("Exported sessions → %1").arg(path), 3000);
    else
        QMessageBox::warning(this, windowTitle(), QStringLiteral("Export failed"));
}

void MainWindow::importSharedSessions() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import shared sessions"), QString(),
        QStringLiteral("macXterm sessions (*.mxsess *.ini);;All files (*)"));
    if (path.isEmpty()) return;
    core::SessionFolder imported;
    if (!core::IniStore::load(imported, path)) {
        QMessageBox::warning(this, windowTitle(), QStringLiteral("Import failed"));
        return;
    }
    int added = 0;
    for (const core::Session& s : imported.sessions())
        if (!m_sessions.findSession(s.name())) { m_sessions.addSession(s); ++added; }
    persistSessions();
    reloadSessionTree();
    QMessageBox::information(this, QStringLiteral("Import"),
        QStringLiteral("Imported %1 new session(s).").arg(added));
}

void MainWindow::buildToolbar() {
    auto* tb = addToolBar(QStringLiteral("Main"));
    auto* newShell = tb->addAction(QStringLiteral("New Shell"));
    connect(newShell, &QAction::triggered, this, [this] { openLocalShell(); });

    auto* newSession = tb->addAction(QStringLiteral("New Session"));
    connect(newSession, &QAction::triggered, this, [this] {
        SessionDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            core::Session s = dlg.session();
            addAndSaveSession(s);   // persist so it survives restart
            openSession(s);
        }
    });

    auto* splitH = tb->addAction(QStringLiteral("Split →"));
    connect(splitH, &QAction::triggered, this, [this] { splitCurrent(Qt::Horizontal); });
    auto* splitV = tb->addAction(QStringLiteral("Split ↓"));
    connect(splitV, &QAction::triggered, this, [this] { splitCurrent(Qt::Vertical); });

    auto* multi = tb->addAction(QStringLiteral("MultiExec"));
    multi->setCheckable(true);
    connect(multi, &QAction::toggled, this, &MainWindow::toggleMultiExec);

    auto* tunnels = tb->addAction(QStringLiteral("Tunnel"));
    connect(tunnels, &QAction::triggered, this, [this] {
        // The tunnel is established through the currently-active SSH tab.
        TerminalWidget* pane = currentPane();
        const core::Session sshSession = pane ? m_tabSessions.value(pane) : core::Session();
        if (sshSession.type() != core::SessionType::Ssh) {
            QMessageBox::information(this, QStringLiteral("SSH Tunnel"),
                QStringLiteral("Open and select an SSH session tab first — the tunnel is "
                               "forwarded through it."));
            return;
        }
        TunnelDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            auto* t = new tunnel::SshTunnel(this);
            connect(t, &tunnel::SshTunnel::error, this, [this](const QString& m) {
                QMessageBox::warning(this, QStringLiteral("Tunnel error"), m);
            });
            if (t->start(sshSession, dlg.tunnel())) {
                m_tunnels.append(t);
                QMessageBox::information(this, QStringLiteral("SSH Tunnel"),
                    QStringLiteral("Local tunnel listening on port %1 → %2:%3 via %4")
                        .arg(t->listenPort()).arg(dlg.tunnel().targetHost)
                        .arg(dlg.tunnel().targetPort).arg(sshSession.host()));
            } else {
                t->deleteLater();
            }
        }
    });

    auto* settings = tb->addAction(QStringLiteral("Settings"));
    connect(settings, &QAction::triggered, this, [this] {
        SettingsDialog dlg(m_settings, this);
        if (dlg.exec() == QDialog::Accepted) {
            m_settings = dlg.settings();
            saveSettings();
            // Apply immediately to every open terminal pane.
            for (int i = 0; i < m_tabs->count(); ++i) {
                if (auto* term = qobject_cast<TerminalWidget*>(m_tabs->widget(i)))
                    applySettings(term);
            }
        }
    });

    auto* vault = tb->addAction(QStringLiteral("Vault"));
    connect(vault, &QAction::triggered, this, [this] { openVault(); });

    // Quick-connect bar: type "user@host[:port]" and press Enter to SSH.
    tb->addSeparator();
    tb->addWidget(new QLabel(QStringLiteral(" Quick connect: "), this));
    auto* quick = new QLineEdit(this);
    quick->setPlaceholderText(QStringLiteral("user@host:port"));
    quick->setClearButtonEnabled(true);
    quick->setMaximumWidth(240);
    connect(quick, &QLineEdit::returnPressed, this, [this, quick] {
        const QString text = quick->text().trimmed();
        if (text.isEmpty()) return;
        core::Session s(text, core::SessionType::Ssh);
        QString rest = text;
        if (const int at = rest.indexOf('@'); at >= 0) {
            s.setUsername(rest.left(at));
            rest = rest.mid(at + 1);
        }
        if (const int colon = rest.indexOf(':'); colon >= 0) {
            s.setHost(rest.left(colon));
            s.setPort(rest.mid(colon + 1).toInt());
        } else {
            s.setHost(rest);
        }
        quick->clear();
        openSession(s);
    });
    tb->addWidget(quick);
}

void MainWindow::reloadSessionTree() {
    m_tree->clear();
    auto* root = new QTreeWidgetItem(m_tree, {m_sessions.name()});
    for (const core::Session& s : m_sessions.sessions()) {
        new QTreeWidgetItem(root, {s.name()});
    }
    root->setExpanded(true);
}

void MainWindow::onTreeActivated(QTreeWidgetItem* item, int) {
    if (!item) return;
    if (const core::Session* s = m_sessions.findSession(item->text(0))) {
        openSession(*s);
    }
}

void MainWindow::persistSessions() {
    if (m_store.isOpen()) m_store.saveTree(m_sessions);
}

void MainWindow::addAndSaveSession(const core::Session& sIn) {
    core::Session s = sIn;
    // If a password was entered and the vault is unlocked, store it encrypted in
    // the vault and keep only a reference in the (SQLite) session — never a
    // plaintext password in the database.
    if (!s.param("password").isEmpty() && m_vaultUnlocked) {
        const QString ref = QStringLiteral("session:") + s.name();
        m_vault.setSecret(ref, s.param("password"));
        m_vault.save(vaultPath(), m_masterPassword);
        s.setParam("vault_ref", ref);
        QVariantMap p = s.params();
        p.remove(QStringLiteral("password"));
        s.setParams(p);
    }
    m_sessions.addSession(s);
    persistSessions();
    reloadSessionTree();
}

void MainWindow::deleteSession(const QString& name) {
    QList<core::Session>& list = m_sessions.sessions();
    for (int i = 0; i < list.size(); ++i) {
        if (list[i].name() == name) { list.removeAt(i); break; }
    }
    persistSessions();
    reloadSessionTree();
}

void MainWindow::showTreeContextMenu(const QPoint& pos) {
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    if (!item) return;
    const QString name = item->text(0);
    const core::Session* s = m_sessions.findSession(name);

    QMenu menu(this);
    if (s) {
        QAction* open = menu.addAction(QStringLiteral("Open"));
        connect(open, &QAction::triggered, this, [this, name] {
            if (const core::Session* sp = m_sessions.findSession(name)) openSession(*sp);
        });
        QAction* edit = menu.addAction(QStringLiteral("Edit…"));
        connect(edit, &QAction::triggered, this, [this, name] {
            const core::Session* sp = m_sessions.findSession(name);
            if (!sp) return;
            SessionDialog dlg(this);
            dlg.setSession(*sp);
            if (dlg.exec() == QDialog::Accepted) {
                deleteSession(name);            // remove the old entry
                addAndSaveSession(dlg.session());  // save the edited one
            }
        });
        QAction* dup = menu.addAction(QStringLiteral("Duplicate"));
        connect(dup, &QAction::triggered, this, [this, name] {
            const core::Session* sp = m_sessions.findSession(name);
            if (!sp) return;
            core::Session copy = *sp;
            copy.setName(name + QStringLiteral(" (copy)"));
            addAndSaveSession(copy);
        });
        QAction* del = menu.addAction(QStringLiteral("Delete"));
        connect(del, &QAction::triggered, this, [this, name] {
            if (QMessageBox::question(this, QStringLiteral("Delete session"),
                    QStringLiteral("Delete session \"%1\"?").arg(name)) == QMessageBox::Yes) {
                deleteSession(name);
            }
        });
    }
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

TerminalWidget* MainWindow::openLocalShell() {
    core::Session s(QStringLiteral("local"), core::SessionType::Shell);
    return openSession(s);
}

TerminalWidget* MainWindow::makePane(const core::Session& session) {
    auto* term = new TerminalWidget(m_tabs);
    connect::IConnection* conn = nullptr;
    switch (session.type()) {
        case core::SessionType::Ssh:    conn = new connect::SshConnection(term);    break;
        case core::SessionType::Telnet: conn = new connect::TelnetConnection(term); break;
        case core::SessionType::Serial: conn = new connect::SerialConnection(term); break;
        case core::SessionType::Mosh:   conn = new connect::MoshConnection(term);   break;
        case core::SessionType::Ftp:    conn = new connect::FtpConnection(term);    break;
        case core::SessionType::Rdp:    conn = new connect::RdpConnection(term);    break;
        case core::SessionType::Vnc:    conn = new connect::VncConnection(term);    break;
        case core::SessionType::Rsh:
        case core::SessionType::Rlogin:
        case core::SessionType::Xdmcp:
            conn = new connect::SimpleTcpConnection(session.type(), term); break;
        default:                        conn = new connect::LocalShellConnection(term); break;
    }
    term->attach(conn);
    applySettings(term);
    m_tabSessions.insert(term, session);
    if (m_multiExec)
        term->setInputHandler([this](const QByteArray& b) { broadcastInput(b); });

    // For SSH sessions with X11 forwarding enabled, make sure a local X server is
    // up so forwarded windows can display (best-effort; no-op if already running).
    if (session.type() == core::SessionType::Ssh && session.param("x11", "1") != "0"
        && !x11::X11Server::isRunning()) {
        QString msg;
        x11::X11Server::ensureRunning(msg);
        if (!msg.isEmpty()) statusBar()->showMessage(msg, 4000);
    }
    conn->connectSession(resolveSecrets(session));

    // Show the SFTP browser + start the remote monitor for SSH sessions.
    if (conn->capabilities().sftp) {
        showSftpFor(session);
        // Follow-terminal-folder: OSC 7 cwd changes re-home the SFTP panel.
        if (m_sftpPanel)
            connect(term, &TerminalWidget::cwdChanged, m_sftpPanel, &SftpPanel::setRemoteCwd);
        if (!m_monitor) {
            m_monitor = new RemoteMonitorBar(this);
            statusBar()->addPermanentWidget(m_monitor);
        }
        m_monitor->start(resolveSecrets(session));
    }
    return term;
}

TerminalWidget* MainWindow::openSession(const core::Session& session) {
    // S3 sessions list the bucket in a simple dialog (host=bucket,
    // username=accessKey, password=secretKey, param "region").
    if (session.type() == core::SessionType::S3) {
        const core::Session s = resolveSecrets(session);
        auto* client = new tools::S3Client(this);
        auto* dlg = new QDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowTitle(QStringLiteral("S3: ") + s.host());
        dlg->resize(420, 520);
        auto* lay = new QVBoxLayout(dlg);
        auto* list = new QListWidget(dlg);
        lay->addWidget(list);
        connect(client, &tools::S3Client::listed, dlg, [list](const QStringList& keys) {
            list->addItems(keys.isEmpty() ? QStringList{QStringLiteral("(empty)")} : keys);
        });
        connect(client, &tools::S3Client::failed, dlg, [list](const QString& e) {
            list->addItem(QStringLiteral("Error: ") + e);
        });
        client->listBucket(s.username(), s.param("password"),
                           s.param("region", QStringLiteral("us-east-1")), s.host());
        dlg->show();
        return nullptr;
    }
    // Browser sessions open an embedded web view tab (falls back to the system
    // browser when built without QtWebEngine).
    if (session.type() == core::SessionType::Browser) {
#if defined(MACXTERM_HAVE_WEBENGINE)
        auto* browser = new BrowserTab(m_tabs);
        browser->load(session.host());
        const int idx = m_tabs->addTab(browser, session.name());
        m_tabs->setCurrentIndex(idx);
        return nullptr;
#else
        QString url = session.host();
        if (!url.contains(QStringLiteral("://"))) url.prepend(QStringLiteral("https://"));
        QDesktopServices::openUrl(QUrl(url));
        return nullptr;
#endif
    }
    // RDP/VNC render their own graphics surface, not a VT stream.
    if (session.type() == core::SessionType::Rdp || session.type() == core::SessionType::Vnc) {
        openGraphicalSession(session);
        return nullptr;
    }
    // SFTP sessions open the graphical file browser directly (over their own
    // SSH/SFTP socket) — there is no terminal pane. Without this an SFTP session
    // fell through to a local shell.
    if (session.type() == core::SessionType::Sftp) {
        showSftpFor(session);
        return nullptr;
    }
    TerminalWidget* term = makePane(session);
    const int idx = m_tabs->addTab(term, session.name());
    m_tabs->setCurrentIndex(idx);
    term->setFocus();
    return term;
}

core::Session MainWindow::resolveGateway(const core::Session& session) {
    const QString gw = session.param("gateway");
    if (gw.isEmpty()) return session;

    // Parse the gateway spec "[user@]host[:port]".
    QString spec = gw, gwUser = session.username();
    if (const int at = spec.indexOf('@'); at >= 0) { gwUser = spec.left(at); spec = spec.mid(at + 1); }
    QString gwHost = spec;
    int gwPort = 22;
    if (const int c = spec.indexOf(':'); c >= 0) {
        gwHost = spec.left(c);
        gwPort = spec.mid(c + 1).toInt();
        if (gwPort <= 0) gwPort = 22;
    }
    if (!session.param("gateway_user").isEmpty()) gwUser = session.param("gateway_user");

    // Build the SSH gateway session (its own optional credentials).
    core::Session gwSess(gwHost, core::SessionType::Ssh);
    gwSess.setHost(gwHost);
    gwSess.setPort(gwPort);
    gwSess.setUsername(gwUser);
    if (!session.param("gateway_password").isEmpty())
        gwSess.setParam("password", session.param("gateway_password"));
    else if (!session.param("password").isEmpty())
        gwSess.setParam("password", session.param("password"));
    if (!session.param("gateway_keyfile").isEmpty())
        gwSess.setParam("keyfile", session.param("gateway_keyfile"));
    if (!session.param("gateway_passphrase").isEmpty())
        gwSess.setParam("passphrase", session.param("gateway_passphrase"));

    // Tunnel: listen on a random local port, forward to the real target.
    tunnel::Tunnel t;
    t.kind = tunnel::TunnelKind::Local;
    t.bindAddr = QStringLiteral("127.0.0.1");
    t.bindPort = 0;
    t.targetHost = session.host();
    t.targetPort = static_cast<quint16>(session.port());

    auto* tun = new tunnel::SshTunnel(this);
    if (!tun->start(gwSess, t) || tun->listenPort() == 0) {
        tun->deleteLater();
        statusBar()->showMessage(QStringLiteral("Gateway tunnel failed; connecting directly"), 4000);
        return session;
    }
    m_tunnels.push_back(tun);

    // Return a copy pointing at the local end of the tunnel.
    core::Session routed = session;
    routed.setHost(QStringLiteral("127.0.0.1"));
    routed.setPort(tun->listenPort());
    statusBar()->showMessage(
        QStringLiteral("Routing %1 via gateway %2 (127.0.0.1:%3)")
            .arg(session.host(), gwHost).arg(tun->listenPort()), 4000);
    return routed;
}

void MainWindow::openGraphicalSession(const core::Session& sessionIn) {
    // Route through an SSH gateway first if configured.
    const core::Session session = resolveGateway(resolveSecrets(sessionIn));
    auto* surface = new RdpSurfaceWidget(m_tabs);

    if (session.type() == core::SessionType::Vnc) {
        auto* vnc = new connect::VncConnection(surface);
        connect(vnc, &connect::VncConnection::serverReady, surface,
                [surface](int w, int h, const QString&) {
                    QImage img(w, h, QImage::Format_ARGB32);
                    img.fill(Qt::black);
                    surface->setFrame(img);
                });
        connect(vnc, &connect::VncConnection::rectDecoded, surface,
                [surface](int x, int y, int w, int h, const QList<quint32>& px) {
                    if (px.size() < w * h) return;
                    QImage tile(w, h, QImage::Format_ARGB32);
                    for (int r = 0; r < h; ++r)
                        for (int c = 0; c < w; ++c)
                            tile.setPixel(c, r, px[r * w + c]);
                    surface->updateRect(x, y, tile);
                });
        vnc->connectSession(resolveSecrets(session));
    } else {  // RDP
        auto* rdp = new connect::RdpConnection(surface);
        if (rdp->connectSession(resolveSecrets(session))) {
            // Pump the FreeRDP event loop and refresh the surface periodically.
            auto* timer = new QTimer(surface);
            connect(timer, &QTimer::timeout, surface, [rdp, surface, timer] {
                if (!rdp->poll()) { timer->stop(); return; }
                const QImage f = rdp->currentFrame();
                if (!f.isNull()) surface->setFrame(f);
            });
            timer->start(33);   // ~30 fps
        }
    }

    const int idx = m_tabs->addTab(surface, session.name());
    m_tabs->setCurrentIndex(idx);
    surface->setFocus();
}

TerminalWidget* MainWindow::currentPane() const {
    QWidget* cur = m_tabs->currentWidget();
    if (auto* t = qobject_cast<TerminalWidget*>(cur)) return t;
    if (cur) {
        // A focused pane inside a splitter, else the first one.
        const auto panes = cur->findChildren<TerminalWidget*>();
        for (TerminalWidget* p : panes) if (p->hasFocus()) return p;
        if (!panes.isEmpty()) return panes.first();
    }
    return nullptr;
}

void MainWindow::broadcastInput(const QByteArray& bytes) {
    for (TerminalWidget* p : m_tabs->findChildren<TerminalWidget*>())
        if (p->multiExecEnabled()) p->feedInput(bytes);
}

void MainWindow::splitQuad() {
    QWidget* cur = m_tabs->currentWidget();
    if (!cur) return;
    const int idx = m_tabs->currentIndex();
    const QString title = m_tabs->tabText(idx);

    TerminalWidget* refPane = currentPane();
    const core::Session s = refPane
        ? m_tabSessions.value(refPane, core::Session(QStringLiteral("local"), core::SessionType::Shell))
        : core::Session(QStringLiteral("local"), core::SessionType::Shell);

    // Build a 2×2 grid: an outer vertical splitter of two horizontal rows. The
    // existing pane becomes the top-left; three new panes fill the rest.
    m_tabs->removeTab(idx);
    auto* outer = new QSplitter(Qt::Vertical, m_tabs);
    auto* top = new QSplitter(Qt::Horizontal, outer);
    auto* bottom = new QSplitter(Qt::Horizontal, outer);
    top->addWidget(cur);
    top->addWidget(makePane(s));
    bottom->addWidget(makePane(s));
    bottom->addWidget(makePane(s));
    outer->addWidget(top);
    outer->addWidget(bottom);
    m_tabs->insertTab(idx, outer, title);
    m_tabs->setCurrentIndex(idx);
}

void MainWindow::splitCurrent(Qt::Orientation orientation) {
    QWidget* cur = m_tabs->currentWidget();
    if (!cur) return;
    const int idx = m_tabs->currentIndex();
    const QString title = m_tabs->tabText(idx);

    // The new pane mirrors the current pane's session (or a local shell).
    TerminalWidget* refPane = currentPane();
    const core::Session s = refPane ? m_tabSessions.value(refPane,
                                        core::Session(QStringLiteral("local"), core::SessionType::Shell))
                                    : core::Session(QStringLiteral("local"), core::SessionType::Shell);
    TerminalWidget* pane = makePane(s);

    if (auto* existing = qobject_cast<QSplitter*>(cur)) {
        existing->addWidget(pane);
    } else {
        m_tabs->removeTab(idx);
        auto* split = new QSplitter(orientation, m_tabs);
        split->addWidget(cur);
        split->addWidget(pane);
        m_tabs->insertTab(idx, split, title);
        m_tabs->setCurrentIndex(idx);
    }
    pane->setFocus();
}

void MainWindow::showSftpFor(const core::Session& session) {
    if (!m_sftpDock) {
        m_sftpPanel = new SftpPanel(this);
        m_sftpDock = new QDockWidget(QStringLiteral("SFTP"), this);
        m_sftpDock->setWidget(m_sftpPanel);
        addDockWidget(Qt::RightDockWidgetArea, m_sftpDock);
    }
    m_sftpDock->show();
    m_sftpPanel->openFor(resolveSecrets(session));   // dedicated SFTP session (blocking; LAN-fast)
}

void MainWindow::toggleMultiExec(bool on) {
    m_multiExec = on;
    // Route (or unroute) every pane's input through the broadcaster.
    for (TerminalWidget* p : m_tabs->findChildren<TerminalWidget*>()) {
        if (on) p->setInputHandler([this](const QByteArray& b) { broadcastInput(b); });
        else    p->setInputHandler(nullptr);
    }
}

void MainWindow::loadSettings() {
    QSettings qs(QStringLiteral("macXterm"), QStringLiteral("macXterm"));
    m_settings.setValue("terminal.font", qs.value("terminal.font", m_settings.fontFamily()));
    m_settings.setValue("terminal.fontSize", qs.value("terminal.fontSize", m_settings.fontSize()));
    m_settings.setValue("terminal.scheme", qs.value("terminal.scheme", m_settings.colorScheme()));
    m_settings.setValue("terminal.scrollback", qs.value("terminal.scrollback", m_settings.scrollbackLines()));
}

void MainWindow::saveSettings() {
    QSettings qs(QStringLiteral("macXterm"), QStringLiteral("macXterm"));
    qs.setValue("terminal.font", m_settings.fontFamily());
    qs.setValue("terminal.fontSize", m_settings.fontSize());
    qs.setValue("terminal.scheme", m_settings.colorScheme());
    qs.setValue("terminal.scrollback", m_settings.scrollbackLines());
}

void MainWindow::applySettings(TerminalWidget* term) {
    if (!term) return;
    term->setColorScheme(term::ColorScheme::byName(m_settings.colorScheme()));
    QFont f = term->font();
    const QString fam = m_settings.fontFamily();
    if (!fam.isEmpty()) f.setFamily(fam);
    f.setPointSize(m_settings.fontSize());
    term->setTerminalFont(f);
    term->setSyntaxHighlighting(m_syntaxHighlight);
    term->setPasteDelay(m_pasteDelay);
    term->setScrollbackLines(m_settings.scrollbackLines());
}

} // namespace macxterm::ui
