#pragma once
#include "core/SessionTree.h"
#include "core/Store.h"
#include "core/Settings.h"
#include <QMainWindow>

class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace macxterm::ui {

class TerminalWidget;

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

public slots:
    void toggleMultiExec(bool on);

private:
    void buildToolbar();
    void reloadSessionTree();
    void onTreeActivated(QTreeWidgetItem* item, int column);
    void showTreeContextMenu(const QPoint& pos);
    void addAndSaveSession(const core::Session& s);
    void deleteSession(const QString& name);
    void persistSessions();
    void loadSettings();
    void saveSettings();
    void applySettings(TerminalWidget* term);   // apply scheme+font to one pane

    QTabWidget* m_tabs = nullptr;
    QTreeWidget* m_tree = nullptr;
    core::SessionFolder m_sessions{QStringLiteral("Sessions")};
    core::Store m_store;
    core::Settings m_settings;
    bool m_multiExec = false;
};

} // namespace macxterm::ui
