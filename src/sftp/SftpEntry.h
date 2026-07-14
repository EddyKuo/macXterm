#pragma once
#include <QString>
#include <QList>

namespace macxterm::sftp {

// One entry in the SFTP browser's directory listing (Architecture §4 sftp/).
// The live listing comes from libssh2_sftp_readdir; these pure helpers model
// and present the entries so sorting/formatting is unit-testable.
struct SftpEntry {
    QString name;
    quint64 size = 0;
    bool isDir = false;
    quint32 permissions = 0;   // POSIX mode bits
    qint64  mtime = 0;

    // "rwxr-xr-x"-style permission string from the mode bits.
    QString permString() const;
    // Human-readable size ("1.5 KB").
    QString sizeString() const;
};

// Sort a listing the way a file browser does: directories first, then by name
// (case-insensitive). ".." always sorts to the very top.
QList<SftpEntry> sortListing(QList<SftpEntry> entries);

} // namespace macxterm::sftp
