#pragma once
#include "connect/IConnection.h"
#include <QStringList>

class QProcess;

namespace macxterm::connect {

// Mosh session. Per ADR_6, Mosh (GPLv3) is invoked as a SEPARATE PROCESS —
// the system `mosh` client — never linked, so the MIT project is unaffected.
// buildArgs() is pure and unit-tested; connectSession() spawns the client.
class MoshConnection : public IConnection {
    Q_OBJECT
public:
    // Build the argv for `mosh` from a Session (host, username, ssh port, key).
    static QStringList buildArgs(const core::Session& session);

    explicit MoshConnection(QObject* parent = nullptr);

    bool connectSession(const core::Session& session) override;
    void disconnectSession() override;
    qint64 send(const QByteArray& data) override;
    Capabilities capabilities() const override { return {false, false, false, false}; }

private:
    QProcess* m_proc = nullptr;
};

} // namespace macxterm::connect
