#pragma once
#include <QString>
#include <QList>

namespace macxterm::tools {

// One entry in a recursive folder comparison (MobaXterm's MobaFoldersDiff).
struct FolderDiffEntry {
    enum Kind { OnlyLeft, OnlyRight, Differ, Same };
    QString relPath;
    Kind kind = Same;
};

// Recursively compare two directory trees. Files present on only one side are
// OnlyLeft/OnlyRight; files on both are Differ (size or content differ) or Same.
// Content comparison is byte-exact. Pure filesystem logic — unit-tested with
// temp directories.
class FolderDiff {
public:
    static QList<FolderDiffEntry> compare(const QString& leftDir, const QString& rightDir,
                                          bool recursive = true);
};

} // namespace macxterm::tools
