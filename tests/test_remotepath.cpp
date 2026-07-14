#include "sftp/RemotePath.h"
#include <QtTest/QtTest>

using namespace macxterm::sftp;

class TestRemotePath : public QObject {
    Q_OBJECT
private slots:
    void normalizeCollapsesAndResolves() {
        QCOMPARE(RemotePath::normalize("/a//b/./c"), QStringLiteral("/a/b/c"));
        QCOMPARE(RemotePath::normalize("/a/b/../c"), QStringLiteral("/a/c"));
        QCOMPARE(RemotePath::normalize("/a/../../x"), QStringLiteral("/x")); // .. past root ignored
        QCOMPARE(RemotePath::normalize("/"), QStringLiteral("/"));
        QCOMPARE(RemotePath::normalize(""), QStringLiteral("/"));
    }

    void joinHandlesAbsoluteChild() {
        QCOMPARE(RemotePath::join("/home/user", "docs"), QStringLiteral("/home/user/docs"));
        QCOMPARE(RemotePath::join("/home/user", "/etc"), QStringLiteral("/etc"));
        QCOMPARE(RemotePath::join("/home/user", "../root"), QStringLiteral("/home/root"));
    }

    void parentAndBaseName() {
        QCOMPARE(RemotePath::parent("/a/b/c.txt"), QStringLiteral("/a/b"));
        QCOMPARE(RemotePath::parent("/"), QStringLiteral("/"));
        QCOMPARE(RemotePath::baseName("/a/b/c.txt"), QStringLiteral("c.txt"));
        QCOMPARE(RemotePath::baseName("/"), QString());
    }
};

QTEST_APPLESS_MAIN(TestRemotePath)
#include "test_remotepath.moc"
