#include "term/ScreenBuffer.h"
#include <QtTest/QtTest>

using namespace macxterm::term;

class TestScreenBuffer : public QObject {
    Q_OBJECT
private slots:
    void dimensions() {
        ScreenBuffer b(10, 40);
        QCOMPARE(b.rows(), 10);
        QCOMPARE(b.cols(), 40);
    }

    void writeAndReadRow() {
        ScreenBuffer b(2, 10);
        b.at(0, 0).ch = 'H';
        b.at(0, 1).ch = 'i';
        QCOMPARE(b.rowText(0), QStringLiteral("Hi"));
        QCOMPARE(b.rowText(1), QString());
    }

    void toTextTrimsTrailingBlankRows() {
        ScreenBuffer b(3, 5);
        b.at(0, 0).ch = 'X';
        QCOMPARE(b.toText(), QStringLiteral("X"));
    }

    void resizeClears() {
        ScreenBuffer b(2, 2);
        b.at(0, 0).ch = 'Z';
        b.resize(4, 4);
        QCOMPARE(b.rows(), 4);
        QCOMPARE(b.at(0, 0).ch, char32_t(' '));
    }
};

QTEST_APPLESS_MAIN(TestScreenBuffer)
#include "test_screenbuffer.moc"
