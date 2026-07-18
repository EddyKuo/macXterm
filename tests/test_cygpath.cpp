#include "core/CygPath.h"
#include <QtTest/QtTest>

using namespace macxterm::core;

class TestCygPath : public QObject {
    Q_OBJECT
private slots:
    void windowsToPosixDrivePaths() {
        QCOMPARE(CygPath::windowsToPosix("C:\\Users\\me"), QStringLiteral("/drives/c/Users/me"));
        QCOMPARE(CygPath::windowsToPosix("D:\\data\\x.txt"), QStringLiteral("/drives/d/data/x.txt"));
        QCOMPARE(CygPath::windowsToPosix("C:\\"), QStringLiteral("/drives/c"));
        QCOMPARE(CygPath::windowsToPosix("c:/lower/case"), QStringLiteral("/drives/c/lower/case"));
    }
    void windowsToPosixUnc() {
        QCOMPARE(CygPath::windowsToPosix("\\\\host\\share\\f"), QStringLiteral("//host/share/f"));
    }
    void posixToWindowsDrivePaths() {
        QCOMPARE(CygPath::posixToWindows("/drives/c/Users/me"), QStringLiteral("C:\\Users\\me"));
        QCOMPARE(CygPath::posixToWindows("/drives/d"), QStringLiteral("D:\\"));
        QCOMPARE(CygPath::posixToWindows("/cygdrive/e/x"), QStringLiteral("E:\\x"));
    }
    void roundTrip() {
        const QString w = "C:\\Program Files\\App\\bin";
        QCOMPARE(CygPath::posixToWindows(CygPath::windowsToPosix(w)), w);
    }
    void passthroughForNonDrivePaths() {
        QCOMPARE(CygPath::posixToWindows("/home/me"), QStringLiteral("/home/me"));
        QCOMPARE(CygPath::windowsToPosix("relative/path"), QStringLiteral("relative/path"));
    }
};

QTEST_APPLESS_MAIN(TestCygPath)
#include "test_cygpath.moc"
