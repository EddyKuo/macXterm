#include "core/WinScpImporter.h"
#include <QtTest/QtTest>

using namespace macxterm::core;

class TestWinScpImport : public QObject {
    Q_OBJECT
private slots:
    void parsesSftpAndFtp() {
        const QByteArray ini =
            "[Sessions\\Default%20Settings]\n"
            "HostName=ignored\n"
            "\n"
            "[Sessions\\my%20sftp]\n"
            "HostName=sftp.example.com\n"
            "PortNumber=22\n"
            "UserName=alice\n"
            "FSProtocol=2\n"
            "\n"
            "[Sessions\\ftpbox]\n"
            "HostName=ftp.example.com\n"
            "FSProtocol=5\n";
        SessionFolder root = WinScpImporter::parseIni(ini);
        QCOMPARE(root.totalSessions(), 2);        // template session skipped

        const Session* sftp = root.findSession("my sftp");
        QVERIFY(sftp != nullptr);
        QCOMPARE(sftp->type(), SessionType::Sftp);
        QCOMPARE(sftp->host(), QStringLiteral("sftp.example.com"));
        QCOMPARE(sftp->port(), 22);
        QCOMPARE(sftp->username(), QStringLiteral("alice"));

        const Session* ftp = root.findSession("ftpbox");
        QVERIFY(ftp != nullptr);
        QCOMPARE(ftp->type(), SessionType::Ftp);
    }
    void skipsSessionsWithoutHost() {
        SessionFolder root = WinScpImporter::parseIni("[Sessions\\empty]\nUserName=x\n");
        QCOMPARE(root.totalSessions(), 0);
    }
};

QTEST_APPLESS_MAIN(TestWinScpImport)
#include "test_winscpimport.moc"
