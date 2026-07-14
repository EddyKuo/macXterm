#pragma once
#include <QObject>
#include <QString>
#include <atomic>
#include <thread>
#include <memory>
#include <vector>
#include <mutex>

namespace macxterm::tools {

// Embedded SSH + SFTP server (MobaXterm's embedded SSH/SFTP daemon), built on
// libssh's server API. Password-authenticated; each session gets an interactive
// login shell (PTY) or an SFTP subsystem rooted at a served directory. A fresh
// ED25519 host key is generated in memory on start. Intended for LAN/diagnostic
// use — bind to localhost or a trusted network.
//
// Requires MACXTERM_HAVE_LIBSSH; without it start() returns false.
class SshServer : public QObject {
    Q_OBJECT
public:
    explicit SshServer(QObject* parent = nullptr);
    ~SshServer() override;

    // Start listening on `port`, authenticating with `username`/`password`, and
    // rooting SFTP at `rootDir`. Returns false on bind/keygen failure.
    bool start(quint16 port, const QString& username, const QString& password,
               const QString& rootDir);
    void stop();
    bool isRunning() const { return m_running.load(); }
    quint16 port() const { return m_port; }

private:
    void acceptLoop();

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stop{false};
    std::shared_ptr<std::thread> m_thread;
    std::vector<std::shared_ptr<std::thread>> m_sessions;   // per-connection handlers
    std::vector<int> m_sessionFds;                          // their sockets (to unblock on stop)
    std::mutex m_sessionsMutex;
    quint16 m_port = 0;
    QString m_user, m_pass, m_root;
};

} // namespace macxterm::tools
