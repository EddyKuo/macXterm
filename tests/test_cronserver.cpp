#include "tools/CronServer.h"
#include <QtTest/QtTest>
#include <QSignalSpy>

using namespace macxterm::tools;

class TestCronServer : public QObject {
    Q_OBJECT
private slots:
    void addsValidJobsRejectsInvalid() {
        CronServer s;
        QCOMPARE(s.jobCount(), 0);
        QVERIFY(s.addJob(QStringLiteral("*/5 * * * *"), QStringLiteral("echo hi")));
        QCOMPARE(s.jobCount(), 1);
        QVERIFY(!s.addJob(QStringLiteral("bad expr"), QStringLiteral("echo no")));
        QCOMPARE(s.jobCount(), 1);
        s.clearJobs();
        QCOMPARE(s.jobCount(), 0);
    }

    void startStopTogglesRunning() {
        CronServer s;
        QVERIFY(!s.isRunning());
        s.start();
        QVERIFY(s.isRunning());
        s.start();                 // idempotent
        QVERIFY(s.isRunning());
        s.stop();
        QVERIFY(!s.isRunning());
    }

    void multipleJobsAccumulate() {
        CronServer s;
        QVERIFY(s.addJob(QStringLiteral("0 * * * *"), QStringLiteral("a")));
        QVERIFY(s.addJob(QStringLiteral("30 9 * * 1-5"), QStringLiteral("b")));
        QCOMPARE(s.jobCount(), 2);
        s.start();
        QVERIFY(s.isRunning());
        s.stop();
        QVERIFY(!s.isRunning());
    }
};

QTEST_MAIN(TestCronServer)
#include "test_cronserver.moc"
