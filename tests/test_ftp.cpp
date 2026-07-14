#include "tools/FtpCommand.h"
#include <QtTest/QtTest>

using namespace macxterm::tools::ftp;

class TestFtp : public QObject {
    Q_OBJECT
private slots:
    void parseCommandWithArg() {
        Command c = parse("RETR file.txt\r\n");
        QVERIFY(c.valid);
        QCOMPARE(c.verb, QStringLiteral("RETR"));
        QCOMPARE(c.arg, QStringLiteral("file.txt"));
    }

    void parseCommandNoArg() {
        Command c = parse("PWD\r\n");
        QVERIFY(c.valid);
        QCOMPARE(c.verb, QStringLiteral("PWD"));
        QVERIFY(c.arg.isEmpty());
    }

    void verbUpperCased() {
        QCOMPARE(parse("user alice\r\n").verb, QStringLiteral("USER"));
    }

    void emptyLineInvalid() {
        QVERIFY(!parse("\r\n").valid);
    }

    void replyFormat() {
        QCOMPARE(reply(220, "Welcome"), QByteArray("220 Welcome\r\n"));
    }
};

QTEST_APPLESS_MAIN(TestFtp)
#include "test_ftp.moc"
