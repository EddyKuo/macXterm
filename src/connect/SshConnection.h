#pragma once
#include "connect/IConnection.h"
#include <vector>

class QSocketNotifier;
typedef struct _LIBSSH2_SESSION LIBSSH2_SESSION;
typedef struct _LIBSSH2_CHANNEL LIBSSH2_CHANNEL;

namespace macxterm::connect {

// SSH session via libssh2 (BSD, ADR_6). Opens a PTY + shell channel; the same
// session is designed to also carry SFTP / tunnel / X11 channels (Architecture
// §6.3) — those are added in later phases. Reports SFTP/x11/tunnel capabilities.
//
// Auth: password (from Session param "password", normally injected by the
// CredentialVault) or private key (param "keyfile" + optional "passphrase").
class SshConnection : public IConnection {
    Q_OBJECT
public:
    explicit SshConnection(QObject* parent = nullptr);
    ~SshConnection() override;

    bool connectSession(const core::Session& session) override;
    void disconnectSession() override;
    qint64 send(const QByteArray& data) override;
    void resize(int cols, int rows) override;
    Capabilities capabilities() const override { return {true, true, true, false}; }

    // X11 forwarding: invoked from libssh2's C callback when the remote app
    // opens an X connection — connect the local X server and relay both ways.
    void acceptX11(LIBSSH2_CHANNEL* channel);

private slots:
    void onSocketReadable();
    void onX11SocketReadable();

private:
    bool doHandshakeAndAuth(const core::Session& session);
    void cleanup();
    void pumpX11();
    // SSH gateway / jump host (ProxyJump): connect the gateway, open a
    // direct-tcpip channel to the target, and return a local socket fd (backed
    // by a relay thread) that the target session runs its handshake over.
    // Returns -1 on failure. No-op unless the session has a "gateway" param.
    int openViaJump(const core::Session& session);

    struct X11Fwd { LIBSSH2_CHANNEL* chan; int xsock; QSocketNotifier* notifier; };
    struct JumpRelay;   // defined in the .cpp

    int m_sock = -1;
    LIBSSH2_SESSION* m_session = nullptr;
    LIBSSH2_CHANNEL* m_channel = nullptr;
    QSocketNotifier* m_notifier = nullptr;
    std::vector<X11Fwd> m_x11;
    JumpRelay* m_jump = nullptr;
    int m_cols = 80;
    int m_rows = 24;
};

} // namespace macxterm::connect
