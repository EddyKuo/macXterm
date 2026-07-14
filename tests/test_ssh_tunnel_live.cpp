#include "tunnel/SshTunnel.h"
#include "tunnel/Tunnel.h"
#include "core/Session.h"
#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <cstdlib>

using namespace macxterm;

// Guarded-live test: forward a local port through a real SSH direct-tcpip
// channel to a local echo server, and verify bytes round-trip end to end.
// Runs when MACXTERM_SSH_TEST_HOST/_USER (+ _KEY/_PASS) are set.
class TestSshTunnelLive : public QObject {
    Q_OBJECT
private slots:
    void forwardsThroughSshChannel() {
        const char* host = std::getenv("MACXTERM_SSH_TEST_HOST");
        const char* user = std::getenv("MACXTERM_SSH_TEST_USER");
        if (!host || !user) QSKIP("Set MACXTERM_SSH_TEST_HOST/_USER to run the live tunnel test");

        // Echo server = the tunnel's target (reachable from the SSH host = localhost).
        QTcpServer echo;
        QVERIFY(echo.listen(QHostAddress::LocalHost));
        connect(&echo, &QTcpServer::newConnection, [&] {
            QTcpSocket* c = echo.nextPendingConnection();
            connect(c, &QTcpSocket::readyRead, [c] { c->write(c->readAll()); });
        });

        core::Session ssh("gw", core::SessionType::Ssh);
        ssh.setHost(host);
        ssh.setUsername(user);
        if (const char* pass = std::getenv("MACXTERM_SSH_TEST_PASS")) ssh.setParam("password", pass);
        if (const char* key = std::getenv("MACXTERM_SSH_TEST_KEY")) ssh.setParam("keyfile", key);
        if (const char* port = std::getenv("MACXTERM_SSH_TEST_PORT")) ssh.setPort(QString(port).toInt());

        tunnel::Tunnel t;
        t.kind = tunnel::TunnelKind::Local;
        t.bindAddr = "127.0.0.1";
        t.bindPort = 0;  // ephemeral
        t.targetHost = "127.0.0.1";
        t.targetPort = echo.serverPort();

        tunnel::SshTunnel tun;
        QVERIFY(tun.start(ssh, t));
        QVERIFY(tun.isRunning());

        // Connect through the tunnel and expect our bytes echoed back.
        QTcpSocket client;
        QByteArray got;
        connect(&client, &QTcpSocket::readyRead, [&] { got += client.readAll(); });
        client.connectToHost(QHostAddress::LocalHost, tun.listenPort());
        QVERIFY(client.waitForConnected(3000));
        client.write("tunnel-echo-123");
        QTRY_VERIFY_WITH_TIMEOUT(got.contains("tunnel-echo-123"), 6000);
        tun.stop();
    }

    void dynamicSocksProxy() {
        const char* host = std::getenv("MACXTERM_SSH_TEST_HOST");
        const char* user = std::getenv("MACXTERM_SSH_TEST_USER");
        if (!host || !user) QSKIP("Set MACXTERM_SSH_TEST_HOST/_USER to run the live tunnel test");

        // Echo server reachable from the SSH host (= localhost).
        QTcpServer echo;
        QVERIFY(echo.listen(QHostAddress::LocalHost));
        connect(&echo, &QTcpServer::newConnection, [&] {
            QTcpSocket* c = echo.nextPendingConnection();
            connect(c, &QTcpSocket::readyRead, [c] { c->write(c->readAll()); });
        });

        core::Session ssh = sshServer();

        // Dynamic (SOCKS) tunnel: no fixed target — the client negotiates it.
        tunnel::Tunnel t;
        t.kind = tunnel::TunnelKind::Dynamic;
        t.bindAddr = "127.0.0.1";
        t.bindPort = 0;

        tunnel::SshTunnel tun;
        QVERIFY(tun.start(ssh, t));
        QVERIFY(tun.isRunning());

        // Speak SOCKS5 to the proxy: greeting, then CONNECT to the echo server.
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, tun.listenPort());
        QVERIFY(client.waitForConnected(3000));
        client.write(QByteArray::fromHex("050100"));          // VER=5, 1 method, no-auth
        QVERIFY(client.waitForReadyRead(3000));
        QCOMPARE(client.readAll(), QByteArray::fromHex("0500"));  // no-auth chosen

        QByteArray req = QByteArray::fromHex("05010001");     // VER CMD=CONNECT RSV ATYP=IPv4
        req += QByteArray::fromHex("7f000001");               // 127.0.0.1
        const quint16 ep = echo.serverPort();
        req += char((ep >> 8) & 0xff); req += char(ep & 0xff);
        client.write(req);
        QVERIFY(client.waitForReadyRead(3000));
        const QByteArray reply = client.readAll();
        QVERIFY(reply.size() >= 2);
        QCOMPARE(static_cast<unsigned char>(reply[1]), static_cast<unsigned char>(0x00));  // granted

        // Now the channel is a transparent pipe to the echo server.
        QByteArray got;
        connect(&client, &QTcpSocket::readyRead, [&] { got += client.readAll(); });
        client.write("socks-echo-xyz");
        QTRY_VERIFY_WITH_TIMEOUT(got.contains("socks-echo-xyz"), 6000);
        tun.stop();
    }

    void remoteForwardStartsAndStops() {
        const char* host = std::getenv("MACXTERM_SSH_TEST_HOST");
        const char* user = std::getenv("MACXTERM_SSH_TEST_USER");
        if (!host || !user) QSKIP("Set MACXTERM_SSH_TEST_HOST/_USER to run the live tunnel test");

        QTcpServer target;               // local target the remote port forwards to
        QVERIFY(target.listen(QHostAddress::LocalHost));

        core::Session ssh = sshServer();
        tunnel::Tunnel t;
        t.kind = tunnel::TunnelKind::Remote;
        t.bindAddr = "127.0.0.1";
        t.bindPort = 0;                  // let the server pick
        t.targetHost = "127.0.0.1";
        t.targetPort = target.serverPort();

        tunnel::SshTunnel tun;
        QVERIFY(tun.start(ssh, t));      // opens the remote listener on its own thread
        QVERIFY(tun.isRunning());
        QTest::qWait(300);
        tun.stop();
        QVERIFY(!tun.isRunning());
    }

private:
    core::Session sshServer() {
        core::Session ssh("gw", core::SessionType::Ssh);
        ssh.setHost(std::getenv("MACXTERM_SSH_TEST_HOST"));
        ssh.setUsername(std::getenv("MACXTERM_SSH_TEST_USER"));
        if (const char* pass = std::getenv("MACXTERM_SSH_TEST_PASS")) ssh.setParam("password", pass);
        if (const char* key = std::getenv("MACXTERM_SSH_TEST_KEY")) ssh.setParam("keyfile", key);
        if (const char* port = std::getenv("MACXTERM_SSH_TEST_PORT")) ssh.setPort(QString(port).toInt());
        return ssh;
    }
};

QTEST_GUILESS_MAIN(TestSshTunnelLive)
#include "test_ssh_tunnel_live.moc"
