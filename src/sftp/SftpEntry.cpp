#include "sftp/SftpEntry.h"
#include <algorithm>

namespace macxterm::sftp {

QString SftpEntry::permString() const {
    static const char* rwx[] = {"---","--x","-w-","-wx","r--","r-x","rw-","rwx"};
    QString s;
    s += isDir ? 'd' : '-';
    s += rwx[(permissions >> 6) & 7];
    s += rwx[(permissions >> 3) & 7];
    s += rwx[permissions & 7];
    return s;
}

QString SftpEntry::sizeString() const {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double v = static_cast<double>(size);
    int u = 0;
    while (v >= 1024.0 && u < 4) { v /= 1024.0; ++u; }
    if (u == 0) return QString::number(size) + " B";
    return QString::number(v, 'f', 1) + " " + units[u];
}

QList<SftpEntry> sortListing(QList<SftpEntry> entries) {
    std::sort(entries.begin(), entries.end(), [](const SftpEntry& a, const SftpEntry& b) {
        if (a.name == "..") return b.name != "..";
        if (b.name == "..") return false;
        if (a.isDir != b.isDir) return a.isDir;          // dirs first
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return entries;
}

} // namespace macxterm::sftp
