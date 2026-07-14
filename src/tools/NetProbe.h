#pragma once
#include <QString>

namespace macxterm::tools {

// Small built-in network probes (MobaXterm bundles netcat/httping/iperf-style
// tools). These are synchronous and unit-testable against a local listener.
class NetProbe {
public:
    struct Result { bool ok = false; qint64 ms = -1; QString detail; };

    // TCP connect latency to host:port (a "TCP ping"). ms = connect time.
    static Result tcpPing(const QString& host, quint16 port, int timeoutMs = 2000);

    // HTTP latency: connect + send a HEAD request + wait for the status line.
    // `url` may be "host", "host:port", or "http://host[:port][/path]".
    static Result httping(const QString& url, int timeoutMs = 3000);
};

} // namespace macxterm::tools
