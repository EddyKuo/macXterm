#include "tunnel/LocalForwarder.h"
#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTcpSocket>

using namespace macxterm::tunnel;

// End-to-end loopback test of the tunnel data path: forwarder listens locally
// and relays to an echo server, exactly as an SSH local tunnel would (with the
// echo server standing in for the remote target reached via the SSH channel).
class TestForwarder : public QObject {
    Q_OBJECT
private slots:
    void relaysBytesToTarget() {
        // Echo server as the "remote target".
        QTcpServer echo;
        QVERIFY(echo.listen(QHostAddress::LocalHost));
        connect(&echo, &QTcpServer::newConnection, [&] {
            QTcpSocket* s = echo.nextPendingConnection();
            connect(s, &QTcpSocket::readyRead, [s] { s->write(s->readAll()); });
        });

        LocalForwarder fwd;
        QVERIFY(fwd.start("127.0.0.1", 0, "127.0.0.1", echo.serverPort()));
        QVERIFY(fwd.isListening());

        // Client connects to the forwarder's local port. Accumulate the reply
        // and let QTRY_VERIFY spin the shared event loop so every socket in this
        // single thread (client, forwarder, upstream, echo) is serviced.
        QTcpSocket client;
        QByteArray got;
        connect(&client, &QTcpSocket::readyRead, [&] { got += client.readAll(); });
        client.connectToHost(QHostAddress::LocalHost, fwd.listenPort());
        QVERIFY(client.waitForConnected(2000));
        client.write("ping-through-tunnel");
        QTRY_VERIFY_WITH_TIMEOUT(got.contains("ping-through-tunnel"), 4000);
    }

    void startFailsOnBadBind() {
        LocalForwarder fwd;
        // 0.0.0.0 with an already-bound low port is unreliable to force; instead
        // bind twice to the same ephemeral port to force a collision.
        QTcpServer occupy;
        QVERIFY(occupy.listen(QHostAddress::LocalHost));
        const quint16 p = occupy.serverPort();
        QVERIFY(!fwd.start("127.0.0.1", p, "127.0.0.1", 9999));  // port in use
    }
};

QTEST_GUILESS_MAIN(TestForwarder)
#include "test_forwarder.moc"
