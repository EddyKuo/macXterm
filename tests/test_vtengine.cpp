#include "term/VtEngine.h"
#include <QtTest/QtTest>

using namespace macxterm::term;

class TestVtEngine : public QObject {
    Q_OBJECT
private slots:
    void plainText() {
        VtEngine vt(5, 20);
        vt.input("hello");
        QVERIFY(vt.screenText().startsWith("hello"));
    }

    void newlineMovesDown() {
        VtEngine vt(5, 20);
        vt.input("line1\r\nline2");
        const QString txt = vt.screenText();
        QVERIFY(txt.contains("line1"));
        QVERIFY(txt.contains("line2"));
    }

    void eraseScreenClears() {
        VtEngine vt(5, 20);
        vt.input("garbage");
        QVERIFY(!vt.screenText().isEmpty());
        vt.input("\x1b[2J\x1b[H");   // clear screen + home
        QCOMPARE(vt.screenText(), QString());
    }

    void cursorPositioning() {
        VtEngine vt(5, 20);
        vt.input("\x1b[2;3HX");      // row 2, col 3
        QCOMPARE(vt.screen().at(1, 2).ch, QChar('X'));
    }

    void nonBmpCodepointDoesNotCrash() {
        // U+1F600 GRINNING FACE (astral plane) — must not trip QChar's
        // BMP assertion; it is substituted with the replacement character.
        VtEngine vt(3, 10);
        vt.input(QByteArray("\xF0\x9F\x98\x80"));   // UTF-8 for U+1F600
        QCOMPARE(vt.screen().at(0, 0).ch, QChar(QChar::ReplacementCharacter));
    }

    void utf8BmpCharPreserved() {
        VtEngine vt(3, 10);
        vt.input(QByteArray("\xC3\xA9"));           // UTF-8 for U+00E9 'é'
        QCOMPARE(vt.screen().at(0, 0).ch, QChar(0x00E9));
    }

    void resizeKeepsEngineValid() {
        VtEngine vt(5, 20);
        vt.resize(10, 40);
        QCOMPARE(vt.rows(), 10);
        QCOMPARE(vt.cols(), 40);
        vt.input("ok");
        QVERIFY(vt.screenText().startsWith("ok"));
    }

    void scrollbackCapIsEnforced() {
        VtEngine vt(3, 20);
        vt.setScrollbackMax(5);
        QCOMPARE(vt.scrollbackMax(), 5);
        // Emit far more lines than the screen + cap can hold.
        for (int i = 0; i < 40; ++i) vt.input(QByteArray("row\r\n"));
        QVERIFY(vt.scrollbackCount() <= 5);
    }

    void shrinkingScrollbackTrimsBacklog() {
        VtEngine vt(3, 20);
        vt.setScrollbackMax(50);
        for (int i = 0; i < 40; ++i) vt.input(QByteArray("row\r\n"));
        QVERIFY(vt.scrollbackCount() > 3);
        vt.setScrollbackMax(3);
        QCOMPARE(vt.scrollbackCount(), 3);
    }

    void zeroScrollbackKeepsNothing() {
        VtEngine vt(3, 20);
        vt.setScrollbackMax(0);
        for (int i = 0; i < 20; ++i) vt.input(QByteArray("row\r\n"));
        QCOMPARE(vt.scrollbackCount(), 0);
    }
};

QTEST_APPLESS_MAIN(TestVtEngine)
#include "test_vtengine.moc"
