#include "term/SyntaxHighlighter.h"
#include <QtTest/QtTest>

using namespace macxterm::term;

class TestHighlighter : public QObject {
    Q_OBJECT
private slots:
    void disabledYieldsNothing() {
        SyntaxHighlighter h;
        h.loadDefaults();                         // rules present but disabled
        QVERIFY(h.highlight(QStringLiteral("error at 10.0.0.1")).isEmpty());
    }

    void matchesKeywordAndIp() {
        SyntaxHighlighter h;
        h.loadDefaults();
        h.setEnabled(true);
        const auto spans = h.highlight(QStringLiteral("ERROR connecting to 10.0.0.1"));
        QVERIFY(spans.size() >= 2);
        // Spans are returned left-to-right.
        for (int i = 1; i < spans.size(); ++i)
            QVERIFY(spans[i].start >= spans[i - 1].start);
        // First span covers "ERROR" (case-insensitive).
        QCOMPARE(spans.first().start, 0);
        QCOMPARE(spans.first().length, 5);
    }

    void customRuleColors() {
        SyntaxHighlighter h;
        h.setEnabled(true);
        h.addRule(QStringLiteral("foo"), QColor(1, 2, 3));
        const auto spans = h.highlight(QStringLiteral("a foo b foo"));
        QCOMPARE(spans.size(), 2);
        QCOMPARE(spans[0].start, 2);
        QCOMPARE(spans[1].start, 8);
        QCOMPARE(spans[0].color, QColor(1, 2, 3));
    }

    void earlierRuleWinsOverlap() {
        SyntaxHighlighter h;
        h.setEnabled(true);
        h.addRule(QStringLiteral("abcd"), QColor(10, 0, 0));   // rule 1
        h.addRule(QStringLiteral("bc"), QColor(0, 10, 0));      // rule 2 overlaps
        const auto spans = h.highlight(QStringLiteral("abcd"));
        QCOMPARE(spans.size(), 1);
        QCOMPARE(spans[0].color, QColor(10, 0, 0));
    }

    void invalidPatternIgnored() {
        SyntaxHighlighter h;
        const int before = h.ruleCount();
        h.addRule(QStringLiteral("("), QColor(0, 0, 0));   // unbalanced → invalid
        QCOMPARE(h.ruleCount(), before);
    }
};

QTEST_MAIN(TestHighlighter)
#include "test_highlighter.moc"
