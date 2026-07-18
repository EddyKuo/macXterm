#pragma once
#include <QString>

namespace macxterm::core {

// MobaXterm/Cygwin-style path translation between Windows paths and the POSIX
// "/drives/<letter>/..." convention used by the bundled local Unix terminal
// (the "cygpath" feature). Pure string logic — platform-neutral and unit-tested,
// so it is available (and testable) on every platform even though the userland
// terminal itself is Windows-only.
//
//   C:\Users\me        <->  /drives/c/Users/me
//   D:\data\x.txt      <->  /drives/d/data/x.txt
//   C:\                <->  /drives/c
//
// Non-absolute or already-POSIX inputs are returned normalised (backslashes →
// forward slashes) but otherwise unchanged, so the helpers are idempotent-ish and
// safe to apply defensively.
class CygPath {
public:
    // Windows path → POSIX "/drives/<letter>/...". A UNC path (\\host\share) maps
    // to //host/share. Paths without a drive letter are just slash-normalised.
    static QString windowsToPosix(const QString& winPath);

    // POSIX "/drives/<letter>/..." → Windows "<LETTER>:\...". A leading "/cygdrive/"
    // is accepted as an alias for "/drives/". Inputs not under the drives root are
    // returned with forward slashes (best-effort).
    static QString posixToWindows(const QString& posixPath);

    // The mount root the local terminal exposes Windows drives under.
    static QString drivesRoot() { return QStringLiteral("/drives"); }
};

} // namespace macxterm::core
