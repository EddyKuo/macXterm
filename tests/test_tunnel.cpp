#include "tunnel/Tunnel.h"
#include <QtTest/QtTest>

using namespace macxterm::tunnel;

class TestTunnel : public QObject {
    Q_OBJECT
private slots:
    void kindStrings() {
        QCOMPARE(tunnelKindToString(TunnelKind::Local), QStringLiteral("local"));
        QCOMPARE(tunnelKindToString(TunnelKind::Dynamic), QStringLiteral("dynamic"));
    }

    void localTunnelValidity() {
        Tunnel t; t.kind = TunnelKind::Local; t.bindPort = 8080;
        QVERIFY(!t.isValid());              // no target
        t.targetHost = "10.0.0.1"; t.targetPort = 80;
        QVERIFY(t.isValid());
    }

    void dynamicNeedsOnlyBindPort() {
        Tunnel t; t.kind = TunnelKind::Dynamic; t.bindPort = 1080;
        QVERIFY(t.isValid());
    }

    void managerRejectsInvalidAndCollisions() {
        TunnelManager m;
        Tunnel bad; bad.kind = TunnelKind::Local; bad.bindPort = 0;
        QVERIFY(!m.add(bad));               // invalid
        QCOMPARE(m.count(), 0);

        Tunnel a; a.kind = TunnelKind::Local; a.bindPort = 9000;
        a.targetHost = "h"; a.targetPort = 22;
        QVERIFY(m.add(a));
        QCOMPARE(m.count(), 1);

        Tunnel dup = a;                     // same bind port
        QVERIFY(!m.add(dup));               // collision
        QCOMPARE(m.count(), 1);
        QVERIFY(m.bindPortInUse(9000));
    }
};

QTEST_APPLESS_MAIN(TestTunnel)
#include "test_tunnel.moc"
