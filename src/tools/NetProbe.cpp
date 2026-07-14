#include "tools/NetProbe.h"
#include <QTcpSocket>
#include <QElapsedTimer>
#include <QUrl>

namespace macxterm::tools {

NetProbe::Result NetProbe::tcpPing(const QString& host, quint16 port, int timeoutMs) {
    Result r;
    QTcpSocket sock;
    QElapsedTimer t; t.start();
    sock.connectToHost(host, port);
    if (sock.waitForConnected(timeoutMs)) {
        r.ok = true;
        r.ms = t.elapsed();
        r.detail = QStringLiteral("connected to %1:%2").arg(host).arg(port);
        sock.disconnectFromHost();
    } else {
        r.detail = sock.errorString();
    }
    return r;
}

NetProbe::Result NetProbe::httping(const QString& url, int timeoutMs) {
    Result r;
    // Normalize into host/port/path.
    QString u = url;
    if (!u.contains(QStringLiteral("://"))) u.prepend(QStringLiteral("http://"));
    const QUrl parsed(u);
    const QString host = parsed.host();
    const quint16 port = parsed.port() > 0 ? static_cast<quint16>(parsed.port()) : 80;
    const QString path = parsed.path().isEmpty() ? QStringLiteral("/") : parsed.path();
    if (host.isEmpty()) { r.detail = QStringLiteral("bad URL"); return r; }

    QTcpSocket sock;
    QElapsedTimer t; t.start();
    sock.connectToHost(host, port);
    if (!sock.waitForConnected(timeoutMs)) { r.detail = sock.errorString(); return r; }

    // GET (not HEAD) for the broadest server compatibility; we only read the
    // status line to measure latency.
    const QByteArray req = "GET " + path.toUtf8() + " HTTP/1.0\r\nHost: " + host.toUtf8() +
                           "\r\nConnection: close\r\n\r\n";
    sock.write(req);
    sock.flush();
    if (!sock.waitForReadyRead(timeoutMs)) { r.detail = QStringLiteral("no response"); return r; }
    const QByteArray line = sock.readLine().trimmed();
    r.ok = line.startsWith("HTTP/");
    r.ms = t.elapsed();
    r.detail = QString::fromLatin1(line);
    sock.disconnectFromHost();
    return r;
}

} // namespace macxterm::tools
