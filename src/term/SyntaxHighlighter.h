#pragma once
#include <QString>
#include <QColor>
#include <QVector>
#include <QRegularExpression>

namespace macxterm::term {

// A colored span within a line: [start, start+length) painted in `color`.
struct HighlightSpan {
    int start = 0;
    int length = 0;
    QColor color;
};

// Terminal keyword/regex highlighter (MobaXterm "syntax highlighting"): applies
// coloration to matched substrings of a line even when the remote end sends no
// colors. Rules are ordered; earlier rules win on overlap. Pure and
// unit-testable — the terminal widget calls highlight() per visible line.
class SyntaxHighlighter {
public:
    SyntaxHighlighter() = default;

    bool enabled() const { return m_enabled; }
    void setEnabled(bool on) { m_enabled = on; }

    // Add a rule. An invalid pattern is ignored.
    void addRule(const QString& pattern, const QColor& color,
                 QRegularExpression::PatternOptions opts = QRegularExpression::NoPatternOption);
    void clearRules() { m_rules.clear(); }
    int ruleCount() const { return m_rules.size(); }

    // Install a set of generally-useful defaults (errors, warnings, numbers,
    // IPs, URLs, quoted strings).
    void loadDefaults();

    // Compute non-overlapping color spans for one line, left to right. Returns
    // an empty vector when disabled or when nothing matches.
    QVector<HighlightSpan> highlight(const QString& line) const;

private:
    struct Rule { QRegularExpression re; QColor color; };
    QVector<Rule> m_rules;
    bool m_enabled = false;
};

} // namespace macxterm::term
