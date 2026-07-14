#include "ui/MainWindow.h"
#include "ui/TerminalWidget.h"
#include "connect/LocalShellConnection.h"
#include "connect/SshConnection.h"
#include "connect/TelnetConnection.h"
#include "connect/SerialConnection.h"
#include "connect/MoshConnection.h"
#include "connect/SimpleTcpConnection.h"
#include "connect/RdpConnection.h"
#include "connect/VncConnection.h"
#include "ui/SessionDialog.h"
#include "ui/SettingsDialog.h"
#include "ui/TunnelDialog.h"
#include "ui/VaultDialog.h"
#include "core/Settings.h"
#include <QTabWidget>
#include <QTabBar>
#include <QTreeWidget>
#include <QDockWidget>
#include <QToolBar>
#include <QAction>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>

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

    buildToolbar();
    reloadSessionTree();
    openLocalShell();
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

    auto* multi = tb->addAction(QStringLiteral("MultiExec"));
    multi->setCheckable(true);
    connect(multi, &QAction::toggled, this, &MainWindow::toggleMultiExec);

    auto* tunnels = tb->addAction(QStringLiteral("Tunnel"));
    connect(tunnels, &QAction::triggered, this, [this] {
        TunnelDialog dlg(this);
        dlg.exec();   // tunnel manager integration in a later phase
    });

    auto* settings = tb->addAction(QStringLiteral("Settings"));
    connect(settings, &QAction::triggered, this, [this] {
        SettingsDialog dlg(core::Settings{}, this);
        dlg.exec();
    });

    auto* vault = tb->addAction(QStringLiteral("Vault"));
    connect(vault, &QAction::triggered, this, [this] {
        VaultDialog dlg(VaultDialog::Mode::Unlock, this);
        dlg.exec();
    });
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

void MainWindow::addAndSaveSession(const core::Session& s) {
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

TerminalWidget* MainWindow::openSession(const core::Session& session) {
    auto* term = new TerminalWidget(m_tabs);
    connect::IConnection* conn = nullptr;
    switch (session.type()) {
        case core::SessionType::Ssh:    conn = new connect::SshConnection(term);    break;
        case core::SessionType::Telnet: conn = new connect::TelnetConnection(term); break;
        case core::SessionType::Serial: conn = new connect::SerialConnection(term); break;
        case core::SessionType::Mosh:   conn = new connect::MoshConnection(term);   break;
        case core::SessionType::Rdp:    conn = new connect::RdpConnection(term);    break;
        case core::SessionType::Vnc:    conn = new connect::VncConnection(term);    break;
        case core::SessionType::Rsh:
        case core::SessionType::Rlogin:
        case core::SessionType::Xdmcp:
            conn = new connect::SimpleTcpConnection(session.type(), term); break;
        default:                        conn = new connect::LocalShellConnection(term); break;
    }
    term->attach(conn);
    const int idx = m_tabs->addTab(term, session.name());
    m_tabs->setCurrentIndex(idx);
    conn->connectSession(session);
    term->setFocus();
    return term;
}

void MainWindow::toggleMultiExec(bool on) {
    m_multiExec = on;
}

} // namespace macxterm::ui
