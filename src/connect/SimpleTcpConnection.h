#pragma once
#include "connect/IConnection.h"
#include "core/Session.h"

class QTcpSocket;

namespace macxterm::connect {

// Shared implementation for the simple TCP-stream protocols that don't need
// option negotiation the way Telnet does: RSH, Rlogin, and the XDMCP query
// bootstrap. Each maps a SessionType to a default port and a small startup
// handshake string; bytes then flow straight through to the terminal.
class SimpleTcpConnection : public IConnection {
    Q_OBJECT
public:
    explicit SimpleTcpConnection(core::SessionType type, QObject* parent = nullptr);

    bool connectSession(const core::Session& session) override;
    void disconnectSession() override;
    qint64 send(const QByteArray& data) override;
    Capabilities capabilities() const override { return {false, false, false, false}; }

    // Pure: the initial bytes sent on connect for this protocol (e.g. Rlogin's
    // NUL-delimited "\0localuser\0remoteuser\0term/speed\0", or Rsh's
    // "stderrport\0localuser\0remoteuser\0command\0"). Exposed for tests.
    static QByteArray startupHandshake(core::SessionType type, const core::Session& session);

    // Whether this protocol begins with a server-sent one-byte status ack that
    // must be swallowed rather than shown in the terminal (Rlogin and Rsh both
    // reply with a leading 0x00 = success). Pure/testable.
    static bool expectsAckByte(core::SessionType type);

private slots:
    void onReadyRead();

private:
    core::SessionType m_type;
    QTcpSocket* m_sock = nullptr;
    bool m_awaitingAck = false;   // strip the first server byte (Rlogin/Rsh status)
};

} // namespace macxterm::connect
