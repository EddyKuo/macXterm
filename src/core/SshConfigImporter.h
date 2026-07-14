#pragma once
#include "core/SessionTree.h"
#include <QString>
#include <QByteArray>

namespace macxterm::core {

// Imports OpenSSH client config (~/.ssh/config) into a SessionFolder, giving
// macXterm parity with MobaXterm's "import sessions" (research §1.7). Each
// `Host` block (excluding wildcard patterns) becomes an SSH Session; HostName /
// Port / User / IdentityFile / ProxyJump map onto session fields/params.
class SshConfigImporter {
public:
    // Parse config text into a folder named "Imported (ssh_config)".
    static SessionFolder parse(const QByteArray& configText);

    // Convenience: read and parse a file. Returns an empty folder on I/O error.
    static SessionFolder importFile(const QString& path);
};

} // namespace macxterm::core
