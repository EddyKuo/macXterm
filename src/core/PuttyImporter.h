#pragma once
#include "core/SessionTree.h"
#include <QString>
#include <QByteArray>

namespace macxterm::core {

// Imports PuTTY saved sessions into a SessionFolder (MobaXterm parity — PuTTY is
// the de-facto SSH client on Windows). PuTTY stores each session as a set of
// key/value settings: in the Windows registry under
// HKCU\Software\SimonTatham\PuTTY\Sessions\<name>, or (Unix PuTTY) as a file per
// session under ~/.putty/sessions/<name>. HostName / PortNumber / UserName /
// Protocol / PublicKeyFile / ProxyHost map onto macXterm session fields.
//
// The value parser is pure and platform-neutral (unit-testable); the registry
// reader is Windows-only.
class PuttyImporter {
public:
    // Parse one PuTTY session's "Key\value" / "Key=value" settings text into a
    // Session named `name`. Protocol maps to the session type (ssh/telnet/rlogin/
    // raw→telnet/serial).
    static Session parseSession(const QString& name, const QByteArray& settings);

    // Windows: enumerate HKCU\Software\SimonTatham\PuTTY\Sessions and build a
    // folder "Imported (PuTTY)". Empty folder on non-Windows or if PuTTY is absent.
    static SessionFolder importFromRegistry();

    // Cross-platform: read a directory of PuTTY session files (Unix PuTTY layout,
    // ~/.putty/sessions). Each file is one session; the file name is the session
    // name (percent-decoded).
    static SessionFolder importFromDir(const QString& dir);
};

} // namespace macxterm::core
