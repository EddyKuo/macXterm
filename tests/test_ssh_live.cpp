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
        conn.send("echo MACXTERM_SSH_LIVE_OK\n");
        QTRY_VERIFY_WITH_TIMEOUT(got.contains("MACXTERM_SSH_LIVE_OK"), 8000);
        conn.disconnectSession();
    }
};

QTEST_GUILESS_MAIN(TestSshLive)
#include "test_ssh_live.moc"
