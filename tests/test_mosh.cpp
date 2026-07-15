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

    void customPortAndKeyMergeIntoOneSshArg() {
        core::Session s("m", core::SessionType::Mosh);
        s.setHost("h");
        s.setPort(2222);
        s.setParam("keyfile", "/k.pem");
        auto args = connect::MoshConnection::buildArgs(s);
        // A single --ssh whose value carries BOTH the key and the port (emitting
        // --ssh twice used to drop the key).
        QCOMPARE(args.count("--ssh"), 1);
        const int i = args.indexOf("--ssh");
        QCOMPARE(args.at(i + 1), QStringLiteral("ssh -i /k.pem -p 2222"));
        QCOMPARE(args.last(), QStringLiteral("h"));  // no username
    }

    void udpPortAndPredict() {
        core::Session s("m", core::SessionType::Mosh);
        s.setHost("h");
        s.setParam("moshport", "60000:60010");
        s.setParam("predict", "experimental");
        auto args = connect::MoshConnection::buildArgs(s);
        const int p = args.indexOf("-p");
        QVERIFY(p >= 0);
        QCOMPARE(args.at(p + 1), QStringLiteral("60000:60010"));
        QVERIFY(args.contains("--predict=experimental"));
    }

    void noExtraArgsWhenPlain() {
        core::Session s("m", core::SessionType::Mosh);
        s.setHost("h");
        auto args = connect::MoshConnection::buildArgs(s);
        QVERIFY(!args.contains("--ssh"));     // default port + no key → no --ssh
        QVERIFY(!args.contains("-p"));
        QCOMPARE(args, QStringList{QStringLiteral("h")});
    }
};

QTEST_GUILESS_MAIN(TestMosh)
#include "test_mosh.moc"
