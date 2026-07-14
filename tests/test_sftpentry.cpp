#include "sftp/SftpEntry.h"
#include <QtTest/QtTest>

using namespace macxterm::sftp;

class TestSftpEntry : public QObject {
    Q_OBJECT
private slots:
    void permStringFromMode() {
        SftpEntry e;
        e.permissions = 0755;
        QCOMPARE(e.permString(), QStringLiteral("-rwxr-xr-x"));
        e.isDir = true;
        e.permissions = 0700;
        QCOMPARE(e.permString(), QStringLiteral("drwx------"));
    }

    void sizeStringHumanReadable() {
        SftpEntry e;
        e.size = 512;   QCOMPARE(e.sizeString(), QStringLiteral("512 B"));
        e.size = 1536;  QCOMPARE(e.sizeString(), QStringLiteral("1.5 KB"));
        e.size = 1048576; QCOMPARE(e.sizeString(), QStringLiteral("1.0 MB"));
    }

    void sortDirsFirstThenName() {
        QList<SftpEntry> in;
        in.append({"zebra.txt", 0, false, 0, 0});
        in.append({"apple", 0, true, 0, 0});
        in.append({"banana.txt", 0, false, 0, 0});
        in.append({"..", 0, true, 0, 0});
        auto out = sortListing(in);
        QCOMPARE(out[0].name, QStringLiteral(".."));      // parent always first
        QCOMPARE(out[1].name, QStringLiteral("apple"));   // dir before files
        QCOMPARE(out[2].name, QStringLiteral("banana.txt"));
        QCOMPARE(out[3].name, QStringLiteral("zebra.txt"));
    }
};

QTEST_APPLESS_MAIN(TestSftpEntry)
#include "test_sftpentry.moc"
