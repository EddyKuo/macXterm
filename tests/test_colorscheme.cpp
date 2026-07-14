#include "term/ColorScheme.h"
#include <QtTest/QtTest>

using namespace macxterm::term;

class TestColorScheme : public QObject {
    Q_OBJECT
private slots:
    void darkDefaults() {
        ColorScheme s = ColorScheme::dark();
        QCOMPARE(s.name(), QStringLiteral("Dark"));
        QCOMPARE(s.background(), QColor(0, 0, 0));
        QCOMPARE(s.ansi(0), QColor(0, 0, 0));
    }

    void lightHasWhiteBackground() {
        ColorScheme s = ColorScheme::light();
        QCOMPARE(s.background(), QColor(255, 255, 255));
        QCOMPARE(s.foreground(), QColor(0, 0, 0));
    }

    void byNameFallsBackToDark() {
        QCOMPARE(ColorScheme::byName("nonsense").name(), QStringLiteral("Dark"));
        QCOMPARE(ColorScheme::byName("light").name(), QStringLiteral("Light"));
        QCOMPARE(ColorScheme::byName("solarized").name(), QStringLiteral("Solarized Dark"));
    }

    void ansiIndexWraps() {
        ColorScheme s = ColorScheme::dark();
        s.setAnsi(1, QColor(10, 20, 30));
        QCOMPARE(s.ansi(1), QColor(10, 20, 30));
        QCOMPARE(s.ansi(17), s.ansi(1));   // 17 & 0x0f == 1
    }
};

QTEST_APPLESS_MAIN(TestColorScheme)
#include "test_colorscheme.moc"
