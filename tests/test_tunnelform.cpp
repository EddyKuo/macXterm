#include "tunnel/TunnelForm.h"
#include <QtTest/QtTest>

using namespace macxterm::tunnel;

class TestTunnelForm : public QObject {
    Q_OBJECT
private slots:
    void toTunnelMapsFields() {
        QVariantMap f;
        f.insert("kind", "local");
        f.insert("bindPort", 8080);
        f.insert("targetHost", "10.0.0.1");
        f.insert("targetPort", 80);
        Tunnel t = TunnelForm::toTunnel(f);
        QCOMPARE(t.kind, TunnelKind::Local);
        QCOMPARE(t.bindPort, quint16(8080));
        QCOMPARE(t.targetHost, QStringLiteral("10.0.0.1"));
        QCOMPARE(t.targetPort, quint16(80));
    }

    void roundTrip() {
        Tunnel t; t.kind = TunnelKind::Dynamic; t.bindPort = 1080; t.bindAddr = "127.0.0.1";
        Tunnel back = TunnelForm::toTunnel(TunnelForm::fromTunnel(t));
        QCOMPARE(back.kind, TunnelKind::Dynamic);
        QCOMPARE(back.bindPort, quint16(1080));
    }

    void validateLocalNeedsTarget() {
        QVariantMap f; f.insert("kind", "local"); f.insert("bindPort", 9000);
        QVERIFY(!TunnelForm::validate(f).isEmpty());     // no target
        f.insert("targetHost", "h"); f.insert("targetPort", 22);
        QVERIFY(TunnelForm::validate(f).isEmpty());
    }

    void validateDynamicNeedsOnlyBind() {
        QVariantMap f; f.insert("kind", "dynamic"); f.insert("bindPort", 1080);
        QVERIFY(TunnelForm::validate(f).isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestTunnelForm)
#include "test_tunnelform.moc"
