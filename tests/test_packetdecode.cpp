#include "tools/PacketCapture.h"
#include <QtTest/QtTest>
#include <QByteArray>

using namespace macxterm::tools;

// Build an Ethernet + IPv4 + L4 frame for the decoder.
static QByteArray ethIpv4(quint8 proto, const QByteArray& l4) {
    QByteArray f;
    f.append(QByteArray(6, '\x01'));                 // dst MAC
    f.append(QByteArray(6, '\x02'));                 // src MAC
    f.append("\x08\x00", 2);                         // ethertype IPv4
    QByteArray ip(20, '\0');
    ip[0] = char(0x45);                              // version 4, IHL 5
    ip[9] = char(proto);
    ip[12] = char(192); ip[13] = char(168); ip[14] = char(1); ip[15] = char(2);   // src
    ip[16] = char(10);  ip[17] = char(0);   ip[18] = char(0); ip[19] = char(1);    // dst
    f.append(ip);
    f.append(l4);
    return f;
}

class TestPacketDecode : public QObject {
    Q_OBJECT
private slots:
    void decodesTcp() {
        QByteArray tcp(4, '\0');
        tcp[0] = char(0xC7); tcp[1] = char(0x38);    // src port 51000
        tcp[2] = char(0x00); tcp[3] = char(0x16);    // dst port 22
        const QByteArray f = ethIpv4(6, tcp);
        const auto s = PacketDecode::summarize(
            reinterpret_cast<const unsigned char*>(f.constData()), f.size(), 1);
        QCOMPARE(s.protocol, QStringLiteral("TCP"));
        QCOMPARE(s.info, QStringLiteral("192.168.1.2:51000 → 10.0.0.1:22"));
        QCOMPARE(s.length, f.size());
    }

    void decodesUdp() {
        QByteArray udp(4, '\0');
        udp[0] = char(0x00); udp[1] = char(0x35);    // src port 53
        udp[2] = char(0x30); udp[3] = char(0x39);    // dst port 12345
        const QByteArray f = ethIpv4(17, udp);
        const auto s = PacketDecode::summarize(
            reinterpret_cast<const unsigned char*>(f.constData()), f.size(), 1);
        QCOMPARE(s.protocol, QStringLiteral("UDP"));
        QVERIFY(s.info.contains(QStringLiteral("192.168.1.2:53")));
        QVERIFY(s.info.contains(QStringLiteral("10.0.0.1:12345")));
    }

    void decodesIcmp() {
        const QByteArray f = ethIpv4(1, QByteArray(8, '\0'));
        const auto s = PacketDecode::summarize(
            reinterpret_cast<const unsigned char*>(f.constData()), f.size(), 1);
        QCOMPARE(s.protocol, QStringLiteral("ICMP"));
    }

    void detectsArp() {
        QByteArray f;
        f.append(QByteArray(6, '\xff'));
        f.append(QByteArray(6, '\x02'));
        f.append("\x08\x06", 2);                     // ARP ethertype
        f.append(QByteArray(28, '\0'));
        const auto s = PacketDecode::summarize(
            reinterpret_cast<const unsigned char*>(f.constData()), f.size(), 1);
        QCOMPARE(s.protocol, QStringLiteral("ARP"));
    }

    void handlesRunt() {
        const QByteArray f(8, '\0');                 // too short for a full frame
        const auto s = PacketDecode::summarize(
            reinterpret_cast<const unsigned char*>(f.constData()), f.size(), 1);
        QVERIFY(!s.protocol.isEmpty());              // must not crash
    }
};

QTEST_APPLESS_MAIN(TestPacketDecode)
#include "test_packetdecode.moc"
