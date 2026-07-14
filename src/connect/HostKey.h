#pragma once
#include <QByteArray>
#include <QString>

namespace macxterm::connect {

// SSH host-key fingerprinting for known_hosts verification (Architecture §10,
// FR-007). Computes the OpenSSH-style "SHA256:base64" fingerprint from the raw
// host key blob — pure, so it is unit-testable and reused by SshConnection and
// the Store's known_host table.
class HostKey {
public:
    // Returns "SHA256:<base64-no-padding>" for the given raw key bytes.
    static QString sha256Fingerprint(const QByteArray& keyBlob);
};

} // namespace macxterm::connect
