#pragma once
#include <QString>
#include <QByteArray>

namespace macxterm::tools {

// FTP (RFC 959) control-channel command layer for the built-in light FTP server
// (research §1.1). Pure parse/format so the protocol state machine is
// unit-testable without sockets (the socket server wraps this in a later phase).
namespace ftp {

struct Command {
    QString verb;   // upper-cased, e.g. "USER", "RETR"
    QString arg;
    bool valid = false;
};

// Parse one control line ("RETR file.txt\r\n").
Command parse(const QByteArray& line);

// Format a reply line ("220 Welcome\r\n").
QByteArray reply(int code, const QString& text);

} // namespace ftp
} // namespace macxterm::tools
