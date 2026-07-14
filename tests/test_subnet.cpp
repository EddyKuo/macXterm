#include "tools/Subnet.h"
#include <QtTest/QtTest>

using namespace macxterm::tools;

class TestSubnet : public QObject {
    Q_OBJECT
private slots:
    void parsesIpv4() {
        quint32 v = 0;
        QVERIFY(Subnet::parseIpv4(QStringLiteral("192.168.1.1"), v));
        QCOMPARE(v, 0xC0A80101u);
        QCOMPARE(Subnet::formatIpv4(v), QStringLiteral("192.168.1.1"));
    }

    void rejectsBadIpv4() {
        quint32 v = 0;
        QVERIFY(!Subnet::parseIpv4(QStringLiteral("256.0.0.1"), v));
        QVERIFY(!Subnet::parseIpv4(QStringLiteral("1.2.3"), v));
        QVERIFY(!Subnet::parseIpv4(QStringLiteral("a.b.c.d"), v));
    }

    void slash24ExcludesNetworkAndBroadcast() {
        const auto hosts = Subnet::hosts(QStringLiteral("192.168.1.0/24"));
        QCOMPARE(hosts.size(), 254);                       // .1 .. .254
        QCOMPARE(hosts.first(), QStringLiteral("192.168.1.1"));
        QCOMPARE(hosts.last(), QStringLiteral("192.168.1.254"));
        QVERIFY(!hosts.contains(QStringLiteral("192.168.1.0")));
        QVERIFY(!hosts.contains(QStringLiteral("192.168.1.255")));
    }

    void slash30HasTwoHosts() {
        const auto hosts = Subnet::hosts(QStringLiteral("10.0.0.0/30"));
        QCOMPARE(hosts.size(), 2);                          // .1 .2 (excl .0/.3)
    }

    void slash32IsSingleAddress() {
        const auto hosts = Subnet::hosts(QStringLiteral("10.0.0.5/32"));
        QCOMPARE(hosts.size(), 1);
        QCOMPARE(hosts.first(), QStringLiteral("10.0.0.5"));
    }

    void rejectsTooLargeAndMalformed() {
        QVERIFY(Subnet::hosts(QStringLiteral("10.0.0.0/8")).isEmpty());   // < /16
        QVERIFY(Subnet::hosts(QStringLiteral("nonsense")).isEmpty());
        QVERIFY(Subnet::hosts(QStringLiteral("10.0.0.0")).isEmpty());     // no prefix
    }
};

QTEST_APPLESS_MAIN(TestSubnet)
#include "test_subnet.moc"
