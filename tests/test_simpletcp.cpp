#include "connect/SimpleTcpConnection.h"
#include "connect/IConnection.h"
#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTcpSocket>

using namespace macxterm;

class TestSimpleTcp : public QObject {
    Q_OBJECT
private slots:
    void rloginHandshakeFormat() {
        core::Session s("r", core::SessionType::Rlogin);
        s.setUsername("alice");
        const QByteArray h = connect::SimpleTcpConnection::startupHandshake(
            core::SessionType::Rlogin, s);
        // \0 alice \0 alice \0 term/speed \0  -> 4 NUL separators
        QCOMPARE(h.count('\0'), 4);
        QVERIFY(h.contains("alice"));
        QVERIFY(h.contains("xterm-256color/38400"));
        QCOMPARE(h.at(0), '\0');
    }

    void rshHandshakeFormat() {
        core::Session s("r", core::SessionType::Rsh);
        s.setUsername("bob");
        s.setParam("remotecommand", "uptime");
        const QByteArray h = connect::SimpleTcpConnection::startupHandshake(
            core::SessionType::Rsh, s);
        // stderr-port "0" \0 local \0 remote \0 command \0  -> 4 NUL separators.
        QCOMPARE(h.count('\0'), 4);
        QVERIFY(h.startsWith("0"));               // no separate stderr channel
        QVERIFY(h.contains(QByteArray("bob")));
        QVERIFY(h.contains(QByteArray("uptime")));
        QVERIFY(h.endsWith('\0'));
    }

    void ackByteOnlyForRloginAndRsh() {
        QVERIFY(connect::SimpleTcpConnection::expectsAckByte(core::SessionType::Rlogin));
        QVERIFY(connect::SimpleTcpConnection::expectsAckByte(core::SessionType::Rsh));
        QVERIFY(!connect::SimpleTcpConnection::expectsAckByte(core::SessionType::Xdmcp));
    }

    void xdmcpHasQueryMarker() {
        core::Session s("x", core::SessionType::Xdmcp);
        const QByteArray h = connect::SimpleTcpConnection::startupHandshake(
            core::SessionType::Xdmcp, s);
        QCOMPARE(h.size(), 4);
    }

    // Connect a live Rlogin session to a local server: the client end sends the
    // rlogin handshake and receives server bytes — covers connect/send/read/
    // disconnect. Uses the connection's own dataReceived to observe the round
    // trip (the server echoes), which is driven by the client's event loop.
    void connectsToLocalServer() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QObject::connect(&server, &QTcpServer::newConnection, &server, [&] {
            QTcpSocket* c = server.nextPendingConnection();
            QObject::connect(c, &QTcpSocket::readyRead, c, [c] {
                c->readAll();                    // consume the handshake / input
                c->write(QByteArray(1, '\0'));   // Rlogin status ack (success)
                c->write("server-hello");        // then the actual reply
                c->flush();
            });
        });

        core::Session s("r", core::SessionType::Rlogin);
        s.setHost(QStringLiteral("127.0.0.1"));
        s.setPort(server.serverPort());
        s.setUsername(QStringLiteral("alice"));

        connect::SimpleTcpConnection conn(core::SessionType::Rlogin);
        QByteArray clientGot;
        QObject::connect(&conn, &connect::IConnection::dataReceived, &conn,
                         [&](const QByteArray& d) { clientGot += d; });
        QVERIFY(conn.connectSession(s));

        // Wait for the client's socket to actually connect (handshake sent), then
        // for the server's reply — proving connect → send → read all fire. Fall
        // through after the timeout regardless so send()/disconnect still run.
        QTRY_VERIFY_WITH_TIMEOUT(!clientGot.isEmpty(), 5000);
        // The leading status ack byte must be swallowed, not surfaced.
        QCOMPARE(clientGot, QByteArray("server-hello"));

        conn.send("more-input");                 // exercise send() post-connect
        conn.disconnectSession();
    }
};

QTEST_GUILESS_MAIN(TestSimpleTcp)   // needs a QCoreApplication for the live socket test
#include "test_simpletcp.moc"
