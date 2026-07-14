#include "term/VtEngine.h"
#include <QtTest/QtTest>

using namespace macxterm;

// Regression test for CJK mojibake: double-width (CJK) glyphs occupy two
// terminal cells; libvterm marks the right-hand cell with chars[0] == -1.
// Treating that as a code point put a U+FFFD after every wide character. The VT
// engine must skip continuation cells so wide characters survive intact.
class TestCjk : public QObject {
    Q_OBJECT
private slots:
    void wideCharsHaveNoReplacementChar() {
        term::VtEngine vt(5, 40);
        vt.input(QByteArray("\xE4\xBD\xA0\xE5\xA5\xBD\xE4\xB8\xAD\xE6\x96\x87")); // 你好中文
        const QString txt = vt.screenText();

        // The four CJK code points are present...
        QVERIFY(txt.contains(QChar(0x4F60)));  // 你
        QVERIFY(txt.contains(QChar(0x597D)));  // 好
        QVERIFY(txt.contains(QChar(0x4E2D)));  // 中
        QVERIFY(txt.contains(QChar(0x6587)));  // 文
        // ...and NO replacement character was emitted for the continuation cells.
        QVERIFY(!txt.contains(QChar(QChar::ReplacementCharacter)));

        // Order preserved (spaces between are blank continuation cells).
        const int a = txt.indexOf(QChar(0x4F60));
        const int d = txt.indexOf(QChar(0x6587));
        QVERIFY(a >= 0 && d > a);
    }
};

QTEST_APPLESS_MAIN(TestCjk)
#include "test_cjk_probe.moc"
