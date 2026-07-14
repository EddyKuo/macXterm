#include "tools/CronExpr.h"
#include <QtTest/QtTest>

using namespace macxterm::tools;

class TestCronExpr : public QObject {
    Q_OBJECT
private slots:
    void everyMinuteMatchesAlways() {
        CronExpr c(QStringLiteral("* * * * *"));
        QVERIFY(c.isValid());
        QVERIFY(c.matches(QDateTime(QDate(2026, 7, 14), QTime(3, 27))));
    }

    void specificMinuteHour() {
        CronExpr c(QStringLiteral("30 9 * * *"));
        QVERIFY(c.isValid());
        QVERIFY(c.matches(QDateTime(QDate(2026, 7, 14), QTime(9, 30))));
        QVERIFY(!c.matches(QDateTime(QDate(2026, 7, 14), QTime(9, 31))));
        QVERIFY(!c.matches(QDateTime(QDate(2026, 7, 14), QTime(8, 30))));
    }

    void stepAndRange() {
        CronExpr c(QStringLiteral("*/15 * * * *"));
        QVERIFY(c.isValid());
        for (int m : {0, 15, 30, 45})
            QVERIFY(c.matches(QDateTime(QDate(2026, 7, 14), QTime(1, m))));
        QVERIFY(!c.matches(QDateTime(QDate(2026, 7, 14), QTime(1, 10))));

        CronExpr r(QStringLiteral("0 9-17 * * *"));   // business hours
        QVERIFY(r.matches(QDateTime(QDate(2026, 7, 14), QTime(9, 0))));
        QVERIFY(r.matches(QDateTime(QDate(2026, 7, 14), QTime(17, 0))));
        QVERIFY(!r.matches(QDateTime(QDate(2026, 7, 14), QTime(18, 0))));
    }

    void dayOfWeek() {
        // 2026-07-14 is a Tuesday. Cron dow: Sun=0..Sat=6, so Tue=2.
        CronExpr tue(QStringLiteral("0 0 * * 2"));
        QVERIFY(tue.matches(QDateTime(QDate(2026, 7, 14), QTime(0, 0))));
        CronExpr wed(QStringLiteral("0 0 * * 3"));
        QVERIFY(!wed.matches(QDateTime(QDate(2026, 7, 14), QTime(0, 0))));
        // Sunday as both 0 and 7. 2026-07-19 is a Sunday.
        CronExpr sun7(QStringLiteral("0 0 * * 7"));
        QVERIFY(sun7.matches(QDateTime(QDate(2026, 7, 19), QTime(0, 0))));
    }

    void listField() {
        CronExpr c(QStringLiteral("0,30 * * * *"));
        QVERIFY(c.matches(QDateTime(QDate(2026, 7, 14), QTime(5, 0))));
        QVERIFY(c.matches(QDateTime(QDate(2026, 7, 14), QTime(5, 30))));
        QVERIFY(!c.matches(QDateTime(QDate(2026, 7, 14), QTime(5, 15))));
    }

    void rejectsMalformed() {
        QVERIFY(!CronExpr(QStringLiteral("* * * *")).isValid());        // too few fields
        QVERIFY(!CronExpr(QStringLiteral("60 * * * *")).isValid());     // minute out of range
        QVERIFY(!CronExpr(QStringLiteral("* 24 * * *")).isValid());     // hour out of range
        QVERIFY(!CronExpr(QStringLiteral("x * * * *")).isValid());      // non-numeric
    }
};

QTEST_APPLESS_MAIN(TestCronExpr)
#include "test_cronexpr.moc"
