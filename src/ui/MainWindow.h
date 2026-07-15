#pragma once
#include "core/SessionTree.h"
#include "core/Store.h"
#include "core/Settings.h"
#include "tunnel/SshTunnel.h"
#include "core/Macro.h"
#include "core/CredentialVault.h"
#include "core/ShortcutRegistry.h"
#include <QMainWindow>
#include <QHash>

class QAction;

class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QDockWidget;

namespace macxterm::ui {

class TerminalWidget;
class SftpPanel;
class RemoteMonitorBar;

// Main application window: session tree sidebar + tabbed terminal area + toolbar
// (Architecture §3, UI_Spec). Phase 1 wires local shell + SSH tabs and MultiExec.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Open a new tab running the local shell.
    TerminalWidget* openLocalShell();
    // Open a new tab for a saved session (SSH/local for now).
    TerminalWidget* openSession(const core::Session& session);
    // Split the current tab, adding another pane (same session or local shell).
    void splitCurrent(Qt::Orientation orientation);
    void splitQuad();   // replace current tab with a 2×2 grid of the session

public slots:
    void toggleMultiExec(bool on);

private:
    void buildToolbar();
    void buildMenus();
    void importSshConfig();
    void exportSessions();
    void importSharedSessions();
    void detachCurrentTab();
    void toggleMacroRecording();
    void playMacro();
    void reloadSessionTree();
    void onTreeActivated(QTreeWidgetItem* item, int column);
    void showTreeContextMenu(const QPoint& pos);
    void addAndSaveSession(const core::Session& s);
    void deleteSession(const QString& name);
    void persistSessions();
    void loadSettings();
    void saveSettings();
    void buildShortcuts();          // create registry-driven QActions
    void applyShortcuts();          // push registry sequences onto the actions
    void editShortcuts();           // open the shortcut editor dialog
    void openVault();                               // create/unlock the vault
    QString vaultPath() const;
    core::Session resolveSecrets(core::Session s) const;  // inject vault password
    // If the session has a "gateway" param, open a local SSH tunnel through the
    // gateway to the target and return a copy pointing at 127.0.0.1:localport
    // (used to route RDP/VNC/etc. through an SSH jump host). Otherwise returns s.
    core::Session resolveGateway(const core::Session& s);
    void applySettings(TerminalWidget* term);   // apply scheme+font to one pane

    void showSftpFor(const core::Session& session);
    void showFtpFor(const core::Session& session);
    // Hide + disconnect the SFTP/FTP docks when no remaining tab still needs them
    // (called after a tab closes, so the browser doesn't outlive its session).
    void syncRemoteDocks();
    void openGraphicalSession(const core::Session& session);  // RDP/VNC surface tab
    TerminalWidget* makePane(const core::Session& session);   // create+connect, no tab
    TerminalWidget* currentPane() const;                      // focused pane of current tab
    void broadcastInput(const QByteArray& bytes);             // MultiExec fan-out

    QTabWidget* m_tabs = nullptr;
    QTreeWidget* m_tree = nullptr;
    QDockWidget* m_sftpDock = nullptr;
    SftpPanel* m_sftpPanel = nullptr;
    QDockWidget* m_ftpDock = nullptr;
    SftpPanel* m_ftpPanel = nullptr;
    RemoteMonitorBar* m_monitor = nullptr;
    core::SessionFolder m_sessions{QStringLiteral("Sessions")};
    core::Store m_store;
    core::Settings m_settings;
    QHash<QWidget*, core::Session> m_tabSessions;   // active session per tab
    QList<tunnel::SshTunnel*> m_tunnels;            // live tunnels
    core::Macro m_macro;                            // last recorded macro
    bool m_recordingMacro = false;
    core::CredentialVault m_vault;
    QString m_masterPassword;
    bool m_vaultUnlocked = false;
    core::ShortcutRegistry m_shortcuts;
    QHash<QString, QAction*> m_shortcutActions;
    bool m_multiExec = false;
    bool m_syntaxHighlight = false;
    int m_pasteDelay = 0;
};

} // namespace macxterm::ui
