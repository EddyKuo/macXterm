#include "core/PuttyImporter.h"
#include <QtTest/QtTest>

using namespace macxterm::core;

class TestPuttyImport : public QObject {
    Q_OBJECT
private slots:
    void parsesSshSession() {
        // Registry-derived "Key=value" form (what importFromRegistry feeds parseSession).
        const QByteArray body =
            "HostName=example.com\n"
            "Protocol=ssh\n"
            "PortNumber=2222\n"
            "UserName=bob\n"
            "PublicKeyFile=C:\\keys\\id.ppk\n"
            "Compression=1\n";
        const Session s = PuttyImporter::parseSession("my server", body);
        QCOMPARE(s.type(), SessionType::Ssh);
        QCOMPARE(s.host(), QStringLiteral("example.com"));
        QCOMPARE(s.port(), 2222);
        QCOMPARE(s.username(), QStringLiteral("bob"));
        QCOMPARE(s.param("keyfile"), QStringLiteral("C:\\keys\\id.ppk"));
        QCOMPARE(s.param("compression"), QStringLiteral("1"));
    }
    void mapsProtocolToType() {
        QCOMPARE(PuttyImporter::parseSession("t", "Protocol=telnet\nHostName=h\n").type(),
                 SessionType::Telnet);
        QCOMPARE(PuttyImporter::parseSession("r", "Protocol=rlogin\nHostName=h\n").type(),
                 SessionType::Rlogin);
    }
    void proxyBecomesGateway() {
        const Session s = PuttyImporter::parseSession("g",
            "HostName=target\nProtocol=ssh\nProxyHost=bastion\nProxyPort=2200\nProxyUsername=jump\n");
        QCOMPARE(s.param("gateway"), QStringLiteral("jump@bastion:2200"));
    }
    void unixFileFormatBackslashSeparator() {
        // Unix PuTTY session files use "Key\value\".
        const Session s = PuttyImporter::parseSession("u", "HostName\\host.example\\\nProtocol\\ssh\\\n");
        QCOMPARE(s.host(), QStringLiteral("host.example"));
    }
};

QTEST_APPLESS_MAIN(TestPuttyImport)
#include "test_puttyimport.moc"
