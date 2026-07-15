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

    void reflowRewrapsHistoryOnNarrow() {
        // Feed a long line that soft-wraps, push it into scrollback, then narrow.
        VtEngine vt(2, 20);
        vt.setScrollbackMax(100);
        // 25 chars wraps across the 20-col screen; extra newlines push to scrollback.
        vt.input(QByteArray("abcdefghijklmnopqrstuvwxy\r\n"));
        for (int i = 0; i < 5; ++i) vt.input(QByteArray("\r\n"));
        const int before = vt.scrollbackCount();
        QVERIFY(before > 0);
        vt.resize(2, 10);   // narrow: the 25-char logical line now needs 3 rows of 10
        // The reflow must not crash and must keep the engine valid.
        vt.input(QByteArray("ok"));
        QVERIFY(vt.screenText().contains("ok"));
        QVERIFY(vt.scrollbackCount() >= before);   // narrower ⇒ same-or-more rows
    }

    void reflowWidenReducesRows() {
        VtEngine vt(2, 10);
        vt.setScrollbackMax(100);
        vt.input(QByteArray("abcdefghijklmnopqrst\r\n"));  // 20 chars over 10 cols
        for (int i = 0; i < 4; ++i) vt.input(QByteArray("\r\n"));
        const int narrowRows = vt.scrollbackCount();
        vt.resize(2, 40);   // widen: the wrapped run collapses to fewer rows
        QVERIFY(vt.scrollbackCount() <= narrowRows);
    }

    void clearScrollbackDropsHistory() {
        VtEngine vt(3, 20);
        vt.setScrollbackMax(100);
        for (int i = 0; i < 20; ++i) vt.input(QByteArray("row\r\n"));
        QVERIFY(vt.scrollbackCount() > 0);
        vt.clearScrollback();
        QCOMPARE(vt.scrollbackCount(), 0);
    }

    void bracketedPasteModeTracks2004() {
        VtEngine vt(3, 20);
        QVERIFY(!vt.bracketedPaste());
        vt.input(QByteArray("\x1b[?2004h"));   // enable
        QVERIFY(vt.bracketedPaste());
        vt.input(QByteArray("\x1b[?2004l"));   // disable
        QVERIFY(!vt.bracketedPaste());
    }

    void bracketedPasteIgnoresOtherModes() {
        VtEngine vt(3, 20);
        vt.input(QByteArray("\x1b[?1049h"));   // altscreen, not 2004
        QVERIFY(!vt.bracketedPaste());
    }

    void bracketedPasteSpansChunks() {
        VtEngine vt(3, 20);
        vt.input(QByteArray("\x1b[?20"));      // split mid-sequence
        vt.input(QByteArray("04h"));
        QVERIFY(vt.bracketedPaste());
    }

    void mouseTrackingModes() {
        VtEngine vt(3, 20);
        QVERIFY(!vt.mouseEnabled());
        vt.input(QByteArray("\x1b[?1000h"));
        QCOMPARE(vt.mouseTracking(), VtEngine::MouseTracking::Normal);
        QVERIFY(vt.mouseEnabled());
        vt.input(QByteArray("\x1b[?1002h"));
        QCOMPARE(vt.mouseTracking(), VtEngine::MouseTracking::ButtonEvent);
        vt.input(QByteArray("\x1b[?1002l"));
        QVERIFY(!vt.mouseEnabled());
    }

    void mouseSgrEncoding() {
        VtEngine vt(3, 20);
        QCOMPARE(vt.mouseEncoding(), VtEngine::MouseEncoding::Default);
        vt.input(QByteArray("\x1b[?1006h"));
        QCOMPARE(vt.mouseEncoding(), VtEngine::MouseEncoding::Sgr);
        vt.input(QByteArray("\x1b[?1006l"));
        QCOMPARE(vt.mouseEncoding(), VtEngine::MouseEncoding::Default);
    }

    void combinedMouseAndPasteInOneSequence() {
        VtEngine vt(3, 20);
        vt.input(QByteArray("\x1b[?1002h\x1b[?1006h\x1b[?2004h"));
        QCOMPARE(vt.mouseTracking(), VtEngine::MouseTracking::ButtonEvent);
        QCOMPARE(vt.mouseEncoding(), VtEngine::MouseEncoding::Sgr);
        QVERIFY(vt.bracketedPaste());
    }
};

QTEST_APPLESS_MAIN(TestVtEngine)
#include "test_vtengine.moc"
