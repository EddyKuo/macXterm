#include "x11/X11Server.h"
#include <QProcess>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QElapsedTimer>
#include <QThread>

namespace macxterm::x11 {

QStringList X11Server::candidateCommands() {
#if defined(Q_OS_MACOS)
    // XQuartz: prefer the app bundle (handles the launchd socket), fall back to
    // the Xquartz binary directly.
    return {
        QStringLiteral("open -a XQuartz"),
        QStringLiteral("/opt/X11/bin/Xquartz"),
        QStringLiteral("/usr/X11/bin/Xquartz"),
    };
#elif defined(Q_OS_WIN)
    return {
        QStringLiteral("C:/Program Files/VcXsrv/vcxsrv.exe :0 -multiwindow -clipboard -wgl"),
        QStringLiteral("C:/Program Files (x86)/Xming/Xming.exe :0 -multiwindow -clipboard"),
    };
#else
    // Linux: usually a server is already running; Xephyr/Xvfb as nested fallback.
    return {
        QStringLiteral("Xephyr :10 -screen 1024x768"),
        QStringLiteral("Xvfb :10 -screen 0 1024x768x24"),
    };
#endif
}

QString X11Server::currentDisplay() {
    const QByteArray d = qgetenv("DISPLAY");
    if (!d.isEmpty()) return QString::fromLocal8Bit(d);
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    return QStringLiteral(":0");
#else
    return QStringLiteral("localhost:0.0");
#endif
}

bool X11Server::isRunning() {
    const QByteArray disp = qgetenv("DISPLAY");
#if !defined(Q_OS_WIN)
    // A DISPLAY pointing at a unix socket path or ":N" — check the socket.
    QString d = QString::fromLocal8Bit(disp);
    if (d.startsWith('/')) return QFileInfo::exists(d);           // explicit socket path
    int n = 0;
    if (d.startsWith(':')) n = d.mid(1).section('.', 0, 0).toInt();
    else if (const int c = d.lastIndexOf(':'); c >= 0) n = d.mid(c + 1).section('.', 0, 0).toInt();
    if (QFileInfo::exists(QStringLiteral("/tmp/.X11-unix/X%1").arg(n))) return true;
    // macOS XQuartz uses a launchd socket under $DISPLAY only; fall through.
    return !disp.isEmpty() && d.startsWith('/');
#else
    return !disp.isEmpty();
#endif
}

bool X11Server::ensureRunning(QString& outMessage) {
    if (isRunning()) {
        outMessage = QStringLiteral("X server already running (DISPLAY=%1)").arg(currentDisplay());
        return true;
    }
    for (const QString& cmd : candidateCommands()) {
        const QStringList parts = QProcess::splitCommand(cmd);
        if (parts.isEmpty()) continue;
        const QString program = parts.first();
        // For absolute paths, skip if the binary is absent. "open"/"Xephyr" are
        // resolved via PATH so we attempt them regardless.
        if (program.contains('/') && !QFileInfo::exists(program)) continue;
        if (QProcess::startDetached(program, parts.mid(1))) {
            // Give the server a moment to create its socket.
            QElapsedTimer t; t.start();
            while (t.elapsed() < 3000) {
                if (isRunning()) {
                    outMessage = QStringLiteral("Launched X server via: %1").arg(cmd);
                    return true;
                }
                QThread::msleep(150);
            }
            outMessage = QStringLiteral("Launched %1 — waiting for it to come up").arg(program);
            return isRunning();
        }
    }
    outMessage = QStringLiteral("No local X server found. Install XQuartz (macOS), "
                                "VcXsrv (Windows), or start your X server, then retry.");
    return false;
}

} // namespace macxterm::x11
