#include "connect/SshConnection.h"
#include "core/Session.h"
#include <QtTest/QtTest>
#include <cstdlib>

using namespace macxterm;

// Guarded-live SSH integration test. It only runs when the environment provides
// a reachable endpoint via MACXTERM_SSH_TEST_HOST / _USER / _PASS (or _KEY);
// otherwise it QSKIPs with a reason so "unavailable" never reads as "passed"
// (TestPlan live-endpoint discipline).
class TestSshLive : public QObject {
    Q_OBJECT
private slots:
    void connectsAndRunsCommand() {
        const char* host = std::getenv("MACXTERM_SSH_TEST_HOST");
        const char* user = std::getenv("MACXTERM_SSH_TEST_USER");
        if (!host || !user) {
            QSKIP("Set MACXTERM_SSH_TEST_HOST/_USER (and _PASS or _KEY) to run live SSH test");
        }
        connect::SshConnection conn;
        core::Session s("live", core::SessionType::Ssh);
        s.setHost(host);
        s.setUsername(user);
        if (const char* pass = std::getenv("MACXTERM_SSH_TEST_PASS")) s.setParam("password", pass);
        if (const char* key = std::getenv("MACXTERM_SSH_TEST_KEY")) s.setParam("keyfile", key);
        if (const char* port = std::getenv("MACXTERM_SSH_TEST_PORT")) s.setPort(QString(port).toInt());

        QByteArray got;
        connect(&conn, &connect::IConnection::dataReceived, [&](const QByteArray& d){ got += d; });
        QVERIFY(conn.connectSession(s));
        conn.resize(100, 40);                       // exercise window resize
        conn.send("echo MACXTERM_SSH_LIVE_OK\n");
        QTRY_VERIFY_WITH_TIMEOUT(got.contains("MACXTERM_SSH_LIVE_OK"), 8000);
        conn.disconnectSession();
    }

    // Connect to the target *through itself* as an SSH gateway/jump host,
    // exercising the SSH-over-SSH openViaJump path (socketpair + relay thread).
    void connectsThroughGateway() {
        const char* host = std::getenv("MACXTERM_SSH_TEST_HOST");
        const char* user = std::getenv("MACXTERM_SSH_TEST_USER");
        const char* key = std::getenv("MACXTERM_SSH_TEST_KEY");
        const char* port = std::getenv("MACXTERM_SSH_TEST_PORT");
        if (!host || !user || !key) QSKIP("Set MACXTERM_SSH_TEST_HOST/_USER/_KEY for the gateway test");

        connect::SshConnection conn;
        core::Session s("via-gw", core::SessionType::Ssh);
        s.setHost(host);
        s.setUsername(user);
        s.setParam("keyfile", key);
        if (port) s.setPort(QString(port).toInt());
        // Gateway = the same server (jump to localhost through it).
        s.setParam("gateway", port ? QStringLiteral("%1@%2:%3").arg(user, host, port)
                                    : QStringLiteral("%1@%2").arg(user, host));
        s.setParam("gateway_keyfile", key);

        QByteArray got;
        connect(&conn, &connect::IConnection::dataReceived, [&](const QByteArray& d){ got += d; });
        QVERIFY(conn.connectSession(s));
        conn.send("echo MACXTERM_GW_OK\n");
        QTRY_VERIFY_WITH_TIMEOUT(got.contains("MACXTERM_GW_OK"), 8000);
        conn.disconnectSession();
    }

    // Connecting to a closed port fails cleanly (covers the socket-open failure
    // path + errorOccurred). No live endpoint needed.
    void failsOnClosedPort() {
        connect::SshConnection conn;
        core::Session s("dead", core::SessionType::Ssh);
        s.setHost(QStringLiteral("127.0.0.1"));
        s.setPort(1);                      // nothing listens on port 1
        s.setUsername(QStringLiteral("nobody"));
        s.setParam("password", QStringLiteral("x"));
        QVERIFY(!conn.connectSession(s));
    }

    // send() before connecting returns an error rather than crashing.
    void sendWithoutChannelFails() {
        connect::SshConnection conn;
        QVERIFY(conn.send("data") < 0);
    }
};

QTEST_GUILESS_MAIN(TestSshLive)
#include "test_ssh_live.moc"
