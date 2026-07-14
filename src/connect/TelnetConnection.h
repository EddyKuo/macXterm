#pragma once
#include "connect/IConnection.h"
#include "connect/TelnetProtocol.h"

class QTcpSocket;

namespace macxterm::connect {

// Telnet client (RFC 854) over QTcpSocket. IAC negotiation is delegated to
// TelnetProtocol; application data flows to/from the terminal.
class TelnetConnection : public IConnection {
    Q_OBJECT
public:
    explicit TelnetConnection(QObject* parent = nullptr);

    bool connectSession(const core::Session& session) override;
    void disconnectSession() override;
    qint64 send(const QByteArray& data) override;
    Capabilities capabilities() const override { return {false, false, false, false}; }

private slots:
    void onReadyRead();

private:
    QTcpSocket* m_sock = nullptr;
    TelnetProtocol m_proto;
};

} // namespace macxterm::connect
