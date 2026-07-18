#pragma once
#include "core/SessionTree.h"
#include <QString>
#include <QByteArray>

namespace macxterm::core {

// Imports WinSCP saved sessions (MobaXterm parity — WinSCP is the common Windows
// SFTP/FTP client). WinSCP stores sessions either in WinSCP.ini under
// [Sessions\<name>] sections, or in the Windows registry under
// HKCU\Software\Martin Prikryl\WinSCP 2\Sessions\<name>. FSProtocol selects
// SFTP vs FTP; HostName / PortNumber / UserName map onto session fields.
//
// The INI parser is pure and platform-neutral (unit-testable); the registry
// reader is Windows-only.
class WinScpImporter {
public:
    // Parse WinSCP.ini text into a folder "Imported (WinSCP)".
    static SessionFolder parseIni(const QByteArray& iniText);

    // Read and parse a WinSCP.ini file. Empty folder on I/O error.
    static SessionFolder importFile(const QString& path);

    // Windows: enumerate the WinSCP registry sessions. Empty on non-Windows.
    static SessionFolder importFromRegistry();
};

} // namespace macxterm::core
