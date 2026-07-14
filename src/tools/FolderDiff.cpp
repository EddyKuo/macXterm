#include "tools/FolderDiff.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSet>
#include <algorithm>

namespace macxterm::tools {
namespace {
// Collect file paths relative to `root`.
QSet<QString> relFiles(const QString& root, bool recursive) {
    QSet<QString> out;
    QDir base(root);
    QDirIterator::IteratorFlags flags = recursive ? QDirIterator::Subdirectories
                                                   : QDirIterator::NoIteratorFlags;
    QDirIterator it(root, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, flags);
    while (it.hasNext()) {
        it.next();
        out.insert(base.relativeFilePath(it.filePath()));
    }
    return out;
}

bool sameContent(const QString& a, const QString& b) {
    QFile fa(a), fb(b);
    if (!fa.open(QIODevice::ReadOnly) || !fb.open(QIODevice::ReadOnly)) return false;
    if (fa.size() != fb.size()) return false;
    constexpr qint64 kChunk = 65536;
    while (!fa.atEnd()) {
        if (fa.read(kChunk) != fb.read(kChunk)) return false;
    }
    return true;
}
} // namespace

QList<FolderDiffEntry> FolderDiff::compare(const QString& leftDir, const QString& rightDir,
                                           bool recursive) {
    const QSet<QString> left = relFiles(leftDir, recursive);
    const QSet<QString> right = relFiles(rightDir, recursive);

    QSet<QString> all = left;
    all.unite(right);
    QList<QString> sorted(all.begin(), all.end());
    std::sort(sorted.begin(), sorted.end());

    QList<FolderDiffEntry> out;
    for (const QString& rel : sorted) {
        const bool inL = left.contains(rel);
        const bool inR = right.contains(rel);
        FolderDiffEntry e;
        e.relPath = rel;
        if (inL && !inR) e.kind = FolderDiffEntry::OnlyLeft;
        else if (!inL && inR) e.kind = FolderDiffEntry::OnlyRight;
        else e.kind = sameContent(QDir(leftDir).filePath(rel), QDir(rightDir).filePath(rel))
                          ? FolderDiffEntry::Same : FolderDiffEntry::Differ;
        out.push_back(e);
    }
    return out;
}

} // namespace macxterm::tools
