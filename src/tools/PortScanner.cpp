#include "tools/PortScanner.h"
#include <QTcpSocket>

namespace macxterm::tools {

bool PortScanner::scanPort(const QString& host, quint16 port, int timeoutMs) {
    QTcpSocket sock;
    sock.connectToHost(host, port);
    const bool ok = sock.waitForConnected(timeoutMs);
    sock.abort();
    return ok;
}

void PortScanner::scanRange(const QString& host, quint16 first, quint16 last, int timeoutMs) {
    int open = 0;
    for (quint16 p = first; p <= last; ++p) {
        if (scanPort(host, p, timeoutMs)) {
            ++open;
            emit portOpen(p);
        }
        if (p == last) break;   // guard against quint16 wraparound at 65535
    }
    emit finished(open);
}

} // namespace macxterm::tools
