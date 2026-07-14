#pragma once
#include <QString>
#include <QStringList>
#include <QList>

namespace macxterm::tools {

// Line-level text diff (MobaTextDiff parity, research §1.2). LCS-based; emits a
// sequence of operations. Pure and unit-testable.
struct DiffLine {
    enum Kind { Equal, Added, Removed };
    Kind kind;
    QString text;
};

class TextDiff {
public:
    // Diff two texts split into lines. Result is in output order (removed lines
    // from `a` appear before the added lines from `b` at each change point).
    static QList<DiffLine> diff(const QString& a, const QString& b);

    // Counts for a quick summary.
    static void counts(const QList<DiffLine>& d, int& added, int& removed);
};

} // namespace macxterm::tools
