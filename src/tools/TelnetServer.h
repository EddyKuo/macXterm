#pragma once
#include <QObject>
#include <QHash>

class QTcpServer;
class QTcpSocket;

namespace macxterm::platform { class Pty; }

namespace macxterm::tools {

// A minimal embedded Telnet server (MobaXterm's embedded TELNET daemon). Each
// accepted client gets its own pseudo-terminal running the local login shell;
// bytes are relayed both ways. Intended for LAN/diagnostic use — there is no
// authentication, so bind to localhost or a trusted network. POSIX only (uses
// platform::Pty, whose Windows backend is a separate task).
class TelnetServer : public QObject {
    Q_OBJECT
public:
    explicit TelnetServer(QObject* parent = nullptr);
    ~TelnetServer() override;

    bool start(quint16 port);
    void stop();
    bool isRunning() const;
    quint16 port() const;

private slots:
    void onNewConnection();

private:
    QTcpServer* m_server = nullptr;
    QHash<QTcpSocket*, platform::Pty*> m_sessions;
};

} // namespace macxterm::tools
