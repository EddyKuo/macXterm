#pragma once
#include "connect/IConnection.h"
#include "platform/Pty.h"

namespace macxterm::connect {

// Local shell session (MobaXterm's "Shell" type). On Unix this is the user's
// login shell over a PTY; the machine is already Unix so no Cygwin is bundled
// (research/MobaXterm.md §1.5, cross-platform note).
class LocalShellConnection : public IConnection {
    Q_OBJECT
public:
    explicit LocalShellConnection(QObject* parent = nullptr);

    bool connectSession(const core::Session& session) override;
    void disconnectSession() override;
    qint64 send(const QByteArray& data) override;
    void resize(int cols, int rows) override;
    Capabilities capabilities() const override { return {false, false, false, false}; }

private:
    platform::Pty m_pty;
    int m_cols = 80;
    int m_rows = 24;
};

} // namespace macxterm::connect
