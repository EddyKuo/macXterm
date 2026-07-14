#pragma once
#include <QByteArray>

namespace macxterm::tunnel {

// Minimal SOCKS4/4a/5 server-side CONNECT negotiation, used by dynamic (-D) SSH
// tunnels. Reads the client's greeting/request from a blocking socket fd, writes
// the success reply, and returns the requested target host/port. Only the
// CONNECT command and the "no authentication" SOCKS5 method are supported.
// Returns false on a malformed or unsupported request. Factored out of SshTunnel
// so the protocol parsing is unit-testable over a socketpair (Windows: no-op).
bool socksNegotiate(int fd, QByteArray& outHost, int& outPort);

} // namespace macxterm::tunnel
