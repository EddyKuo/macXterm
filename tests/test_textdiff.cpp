#include "tools/TextDiff.h"
#include <QtTest/QtTest>

using namespace macxterm::tools;

class TestTextDiff : public QObject {
    Q_OBJECT
private slots:
    void identicalTextAllEqual() {
        auto d = TextDiff::diff("a\nb\nc", "a\nb\nc");
        int add, rem; TextDiff::counts(d, add, rem);
        QCOMPARE(add, 0);
        QCOMPARE(rem, 0);
        QCOMPARE(d.size(), 3);
    }

    void detectsAddedLine() {
        auto d = TextDiff::diff("a\nc", "a\nb\nc");
        int add, rem; TextDiff::counts(d, add, rem);
        QCOMPARE(add, 1);
        QCOMPARE(rem, 0);
    }

    void detectsRemovedLine() {
        auto d = TextDiff::diff("a\nb\nc", "a\nc");
        int add, rem; TextDiff::counts(d, add, rem);
        QCOMPARE(add, 0);
        QCOMPARE(rem, 1);
    }

    void detectsChangedLine() {
        auto d = TextDiff::diff("a\nB\nc", "a\nX\nc");
        int add, rem; TextDiff::counts(d, add, rem);
        QCOMPARE(add, 1);   // X added
        QCOMPARE(rem, 1);   // B removed
    }

    void preservesEqualContext() {
        auto d = TextDiff::diff("x\ny", "x\nz");
        QCOMPARE(d.first().kind, DiffLine::Equal);
        QCOMPARE(d.first().text, QStringLiteral("x"));
    }
};

QTEST_APPLESS_MAIN(TestTextDiff)
#include "test_textdiff.moc"
