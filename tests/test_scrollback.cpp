#include "term/VtEngine.h"
#include <QtTest/QtTest>

using namespace macxterm::term;

static QString lineText(const QVector<Cell>& line) {
    QString s;
    for (const Cell& c : line) appendCodePoint(s, c.ch);
    while (!s.isEmpty() && s.back() == QChar(' ')) s.chop(1);
    return s;
}

class TestScrollback : public QObject {
    Q_OBJECT
private slots:
    void linesScrollIntoScrollback() {
        VtEngine vt(3, 20);   // only 3 visible rows
        vt.input("L1\r\nL2\r\nL3\r\nL4\r\nL5\r\n");

        // With 3 rows, the earliest lines scrolled off the top and were retained.
        QVERIFY(vt.scrollbackCount() >= 2);
        QCOMPARE(lineText(vt.scrollbackLine(0)), QStringLiteral("L1"));
        QCOMPARE(lineText(vt.scrollbackLine(1)), QStringLiteral("L2"));
    }

    void scrollbackIsCappedImplicitly() {
        VtEngine vt(2, 10);
        for (int i = 0; i < 50; ++i) vt.input(QByteArray("x\r\n"));
        // Many lines scrolled off; scrollback retains them (well under the cap).
        QVERIFY(vt.scrollbackCount() >= 40);
    }
};

QTEST_APPLESS_MAIN(TestScrollback)
#include "test_scrollback.moc"
