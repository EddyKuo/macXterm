#include "tunnel/Socks.h"
#include <QtTest/QtTest>
#include <sys/socket.h>
#include <unistd.h>

using namespace macxterm::tunnel;

// SOCKS4/4a/5 CONNECT negotiation, driven over a socketpair: one end plays the
// client and writes a request, the other is handed to socksNegotiate().
class TestSocks : public QObject {
    Q_OBJECT
private:
    // Run socksNegotiate on sp[1] after writing `request` to sp[0].
    bool run(const QByteArray& request, QByteArray& host, int& port, QByteArray& reply) {
        int sp[2];
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sp) != 0) return false;
        ::send(sp[0], request.constData(), request.size(), 0);
        const bool ok = socksNegotiate(sp[1], host, port);
        // Drain whatever reply the negotiator sent back.
        char buf[64];
        const ssize_t n = ::recv(sp[0], buf, sizeof(buf), MSG_DONTWAIT);
        if (n > 0) reply = QByteArray(buf, static_cast<int>(n));
        ::close(sp[0]); ::close(sp[1]);
        return ok;
    }

private slots:
    void socks5DomainConnect() {
        // VER=5 NM=1 M=0 | VER=5 CMD=1 RSV=0 ATYP=3 LEN=9 "localhost" PORT=8080
        QByteArray req;
        req.append("\x05\x01\x00", 3);
        req.append("\x05\x01\x00\x03", 4);
        req.append(static_cast<char>(9));
        req.append("localhost");
        req.append("\x1f\x90", 2);            // 0x1f90 = 8080
        QByteArray host, reply; int port = 0;
        QVERIFY(run(req, host, port, reply));
        QCOMPARE(host, QByteArray("localhost"));
        QCOMPARE(port, 8080);
        QVERIFY(reply.size() >= 2);
        QCOMPARE(static_cast<unsigned char>(reply[0]), static_cast<unsigned char>(0x05));
        QCOMPARE(static_cast<unsigned char>(reply[1]), static_cast<unsigned char>(0x00));
    }

    void socks5Ipv4Connect() {
        QByteArray req;
        req.append("\x05\x01\x00", 3);
        req.append("\x05\x01\x00\x01", 4);
        req.append("\x7f\x00\x00\x01", 4);    // 127.0.0.1
        req.append("\x00\x50", 2);            // port 80
        QByteArray host, reply; int port = 0;
        QVERIFY(run(req, host, port, reply));
        QCOMPARE(host, QByteArray("127.0.0.1"));
        QCOMPARE(port, 80);
    }

    void socks4Connect() {
        // VER=4 CMD=1 PORT=0x0050 IP=1.2.3.4 USERID="" NUL
        QByteArray req;
        req.append("\x04\x01\x00\x50", 4);
        req.append("\x01\x02\x03\x04", 4);
        req.append(static_cast<char>(0));     // empty userid terminator
        QByteArray host, reply; int port = 0;
        QVERIFY(run(req, host, port, reply));
        QCOMPARE(host, QByteArray("1.2.3.4"));
        QCOMPARE(port, 80);
        QVERIFY(reply.size() >= 2);
        QCOMPARE(static_cast<unsigned char>(reply[1]), static_cast<unsigned char>(0x5A));
    }

    void socks4aHostname() {
        // SOCKS4a: IP = 0.0.0.x (x!=0) signals a trailing hostname.
        QByteArray req;
        req.append("\x04\x01\x1f\x90", 4);    // port 8080
        req.append("\x00\x00\x00\x01", 4);    // 0.0.0.1 → 4a
        req.append(static_cast<char>(0));     // empty userid
        req.append("example.com");
        req.append(static_cast<char>(0));     // hostname terminator
        QByteArray host, reply; int port = 0;
        QVERIFY(run(req, host, port, reply));
        QCOMPARE(host, QByteArray("example.com"));
        QCOMPARE(port, 8080);
    }

    void rejectsBindCommand() {
        QByteArray req;
        req.append("\x05\x01\x00", 3);
        req.append("\x05\x02\x00\x01", 4);    // CMD=2 (BIND) — unsupported
        req.append("\x7f\x00\x00\x01", 4);
        req.append("\x00\x50", 2);
        QByteArray host, reply; int port = 0;
        QVERIFY(!run(req, host, port, reply));
    }
};

QTEST_MAIN(TestSocks)
#include "test_socks.moc"
