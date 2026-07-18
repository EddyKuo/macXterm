#include "core/CygPath.h"

namespace macxterm::core {

QString CygPath::windowsToPosix(const QString& winPath) {
    if (winPath.isEmpty()) return winPath;
    QString p = winPath;
    p.replace('\\', '/');

    // UNC: \\host\share  ->  //host/share
    if (p.startsWith(QLatin1String("//"))) return p;

    // Drive-letter path: "C:/..." -> "/drives/c/...".
    if (p.size() >= 2 && p[1] == QLatin1Char(':') && p[0].isLetter()) {
        const QChar drive = p[0].toLower();
        QString rest = p.mid(2);                 // after "C:"
        while (rest.startsWith(QLatin1Char('/'))) rest.remove(0, 1);
        QString out = QStringLiteral("/drives/") + drive;
        if (!rest.isEmpty()) out += QLatin1Char('/') + rest;
        return out;
    }
    // Already POSIX-ish / relative: just normalised slashes.
    return p;
}

QString CygPath::posixToWindows(const QString& posixPath) {
    if (posixPath.isEmpty()) return posixPath;
    QString p = posixPath;
    p.replace('\\', '/');

    // Accept both /drives/<l>/ and the classic Cygwin /cygdrive/<l>/.
    QString rest;
    if (p.startsWith(QLatin1String("/drives/")))
        rest = p.mid(QStringLiteral("/drives/").size());
    else if (p.startsWith(QLatin1String("/cygdrive/")))
        rest = p.mid(QStringLiteral("/cygdrive/").size());
    else
        return p;   // not under the drives root — best-effort passthrough

    if (rest.isEmpty()) return p;
    const QChar drive = rest[0].toUpper();
    QString tail = rest.mid(1);                  // after "c"
    while (tail.startsWith(QLatin1Char('/'))) tail.remove(0, 1);
    QString out = QString(drive) + QLatin1String(":\\");
    if (!tail.isEmpty()) out += QString(tail).replace('/', '\\');
    return out;
}

} // namespace macxterm::core
