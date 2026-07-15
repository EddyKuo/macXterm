#include "connect/XdmcpProtocol.h"
#include "connect/XdmcpConnection.h"
#include <QtTest/QtTest>
#include <QUdpSocket>
#include <QScopeGuard>
#include <QSignalSpy>

using namespace macxterm::connect::xdmcp;
using macxterm::connect::XdmcpConnection;

// Build a Willing / Accept / rejection packet the way a display manager would,
// for the loopback fixtures below.
static QByteArray makeWilling(const QByteArray& host, const QByteArray& status) {
    QByteArray p;
    auto u16 = [&](int v){ p.append(char((v>>8)&0xff)); p.append(char(v&0xff)); };
    auto arr = [&](const QByteArray& a){ p.append(char(a.size())); p.append(a); };
    u16(1); u16(Willing);
    u16(1 + 1 + host.size() + 1 + status.size());
    arr(QByteArray()); arr(host); arr(status);
    return p;
}
static QByteArray makeAccept(quint32 sessionId) {
    QByteArray p;
    auto u16 = [&](int v){ p.append(char((v>>8)&0xff)); p.append(char(v&0xff)); };
    auto u32 = [&](quint32 v){ p.append(char((v>>24)&0xff)); p.append(char((v>>16)&0xff));
                               p.append(char((v>>8)&0xff)); p.append(char(v&0xff)); };
    auto arr = [&](const QByteArray& a){ p.append(char(a.size())); p.append(a); };
    u16(1); u16(Accept);
    u16(4 + 1 + 1 + 1 + 1);   // sessionId + 4 empty ARRAY8s
    u32(sessionId);
    arr({}); arr({}); arr({}); arr({});
    return p;
}
static QByteArray makeOpcode(quint16 opcode) {
    QByteArray p;
    auto u16 = [&](int v){ p.append(char((v>>8)&0xff)); p.append(char(v&0xff)); };
    u16(1); u16(opcode); u16(0);
    return p;
}

class TestXdmcp : public QObject {
    Q_OBJECT
private slots:
    void encodeQueryNoAuth() {
        const QByteArray q = encodeQuery({});
        // header(6): version=1, opcode=2 (Query), length=1; body = count byte 0.
        QCOMPARE(q.size(), 7);
        const Header h = parseHeader(q);
        QVERIFY(h.valid);
        QCOMPARE(h.version, quint16(1));
        QCOMPARE(h.opcode, quint16(Query));
        QCOMPARE(h.length, quint16(1));
        QCOMPARE(static_cast<quint8>(q[6]), quint8(0));   // zero auth names
    }

    void encodeQueryWithAuthNames() {
        const QByteArray q = encodeQuery({QByteArray("MIT-MAGIC-COOKIE-1")});
        const Header h = parseHeader(q);
        QCOMPARE(h.opcode, quint16(Query));
        // body = count(1) + array8(len byte + 18 bytes) = 1 + 1 + 18 = 20.
        QCOMPARE(h.length, quint16(20));
        QCOMPARE(static_cast<quint8>(q[6]), quint8(1));    // one name
        QCOMPARE(static_cast<quint8>(q[7]), quint8(18));   // name length
        QVERIFY(q.mid(8, 18) == QByteArray("MIT-MAGIC-COOKIE-1"));
    }

    void parseHeaderTooShort() {
        QVERIFY(!parseHeader(QByteArray("\0\1\0", 3)).valid);
    }

    void parseWillingRoundTrip() {
        // Build a Willing packet by hand and parse it back.
        QByteArray pkt;
        auto u16 = [&](int v){ pkt.append(char((v>>8)&0xff)); pkt.append(char(v&0xff)); };
        auto arr = [&](const QByteArray& a){ pkt.append(char(a.size())); pkt.append(a); };
        const QByteArray auth("");
        const QByteArray host("xdm.example");
        const QByteArray status("Willing to manage");
        u16(1); u16(Willing);
        const int len = 1 + auth.size() + 1 + host.size() + 1 + status.size();
        u16(len);
        arr(auth); arr(host); arr(status);

        const WillingInfo w = parseWilling(pkt);
        QVERIFY(w.valid);
        QCOMPARE(w.hostname, host);
        QCOMPARE(w.status, status);
        QVERIFY(w.authenticationName.isEmpty());
    }

