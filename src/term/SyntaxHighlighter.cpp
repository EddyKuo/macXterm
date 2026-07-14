#include "term/SyntaxHighlighter.h"
#include <algorithm>

namespace macxterm::term {

void SyntaxHighlighter::addRule(const QString& pattern, const QColor& color,
                                QRegularExpression::PatternOptions opts) {
    QRegularExpression re(pattern, opts);
    if (!re.isValid()) return;
    m_rules.push_back({re, color});
}

void SyntaxHighlighter::loadDefaults() {
    // Order matters: earlier rules win on overlap.
    addRule(QStringLiteral("\\b(error|fail(ed|ure)?|fatal|denied|refused|panic)\\b"),
            QColor(0xff, 0x55, 0x55), QRegularExpression::CaseInsensitiveOption);
    addRule(QStringLiteral("\\b(warn(ing)?|deprecated)\\b"),
            QColor(0xff, 0xcc, 0x33), QRegularExpression::CaseInsensitiveOption);
    addRule(QStringLiteral("\\b(ok|success|done|passed|listening|started)\\b"),
            QColor(0x66, 0xdd, 0x66), QRegularExpression::CaseInsensitiveOption);
    // IPv4 addresses.
    addRule(QStringLiteral("\\b\\d{1,3}(?:\\.\\d{1,3}){3}\\b"), QColor(0x66, 0xcc, 0xff));
    // URLs.
    addRule(QStringLiteral("\\b[a-z]+://[^\\s]+"), QColor(0x88, 0xaa, 0xff),
            QRegularExpression::CaseInsensitiveOption);
    // Double- or single-quoted strings.
    addRule(QStringLiteral("\"[^\"]*\"|'[^']*'"), QColor(0xcc, 0x99, 0x66));
    // Numbers.
    addRule(QStringLiteral("\\b\\d+\\b"), QColor(0xaa, 0xbb, 0xcc));
}

QVector<HighlightSpan> SyntaxHighlighter::highlight(const QString& line) const {
    QVector<HighlightSpan> spans;
    if (!m_enabled || line.isEmpty() || m_rules.isEmpty()) return spans;

    // Track which columns are already colored so earlier rules win on overlap.
    QVector<bool> taken(line.size(), false);
    for (const Rule& rule : m_rules) {
        auto it = rule.re.globalMatch(line);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            const int start = m.capturedStart();
            const int len = m.capturedLength();
            if (len <= 0) continue;
            // Skip if any covered column is already taken.
            bool free = true;
            for (int i = start; i < start + len && i < taken.size(); ++i)
                if (taken[i]) { free = false; break; }
            if (!free) continue;
            for (int i = start; i < start + len && i < taken.size(); ++i) taken[i] = true;
            spans.push_back({start, len, rule.color});
        }
    }
    std::sort(spans.begin(), spans.end(),
              [](const HighlightSpan& a, const HighlightSpan& b) { return a.start < b.start; });
    return spans;
}

} // namespace macxterm::term
