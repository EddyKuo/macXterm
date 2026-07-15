#include "sftp/RemoteTransfer.h"
#include "sftp/RemotePath.h"
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>

namespace macxterm::sftp {

qint64 downloadTree(IRemoteFs& fs, const QString& remote, const QString& local, bool isDir,
                    const std::function<bool()>& canceled,
                    const std::function<void(const QString&)>& onItem) {
    if (canceled && canceled()) return -1;
    if (!isDir) {
        if (onItem) onItem(remote);
        return fs.download(remote, local);
    }
    // Mirror the remote directory locally, then recurse into its entries.
    if (!QDir().mkpath(local)) return -1;
    QList<SftpEntry> entries;
    if (!fs.list(remote, entries)) return -1;
    qint64 total = 0;
    for (const SftpEntry& e : entries) {
        if (e.name == QStringLiteral(".") || e.name == QStringLiteral("..")) continue;
        if (canceled && canceled()) return -1;
        const qint64 n = downloadTree(fs, RemotePath::join(remote, e.name),
                                      QDir(local).filePath(e.name), e.isDir, canceled, onItem);
        if (n < 0) return -1;
        total += n;
    }
    return total;
}

qint64 uploadTree(IRemoteFs& fs, const QString& local, const QString& remote,
                  const std::function<bool()>& canceled,
                  const std::function<void(const QString&)>& onItem) {
    if (canceled && canceled()) return -1;
    QFileInfo fi(local);
    if (!fi.isDir()) {
        if (onItem) onItem(local);
        return fs.upload(local, remote);
    }
    fs.makeDir(remote);   // best-effort; may already exist
    qint64 total = 0;
    QDirIterator it(local, QDir::AllEntries | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        it.next();
        if (canceled && canceled()) return -1;
        const qint64 n = uploadTree(fs, it.filePath(),
                                    RemotePath::join(remote, it.fileName()), canceled, onItem);
        if (n < 0) return -1;
        total += n;
    }
    return total;
}

} // namespace macxterm::sftp