    void parseWillingRejectsWrongOpcode() {
        // A Query packet is not a Willing.
        QVERIFY(!parseWilling(encodeQuery({})).valid);
    }

    void parseWillingRejectsTruncated() {
        QByteArray pkt;
        auto u16 = [&](int v){ pkt.append(char((v>>8)&0xff)); pkt.append(char(v&0xff)); };
        u16(1); u16(Willing); u16(10);
        pkt.append(char(5));            // claims a 5-byte array but supplies none
        QVERIFY(!parseWilling(pkt).valid);
    }

    // End-to-end over loopback UDP: a tiny "display manager" answers a Query
    // with a Willing, and the client parses it — proving the codec works on the
    // wire, not just in memory.
    void queryWillingOverUdp() {
        QUdpSocket server;
        QVERIFY(server.bind(QHostAddress::LocalHost, 0));
        auto guard = qScopeGuard([&]{ server.close(); });

        QUdpSocket client;
        QVERIFY(client.bind(QHostAddress::LocalHost, 0));

        // Server: on datagram, verify it's a Query and reply with Willing.
        bool sawQuery = false;
        QObject::connect(&server, &QUdpSocket::readyRead, &server, [&] {
            while (server.hasPendingDatagrams()) {
                QByteArray dg(server.pendingDatagramSize(), '\0');
                QHostAddress from; quint16 fromPort = 0;
                server.readDatagram(dg.data(), dg.size(), &from, &fromPort);
                if (parseHeader(dg).opcode == Query) sawQuery = true;
                QByteArray reply;
                auto u16 = [&](int v){ reply.append(char((v>>8)&0xff)); reply.append(char(v&0xff)); };
                auto arr = [&](const QByteArray& a){ reply.append(char(a.size())); reply.append(a); };
                QByteArray host("xdm.local"), status("Willing to manage");
                u16(1); u16(Willing);
                u16(1 + 1 + host.size() + 1 + status.size());
                arr(QByteArray()); arr(host); arr(status);
                server.writeDatagram(reply, from, fromPort);
            }
        });

        client.writeDatagram(encodeQuery({}), QHostAddress::LocalHost, server.localPort());

        QVERIFY(QTest::qWaitFor([&]{ return client.hasPendingDatagrams(); }, 5000));
        QByteArray dg(client.pendingDatagramSize(), '\0');
        client.readDatagram(dg.data(), dg.size());
        const WillingInfo w = parseWilling(dg);
        QVERIFY(sawQuery);
        QVERIFY(w.valid);
        QCOMPARE(w.hostname, QByteArray("xdm.local"));
    }

    // ── S32-02: Request encoding, Accept parsing, rejection detection ──

    void encodeRequestAndReparseHeader() {
        RequestParams rp;
        rp.displayNumber = 0;
        rp.connectionTypes = {0};
        rp.connectionAddresses = { QByteArray("\x7f\x00\x00\x01", 4) };
        rp.authorizationNames = { QByteArray("MIT-MAGIC-COOKIE-1") };
        const QByteArray pkt = encodeRequest(rp);
        const Header h = parseHeader(pkt);
        QVERIFY(h.valid);
        QCOMPARE(h.opcode, quint16(Request));
        QCOMPARE(h.length, quint16(pkt.size() - 6));   // header excluded
        QVERIFY(pkt.contains(QByteArray("MIT-MAGIC-COOKIE-1")));
    }

    void parseAcceptRoundTrip() {
        const AcceptInfo a = parseAccept(makeAccept(0xDEADBEEF));
        QVERIFY(a.valid);
        QCOMPARE(a.sessionId, quint32(0xDEADBEEF));
    }

