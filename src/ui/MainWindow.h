#pragma once
#include "core/SessionTree.h"
#include "core/Store.h"
#include "core/Settings.h"
#include "tunnel/SshTunnel.h"
#include "core/Macro.h"
#include <QMainWindow>
#include <QHash>

class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QDockWidget;

namespace macxterm::ui {

class TerminalWidget;
class SftpPanel;

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

public slots:
    void toggleMultiExec(bool on);

private:
    void buildToolbar();
    void buildMenus();
    void importSshConfig();
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
    void applySettings(TerminalWidget* term);   // apply scheme+font to one pane

    void showSftpFor(const core::Session& session);
    void openGraphicalSession(const core::Session& session);  // RDP/VNC surface tab
    TerminalWidget* makePane(const core::Session& session);   // create+connect, no tab
    TerminalWidget* currentPane() const;                      // focused pane of current tab
    void broadcastInput(const QByteArray& bytes);             // MultiExec fan-out

    QTabWidget* m_tabs = nullptr;
    QTreeWidget* m_tree = nullptr;
    QDockWidget* m_sftpDock = nullptr;
    SftpPanel* m_sftpPanel = nullptr;
    core::SessionFolder m_sessions{QStringLiteral("Sessions")};
    core::Store m_store;
    core::Settings m_settings;
    QHash<QWidget*, core::Session> m_tabSessions;   // active session per tab
    QList<tunnel::SshTunnel*> m_tunnels;            // live tunnels
    core::Macro m_macro;                            // last recorded macro
    bool m_recordingMacro = false;
    bool m_multiExec = false;
};

} // namespace macxterm::ui
