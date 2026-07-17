#include "connect/LocalShellConnection.h"
#include <QProcessEnvironment>
#include <QFileInfo>
#include <cstdlib>

#if !defined(_WIN32)
#include <unistd.h>
#include <pwd.h>
#endif

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
        shell = env ? QString::fromLocal8Bit(env) : QString();
    }
#if !defined(_WIN32)
    // $SHELL can be missing/inconsistent when the app is launched from Finder or
    // via `open` (launchd session), so fall back to the account's login shell.
    if (shell.isEmpty()) {
        if (const passwd* pw = ::getpwuid(::getuid()); pw && pw->pw_shell && *pw->pw_shell)
            shell = QString::fromLocal8Bit(pw->pw_shell);
    }
#endif
    if (shell.isEmpty()) shell = QStringLiteral("/bin/sh");

    // Start it as a LOGIN shell (argv[0] = "-<name>") so it sources the user's
    // full profile (~/.zprofile, /etc/profile, …). This gives the same PATH and
    // environment no matter how macXterm itself was launched — otherwise a
    // Finder double-click and a CLI `open` yield different shell environments.
    const bool wantLogin = session.param("loginshell", "1") != "0";
    const QString argv0 = wantLogin ? QStringLiteral("-") + QFileInfo(shell).fileName() : QString();

    // Start in the user's home unless the session pins a directory. A fresh login
    // shell inherits macXterm's cwd otherwise — "/" when launched from Finder —
    // instead of "~". Prefer an explicit session "cwd", then $HOME, then the
    // account's home from getpwuid (robust under a launchd/Finder launch).
    QString workDir = session.param("cwd");
#if !defined(_WIN32)
    if (workDir.isEmpty()) {
        const char* home = std::getenv("HOME");
        if (home && *home) workDir = QString::fromLocal8Bit(home);
        else if (const passwd* pw = ::getpwuid(::getuid()); pw && pw->pw_dir && *pw->pw_dir)
            workDir = QString::fromLocal8Bit(pw->pw_dir);
    }
#endif

    if (!m_pty.start(shell, {}, m_cols, m_rows, argv0, workDir)) {
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