    void parseAcceptRejectsWrongOpcode() {
        QVERIFY(!parseAccept(makeWilling("h", "s")).valid);
    }

    void rejectionOpcodes() {
        QVERIFY(isRejection(Decline));
        QVERIFY(isRejection(Refuse));
        QVERIFY(isRejection(Failed));
        QVERIFY(!isRejection(Accept));
        QVERIFY(!isRejection(Willing));
    }

    // Full state machine over loopback UDP: a stub display manager answers
    // Query→Willing and Request→Accept; XdmcpConnection must emit accepted with
    // the right session id, and the manager must receive a well-formed Request.
    void handshakeQueryRequestAccept() {
        QUdpSocket xdm;
        QVERIFY(xdm.bind(QHostAddress::LocalHost, 0));
        auto guard = qScopeGuard([&]{ xdm.close(); });

        bool gotWellFormedRequest = false;
        QObject::connect(&xdm, &QUdpSocket::readyRead, &xdm, [&] {
            while (xdm.hasPendingDatagrams()) {
                QByteArray dg(xdm.pendingDatagramSize(), '\0');
                QHostAddress from; quint16 fromPort = 0;
                xdm.readDatagram(dg.data(), dg.size(), &from, &fromPort);
                const Header h = parseHeader(dg);
                if (h.opcode == Query) {
                    xdm.writeDatagram(makeWilling("xdm.local", "Willing to manage"), from, fromPort);
                } else if (h.opcode == Request) {
                    if (dg.contains(QByteArray("MIT-MAGIC-COOKIE-1"))) gotWellFormedRequest = true;
                    xdm.writeDatagram(makeAccept(0x12345678), from, fromPort);
                }
            }
        });

        XdmcpConnection conn;
        QSignalSpy willingSpy(&conn, &XdmcpConnection::willing);
        QSignalSpy acceptSpy(&conn, &XdmcpConnection::accepted);
        QVERIFY(conn.start(QStringLiteral("127.0.0.1"), xdm.localPort()));

        QVERIFY(QTest::qWaitFor([&]{ return acceptSpy.count() > 0; }, 5000));
        QCOMPARE(willingSpy.count(), 1);
        QCOMPARE(acceptSpy.count(), 1);
        QCOMPARE(acceptSpy.first().at(0).toUInt(), quint32(0x12345678));
        QVERIFY(gotWellFormedRequest);
    }

    // Rejection path: the manager Declines the Request.
    void handshakeDeclineRejects() {
        QUdpSocket xdm;
        QVERIFY(xdm.bind(QHostAddress::LocalHost, 0));
        auto guard = qScopeGuard([&]{ xdm.close(); });

        QObject::connect(&xdm, &QUdpSocket::readyRead, &xdm, [&] {
            while (xdm.hasPendingDatagrams()) {
                QByteArray dg(xdm.pendingDatagramSize(), '\0');
                QHostAddress from; quint16 fromPort = 0;
                xdm.readDatagram(dg.data(), dg.size(), &from, &fromPort);
                const Header h = parseHeader(dg);
                if (h.opcode == Query)
                    xdm.writeDatagram(makeWilling("xdm", "ok"), from, fromPort);
                else if (h.opcode == Request)
                    xdm.writeDatagram(makeOpcode(Decline), from, fromPort);
            }
        });

        XdmcpConnection conn;
        QSignalSpy rejectSpy(&conn, &XdmcpConnection::rejected);
        QSignalSpy acceptSpy(&conn, &XdmcpConnection::accepted);
        QVERIFY(conn.start(QStringLiteral("127.0.0.1"), xdm.localPort()));

        QVERIFY(QTest::qWaitFor([&]{ return rejectSpy.count() > 0; }, 5000));
        QCOMPARE(rejectSpy.first().at(0).toUInt(), quint32(Decline));
        QCOMPARE(acceptSpy.count(), 0);
    }
};

QTEST_GUILESS_MAIN(TestXdmcp)
#include "test_xdmcp.moc"
