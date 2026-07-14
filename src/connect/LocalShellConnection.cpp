#include "connect/LocalShellConnection.h"
#include <QProcessEnvironment>
#include <cstdlib>

namespace macxterm::connect {

LocalShellConnection::LocalShellConnection(QObject* parent) : IConnection(parent) {
    connect(&m_pty, &platform::Pty::readyRead, this, &IConnection::dataReceived);
    connect(&m_pty, &platform::Pty::finished, this, [this](int) {
        setState(State::Closed);
    });
}

bool LocalShellConnection::connectSession(const core::Session& session) {
    setState(State::Connecting);
    QString shell = session.param("shell");
    if (shell.isEmpty()) {
        const char* env = std::getenv("SHELL");
        shell = env ? QString::fromLocal8Bit(env) : QStringLiteral("/bin/sh");
    }
    if (!m_pty.start(shell, {}, m_cols, m_rows)) {
        setState(State::Failed);
        emit errorOccurred(QStringLiteral("Failed to start shell: %1").arg(shell));
        return false;
    }
    setState(State::Connected);
    return true;
}

void LocalShellConnection::disconnectSession() {
    m_pty.terminate();
    setState(State::Disconnected);
}

qint64 LocalShellConnection::send(const QByteArray& data) {
    return m_pty.write(data);
}

void LocalShellConnection::resize(int cols, int rows) {
    m_cols = cols;
    m_rows = rows;
    m_pty.resize(cols, rows);
}

} // namespace macxterm::connect
