#include "term/VtEngine.h"
#include "term/ScreenBuffer.h"
#include <QtTest/QtTest>

using namespace macxterm::term;

// VtEngine must translate SGR color sequences into the Cell color model:
// 16-color → Ansi index, 256-color cube → RGB, 24-bit → RGB.
class TestTrueColor : public QObject {
    Q_OBJECT
private:
    const Cell& firstCell(VtEngine& vt) { return vt.screen().at(0, 0); }

private slots:
    void ansi16IsIndexed() {
        VtEngine vt;
        vt.input(QByteArray("\x1b[31mX"));            // SGR 31 = red (index 1)
        const Cell& c = firstCell(vt);
        QCOMPARE(c.ch, char32_t('X'));
        QCOMPARE(c.fgKind, CellColor::Ansi);
        QCOMPARE(int(c.fgIndex), 1);
    }

    void extended256CubeIsRgb() {
        VtEngine vt;
        // SGR 38;5;196 = 256-color index 196 → cube (196-16=180): r=5,g=0,b=0 → (255,0,0)
        vt.input(QByteArray("\x1b[38;5;196mX"));
        const Cell& c = firstCell(vt);
        QCOMPARE(c.fgKind, CellColor::Rgb);
        QCOMPARE(c.fgRgb, 0xff0000u);
    }

    void extended256GrayIsRgb() {
        VtEngine vt;
        // index 232 = grayscale start → 8 (0x08); component 8+10*0=8 → 0x080808
        vt.input(QByteArray("\x1b[38;5;232mX"));
        const Cell& c = firstCell(vt);
        QCOMPARE(c.fgKind, CellColor::Rgb);
        QCOMPARE(c.fgRgb, 0x080808u);
    }

    void trueColorIsRgb() {
        VtEngine vt;
        // SGR 38;2;10;20;30 = 24-bit RGB(10,20,30)
        vt.input(QByteArray("\x1b[38;2;10;20;30mX"));
        const Cell& c = firstCell(vt);
        QCOMPARE(c.fgKind, CellColor::Rgb);
        QCOMPARE(c.fgRgb, (10u << 16) | (20u << 8) | 30u);
    }

    void trueColorBackground() {
        VtEngine vt;
        vt.input(QByteArray("\x1b[48;2;1;2;3mX"));    // background 24-bit
        const Cell& c = firstCell(vt);
        QCOMPARE(c.bgKind, CellColor::Rgb);
        QCOMPARE(c.bgRgb, (1u << 16) | (2u << 8) | 3u);
    }

    void resetGoesBackToDefault() {
        VtEngine vt;
        vt.input(QByteArray("\x1b[31mX\x1b[0mY"));     // red X, reset, default Y
        QCOMPARE(vt.screen().at(0, 1).fgKind, CellColor::Default);
    }

    void cjkIsDoubleWidth() {
        VtEngine vt;
        vt.input(QString::fromUtf8("星A").toUtf8());   // 星 is East-Asian wide
        const Cell& wide = vt.screen().at(0, 0);
        QVERIFY(wide.wide);                            // primary cell marked wide
        QCOMPARE(wide.ch, char32_t(0x661F));              // 星
        // 'A' lands two columns over (col 1 is the skipped continuation cell).
        QCOMPARE(vt.screen().at(0, 2).ch, char32_t('A'));
        QVERIFY(!vt.screen().at(0, 2).wide);
    }
};

QTEST_MAIN(TestTrueColor)
#include "test_truecolor.moc"
