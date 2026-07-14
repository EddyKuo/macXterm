#include "tools/FtpCommand.h"

namespace macxterm::tools::ftp {

Command parse(const QByteArray& line) {
    Command c;
    QString s = QString::fromUtf8(line).trimmed();
    if (s.isEmpty()) return c;
    const int sp = s.indexOf(' ');
    if (sp < 0) {
        c.verb = s.toUpper();
    } else {
        c.verb = s.left(sp).toUpper();
        c.arg = s.mid(sp + 1);
    }
    c.valid = !c.verb.isEmpty();
    return c;
}

QByteArray reply(int code, const QString& text) {
    return QByteArray::number(code) + " " + text.toUtf8() + "\r\n";
}

} // namespace macxterm::tools::ftp
