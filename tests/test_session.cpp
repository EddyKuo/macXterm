#include "core/Session.h"
#include <QtTest/QtTest>

using namespace macxterm::core;

class TestSession : public QObject {
    Q_OBJECT
private slots:
    void typeRoundTrip() {
        QCOMPARE(sessionTypeToString(SessionType::Ssh), QStringLiteral("SSH"));
        QCOMPARE(sessionTypeFromString("ssh"), SessionType::Ssh);
        QCOMPARE(sessionTypeFromString("VNC"), SessionType::Vnc);
        QCOMPARE(sessionTypeFromString("nonsense"), SessionType::Unknown);
    }

    void defaultPorts() {
        Session ssh("a", SessionType::Ssh);
        QCOMPARE(ssh.port(), 22);
        Session rdp("b", SessionType::Rdp);
        QCOMPARE(rdp.port(), 3389);
        Session vnc("c", SessionType::Vnc);
        QCOMPARE(vnc.port(), 5900);
    }

    void explicitPortOverrides() {
        Session s("a", SessionType::Ssh);
        s.setPort(2222);
        QCOMPARE(s.port(), 2222);
    }

    void paramsAndEquality() {
        Session a("srv", SessionType::Ssh);
        a.setHost("10.0.0.1");
        a.setUsername("root");
        Session b = a;
        QVERIFY(a == b);
        b.setHost("10.0.0.2");
        QVERIFY(!(a == b));
        QCOMPARE(a.host(), QStringLiteral("10.0.0.1"));
    }
};

QTEST_APPLESS_MAIN(TestSession)
#include "test_session.moc"
