#include "core/Session.h"
#include <QHash>

namespace macxterm::core {

static const QHash<SessionType, QString>& typeNames() {
    static const QHash<SessionType, QString> m = {
        {SessionType::Ssh, "SSH"},   {SessionType::Telnet, "Telnet"},
        {SessionType::Rsh, "RSH"},   {SessionType::Rlogin, "Rlogin"},
        {SessionType::Serial, "Serial"}, {SessionType::Mosh, "Mosh"},
        {SessionType::Sftp, "SFTP"}, {SessionType::Ftp, "FTP"},
        {SessionType::S3, "S3"},     {SessionType::Shell, "Shell"},
        {SessionType::Rdp, "RDP"},   {SessionType::Vnc, "VNC"},
        {SessionType::Xdmcp, "XDMCP"}, {SessionType::Browser, "Browser"},
        {SessionType::Unknown, "Unknown"},
    };
    return m;
}

QString sessionTypeToString(SessionType t) {
    return typeNames().value(t, "Unknown");
}

SessionType sessionTypeFromString(const QString& s) {
    const QString up = s.trimmed().toUpper();
    for (auto it = typeNames().constBegin(); it != typeNames().constEnd(); ++it) {
        if (it.value().toUpper() == up) return it.key();
    }
    return SessionType::Unknown;
}

int Session::port() const {
    bool ok = false;
    const int p = param("port").toInt(&ok);
    if (ok) return p;
    // Sensible protocol defaults matching MobaXterm.
    switch (m_type) {
        case SessionType::Ssh:
        case SessionType::Sftp:
        case SessionType::Mosh:   return 22;
        case SessionType::Telnet: return 23;
        case SessionType::Rlogin: return 513;
        case SessionType::Rsh:    return 514;
        case SessionType::Ftp:    return 21;
        case SessionType::Rdp:    return 3389;
        case SessionType::Vnc:    return 5900;
        case SessionType::Xdmcp:  return 177;
        default:                  return 0;
    }
}

} // namespace macxterm::core
