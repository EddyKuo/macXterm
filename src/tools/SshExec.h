#pragma once
#include "core/Session.h"
#include <QString>
#include <QByteArray>

typedef struct _LIBSSH2_SESSION LIBSSH2_SESSION;

namespace macxterm::tools {

// Runs one-shot commands over a dedicated SSH session (used by the remote
// monitor to sample /proc without disturbing the interactive shell). Blocking;
// keep polling intervals modest and prefer a worker thread for slow endpoints.
class SshExec {
public:
    ~SshExec();
    bool connectSession(const core::Session& session);
    void disconnectSession();
    bool isConnected() const { return m_session != nullptr; }

    // Run a command; returns its combined stdout (empty on error).
    QByteArray run(const QString& command);

private:
    LIBSSH2_SESSION* m_session = nullptr;
    int m_sock = -1;
};

} // namespace macxterm::tools
