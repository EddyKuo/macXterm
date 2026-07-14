#include "tools/TextDiff.h"
#include <vector>

namespace macxterm::tools {

QList<DiffLine> TextDiff::diff(const QString& a, const QString& b) {
    const QStringList A = a.split('\n');
    const QStringList B = b.split('\n');
    const int n = A.size(), m = B.size();

    // Classic LCS dynamic-programming table.
    std::vector<std::vector<int>> lcs(n + 1, std::vector<int>(m + 1, 0));
    for (int i = n - 1; i >= 0; --i)
        for (int j = m - 1; j >= 0; --j)
            lcs[i][j] = (A[i] == B[j]) ? lcs[i + 1][j + 1] + 1
                                       : std::max(lcs[i + 1][j], lcs[i][j + 1]);

    QList<DiffLine> out;
    int i = 0, j = 0;
    while (i < n && j < m) {
        if (A[i] == B[j]) {
            out.push_back({DiffLine::Equal, A[i]}); ++i; ++j;
        } else if (lcs[i + 1][j] >= lcs[i][j + 1]) {
            out.push_back({DiffLine::Removed, A[i]}); ++i;
        } else {
            out.push_back({DiffLine::Added, B[j]}); ++j;
        }
    }
    while (i < n) out.push_back({DiffLine::Removed, A[i++]});
    while (j < m) out.push_back({DiffLine::Added, B[j++]});
    return out;
}

void TextDiff::counts(const QList<DiffLine>& d, int& added, int& removed) {
    added = removed = 0;
    for (const DiffLine& l : d) {
        if (l.kind == DiffLine::Added) ++added;
        else if (l.kind == DiffLine::Removed) ++removed;
    }
}

} // namespace macxterm::tools
