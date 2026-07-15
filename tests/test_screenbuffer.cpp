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

    // ── S31-08 code-point helpers ──

    void appendCodePointBmp() {
        QString s;
        appendCodePoint(s, U'A');
        appendCodePoint(s, 0x00E9);          // é (BMP)
        QCOMPARE(s, QStringLiteral("Aé"));
        QCOMPARE(s.size(), 2);               // one UTF-16 unit each
    }

    void appendCodePointAstralUsesSurrogates() {
        QString s;
        appendCodePoint(s, 0x1F600);         // 😀 astral
        QCOMPARE(s.size(), 2);               // surrogate pair
        QVERIFY(s.at(0).isHighSurrogate());
        QVERIFY(s.at(1).isLowSurrogate());
        QCOMPARE(s.toUcs4().value(0), uint(0x1F600));
    }

    void codePointStringMatchesFromUcs4() {
        QCOMPARE(codePointString(0x1F600), QString::fromUcs4(U"\U0001F600"));
        QCOMPARE(codePointString(U'Z'), QStringLiteral("Z"));
    }

    void cellUnitFoldsAstralToReplacement() {
        // Column-aligned buffers keep one unit per cell: astral → U+FFFD.
        QCOMPARE(cellUnit(U'A'), QChar('A'));
        QCOMPARE(cellUnit(0x00E9), QChar(0x00E9));
        QCOMPARE(cellUnit(0x1F600), QChar(QChar::ReplacementCharacter));
    }

    void rowTextRoundTripsAstralCell() {
        // A single astral code point in one cell renders as a surrogate pair,
        // but still occupies exactly one cell.
        ScreenBuffer b(1, 3);
        b.at(0, 0).ch = 0x1F600;             // 😀
        b.at(0, 1).ch = 'x';
        const QString row = b.rowText(0);
        QCOMPARE(row, QString::fromUcs4(U"\U0001F600") + QStringLiteral("x"));
        QCOMPARE(row.toUcs4().size(), 2);    // two code points from two cells
    }
};

QTEST_APPLESS_MAIN(TestScreenBuffer)
#include "test_screenbuffer.moc"
