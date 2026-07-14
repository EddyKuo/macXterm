#include "connect/SimpleTcpConnection.h"
#include <QtTest/QtTest>

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

    void rshHasNoHandshake() {
        core::Session s("r", core::SessionType::Rsh);
        QVERIFY(connect::SimpleTcpConnection::startupHandshake(
                    core::SessionType::Rsh, s).isEmpty());
    }

    void xdmcpHasQueryMarker() {
        core::Session s("x", core::SessionType::Xdmcp);
        const QByteArray h = connect::SimpleTcpConnection::startupHandshake(
            core::SessionType::Xdmcp, s);
        QCOMPARE(h.size(), 4);
    }
};

QTEST_APPLESS_MAIN(TestSimpleTcp)
#include "test_simpletcp.moc"
