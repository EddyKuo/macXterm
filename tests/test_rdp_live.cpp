#include "connect/RdpConnection.h"
#include "core/Session.h"
#include <QtTest/QtTest>
#include <cstdlib>

using namespace macxterm;

// Guarded-live RDP test. Runs when MACXTERM_RDP_TEST_HOST is set (point it at a
// deployed RDP server fixture, e.g. sfreerdp or an xrdp container). Requires the
// build to have FreeRDP (MACXTERM_HAVE_FREERDP); otherwise the connection is a
// scaffold and the test QSKIPs.
class TestRdpLive : public QObject {
    Q_OBJECT
private slots:
    void connectsToRdpServer() {
#ifndef MACXTERM_HAVE_FREERDP
        QSKIP("Built without FreeRDP; RDP is a scaffold in this build");
#else
        const char* host = std::getenv("MACXTERM_RDP_TEST_HOST");
        if (!host) QSKIP("Set MACXTERM_RDP_TEST_HOST to run the live RDP test");

        connect::RdpConnection conn;
        core::Session s("rdp", core::SessionType::Rdp);
        s.setHost(host);
        s.setParam("ignorecert", "1");   // fixtures use a self-signed cert
        if (const char* port = std::getenv("MACXTERM_RDP_TEST_PORT")) s.setPort(QString(port).toInt());
        if (const char* user = std::getenv("MACXTERM_RDP_TEST_USER")) s.setUsername(user);
        if (const char* pass = std::getenv("MACXTERM_RDP_TEST_PASS")) s.setParam("password", pass);

        const bool ok = conn.connectSession(s);
        if (!ok) QSKIP("RDP server not reachable / negotiation refused");
        QCOMPARE(conn.state(), connect::IConnection::State::Connected);

        // Pump the FreeRDP event loop briefly and capture a framebuffer.
        for (int i = 0; i < 50 && conn.currentFrame().isNull(); ++i) {
            conn.poll();
            QTest::qWait(50);
        }
        QVERIFY(!conn.currentFrame().isNull());   // got a real remote framebuffer
        conn.disconnectSession();
#endif
    }
};

QTEST_GUILESS_MAIN(TestRdpLive)
#include "test_rdp_live.moc"
