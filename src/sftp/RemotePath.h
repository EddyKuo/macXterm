#pragma once
#include <QString>
#include <QStringList>

namespace macxterm::sftp {

// POSIX-style remote path helpers for the SFTP browser (Architecture §4 sftp/).
// Remote paths are always '/'-separated regardless of the local OS — this is
// why we can't use QDir/QFileInfo (which follow local separators).
class RemotePath {
public:
    // Normalize: collapse duplicate slashes, resolve "." and ".." segments,
    // keep a leading '/'. Result never has a trailing slash except root "/".
    static QString normalize(const QString& path);

    // Join base + child, normalizing the result. Absolute child wins.
    static QString join(const QString& base, const QString& child);

    // Parent directory ("/a/b" -> "/a", "/" -> "/").
    static QString parent(const QString& path);

    // Final component ("/a/b.txt" -> "b.txt", "/" -> "").
    static QString baseName(const QString& path);
};

} // namespace macxterm::sftp
