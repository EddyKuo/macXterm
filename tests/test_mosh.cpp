#include "connect/MoshConnection.h"
#include <QtTest/QtTest>

using namespace macxterm;

class TestMosh : public QObject {
    Q_OBJECT
private slots:
    void basicArgs() {
        core::Session s("m", core::SessionType::Mosh);
        s.setHost("host.example");
        s.setUsername("user");
        auto args = connect::MoshConnection::buildArgs(s);
        QCOMPARE(args.last(), QStringLiteral("user@host.example"));
    }

    // connectSession spawns the `mosh` wrapper. Whether or not mosh is installed
    // it drives the connect/send/disconnect paths (success → Connected; absent →
    // the "failed to launch" error branch).
    void connectSendDisconnect() {
        connect::MoshConnection conn;
        core::Session s("m", core::SessionType::Mosh);
        s.setHost(QStringLiteral("127.0.0.1"));
        s.setUsername(QStringLiteral("user"));
        conn.connectSession(s);          // true or false depending on host tooling
        conn.send("data");               // exercise send() (proc may not be running)
        conn.disconnectSession();        // exercise terminate/cleanup
    }

    void customPortAndKey() {
        core::Session s("m", core::SessionType::Mosh);
        s.setHost("h");
        s.setPort(2222);
        s.setParam("keyfile", "/k.pem");
        auto args = connect::MoshConnection::buildArgs(s);
        QVERIFY(args.contains("--ssh"));
        QVERIFY(args.join(' ').contains("ssh -i /k.pem"));
        QVERIFY(args.join(' ').contains("ssh -p 2222"));
        QCOMPARE(args.last(), QStringLiteral("h"));  // no username
    }
};

QTEST_GUILESS_MAIN(TestMosh)
#include "test_mosh.moc"
