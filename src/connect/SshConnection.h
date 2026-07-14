#pragma once
#include "connect/IConnection.h"

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

private slots:
    void onSocketReadable();

private:
    bool doHandshakeAndAuth(const core::Session& session);
    void cleanup();

    int m_sock = -1;
    LIBSSH2_SESSION* m_session = nullptr;
    LIBSSH2_CHANNEL* m_channel = nullptr;
    QSocketNotifier* m_notifier = nullptr;
    int m_cols = 80;
    int m_rows = 24;
};

} // namespace macxterm::connect
