#pragma once
#include <QString>
#include <QStringList>

namespace macxterm::x11 {

// Manages the *platform* X server used to display SSH-forwarded X applications.
// macXterm does not implement or bundle an X server; instead it locates and
// launches the one installed on the host (XQuartz on macOS, VcXsrv/Xming on
// Windows, the native X on Linux) and reports the DISPLAY to advertise. This is
// the pragmatic cross-platform path (research §4): integrate an existing X.Org
// rather than reimplement one.
class X11Server {
public:
    // Candidate X-server launch commands for the current platform, most
    // preferred first. Pure (used to decide what to try) — testable.
    static QStringList candidateCommands();

    // Is a local X server already reachable? Checks $DISPLAY and, on Unix, the
    // presence of an X11 socket.
    static bool isRunning();

    // The DISPLAY string a forwarded app should use (from $DISPLAY, or the
    // conventional local value).
    static QString currentDisplay();

    // Ensure a local X server is running: if one is already reachable, return
    // true; otherwise try to launch an installed platform X server. `outMessage`
    // receives a human-readable status. Returns true if a server is (now)
    // available. Non-blocking beyond a short readiness wait.
    static bool ensureRunning(QString& outMessage);
};

} // namespace macxterm::x11
