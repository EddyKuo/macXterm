#include "sftp/RemotePath.h"

namespace macxterm::sftp {

QString RemotePath::normalize(const QString& path) {
    if (path.isEmpty()) return QStringLiteral("/");
    const bool absolute = path.startsWith('/');
    const QStringList raw = path.split('/', Qt::SkipEmptyParts);
    QStringList out;
    for (const QString& seg : raw) {
        if (seg == ".") continue;
        if (seg == "..") {
            if (!out.isEmpty() && out.last() != "..") out.removeLast();
            else if (!absolute) out.append("..");
            // for absolute paths, ".." at root is ignored
            continue;
        }
        out.append(seg);
    }
    QString result = (absolute ? "/" : "") + out.join('/');
    if (result.isEmpty()) result = absolute ? QStringLiteral("/") : QStringLiteral(".");
    return result;
}

QString RemotePath::join(const QString& base, const QString& child) {
    if (child.startsWith('/')) return normalize(child);
    return normalize(base + "/" + child);
}

QString RemotePath::parent(const QString& path) {
    const QString n = normalize(path);
    if (n == "/") return n;
    const int slash = n.lastIndexOf('/');
    if (slash <= 0) return QStringLiteral("/");
    return n.left(slash);
}

QString RemotePath::baseName(const QString& path) {
    const QString n = normalize(path);
    if (n == "/") return {};
    return n.mid(n.lastIndexOf('/') + 1);
}

} // namespace macxterm::sftp
