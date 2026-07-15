#include "tools/FtpServer.h"
#include "tools/FtpCommand.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>

namespace macxterm::tools {

FtpServer::FtpServer(QObject* parent) : QObject(parent) {
    m_root = QDir::currentPath();
}
FtpServer::~FtpServer() { stop(); }

bool FtpServer::start(quint16 port, const QString& bindAddr) {
    stop();
    m_bindAddr = bindAddr;
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &FtpServer::onNewConnection);
    return m_server->listen(QHostAddress(bindAddr), port);
}

void FtpServer::stop() {
    if (m_server) { m_server->deleteLater(); m_server = nullptr; }
    m_conns.clear();
}

quint16 FtpServer::port() const { return m_server ? m_server->serverPort() : 0; }
bool FtpServer::isRunning() const { return m_server && m_server->isListening(); }

void FtpServer::onNewConnection() {
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket* c = m_server->nextPendingConnection();
        m_conns.insert(c, Conn{});
        c->write(ftp::reply(220, "macXterm FTP ready"));
        auto drain = [this, c] {
            while (c->canReadLine()) handleLine(c, c->readLine());
        };
        connect(c, &QTcpSocket::readyRead, c, drain);
        connect(c, &QTcpSocket::disconnected, this, [this, c] {
            if (m_conns.contains(c)) {
                if (QTcpServer* d = m_conns[c].dataSrv) d->deleteLater();
                m_conns.remove(c);
            }
            c->deleteLater();
        });
        if (c->canReadLine()) drain();
    }
}

// Resolve an FTP path argument against the connection's cwd to an absolute local
// path, clamped inside the served root (no escaping via "..").
QString FtpServer::resolve(const Conn& conn, const QString& arg) const {
    QString rel = arg.startsWith('/') ? arg : (conn.cwd + '/' + arg);
    const QString abs = QDir::cleanPath(m_root + '/' + rel);
    const QString rootAbs = QDir::cleanPath(m_root);
    if (abs != rootAbs && !abs.startsWith(rootAbs + '/')) return rootAbs;  // clamp
    return abs;
}

QTcpSocket* FtpServer::acceptData(Conn& conn) {
    if (!conn.dataSrv) return nullptr;
    if (!conn.dataSrv->hasPendingConnections())
        conn.dataSrv->waitForNewConnection(3000);
    return conn.dataSrv->nextPendingConnection();
}

void FtpServer::handleLine(QTcpSocket* c, const QByteArray& line) {
    const ftp::Command cmd = ftp::parse(line);
    if (!cmd.valid) { c->write(ftp::reply(500, "Syntax error")); return; }
    if (!m_conns.contains(c)) m_conns.insert(c, Conn{});
    Conn& conn = m_conns[c];

    const QString& v = cmd.verb;
    if (v == "USER")      c->write(ftp::reply(331, "User name okay, need password"));
    else if (v == "PASS") c->write(ftp::reply(230, "User logged in"));
    else if (v == "SYST") c->write(ftp::reply(215, "UNIX Type: L8"));
    else if (v == "TYPE") c->write(ftp::reply(200, "Type set to " + cmd.arg));
    else if (v == "NOOP") c->write(ftp::reply(200, "OK"));
    else if (v == "PWD")
        c->write(ftp::reply(257, '"' + conn.cwd + "\" is current directory"));
    else if (v == "CWD") {
        const QString abs = resolve(conn, cmd.arg);
        if (QFileInfo(abs).isDir()) {
            const QString rootAbs = QDir::cleanPath(m_root);
            QString rel = abs.mid(rootAbs.size());
            conn.cwd = rel.isEmpty() ? QStringLiteral("/") : rel;
            c->write(ftp::reply(250, "Directory changed to " + conn.cwd));
        } else {
            c->write(ftp::reply(550, "No such directory"));
        }
    }
    else if (v == "PASV") {
        if (conn.dataSrv) conn.dataSrv->deleteLater();
        conn.dataSrv = new QTcpServer(this);
        if (!conn.dataSrv->listen(QHostAddress(m_bindAddr), 0)) {
            c->write(ftp::reply(425, "Cannot open data connection"));
            return;
        }
        const quint16 p = conn.dataSrv->serverPort();
        // 127,0,0,1,p1,p2 — high/low bytes of the data port.
        const QStringList oct = m_bindAddr.split('.');
        c->write(ftp::reply(227, QStringLiteral("Entering Passive Mode (%1,%2,%3,%4,%5,%6)")
            .arg(oct.value(0), oct.value(1), oct.value(2), oct.value(3))
            .arg(p >> 8).arg(p & 0xff)));
    }
    else if (v == "LIST" || v == "NLST") {
        const QString abs = resolve(conn, cmd.arg);
        c->write(ftp::reply(150, "Opening data connection for directory list"));
        QTcpSocket* data = acceptData(conn);
        if (!data) { c->write(ftp::reply(425, "No data connection")); return; }
        QByteArray body;
        for (const QFileInfo& fi : QDir(abs).entryInfoList(
                 QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name)) {
            if (v == "NLST") {
                body += fi.fileName().toUtf8() + "\r\n";
            } else {
                const QChar type = fi.isDir() ? QChar('d') : QChar('-');
                body += QStringLiteral("%1rw-r--r-- 1 owner group %2 %3 %4\r\n")
                    .arg(type)
                    .arg(fi.size(), 12)
                    .arg(fi.lastModified().toString(QStringLiteral("MMM dd hh:mm")),
                         fi.fileName()).toUtf8();
            }
        }
        data->write(body);
        data->waitForBytesWritten(3000);
        data->disconnectFromHost();
        data->deleteLater();
        c->write(ftp::reply(226, "Transfer complete"));
    }
    else if (v == "RETR") {
        const QString abs = resolve(conn, cmd.arg);
        QFile f(abs);
        if (!f.open(QIODevice::ReadOnly)) { c->write(ftp::reply(550, "No such file")); return; }
        c->write(ftp::reply(150, "Opening data connection"));
        QTcpSocket* data = acceptData(conn);
        if (!data) { c->write(ftp::reply(425, "No data connection")); return; }
        data->write(f.readAll());
        data->waitForBytesWritten(5000);
        data->disconnectFromHost();
        data->deleteLater();
        c->write(ftp::reply(226, "Transfer complete"));
    }
    else if (v == "STOR") {
        const QString abs = resolve(conn, cmd.arg);
        c->write(ftp::reply(150, "Opening data connection"));
        c->flush();   // flush before blocking on the data read, or the client
                      // waits for 150 while we wait for its data (deadlock)
        QTcpSocket* data = acceptData(conn);
        if (!data) { c->write(ftp::reply(425, "No data connection")); return; }
        QByteArray body;
        // Read until the client closes the data connection.
        while (data->state() == QAbstractSocket::ConnectedState) {
            if (!data->waitForReadyRead(5000)) break;
            body += data->readAll();
        }
        body += data->readAll();
        QFile f(abs);
        const bool ok = f.open(QIODevice::WriteOnly) && f.write(body) == body.size();
        data->deleteLater();
        c->write(ok ? ftp::reply(226, "Transfer complete")
                    : ftp::reply(550, "Cannot store file"));
    }
    else if (v == "MKD") {
        c->write(QDir().mkpath(resolve(conn, cmd.arg))
                     ? ftp::reply(257, '"' + cmd.arg + "\" created")
                     : ftp::reply(550, "Cannot create directory"));
    }
    else if (v == "RMD") {
        c->write(QDir(resolve(conn, cmd.arg)).removeRecursively()
                     ? ftp::reply(250, "Directory removed") : ftp::reply(550, "Cannot remove"));
    }
    else if (v == "DELE") {
        c->write(QFile::remove(resolve(conn, cmd.arg))
                     ? ftp::reply(250, "File deleted") : ftp::reply(550, "Cannot delete"));
    }
    else if (v == "RNFR") {
        conn.storeTarget = resolve(conn, cmd.arg);   // reuse field to hold the rename source
        c->write(QFileInfo::exists(conn.storeTarget)
                     ? ftp::reply(350, "Ready for RNTO") : ftp::reply(550, "No such file"));
    }
    else if (v == "RNTO") {
        const bool ok = !conn.storeTarget.isEmpty()
                        && QFile::rename(conn.storeTarget, resolve(conn, cmd.arg));
        conn.storeTarget.clear();
        c->write(ok ? ftp::reply(250, "Renamed") : ftp::reply(550, "Rename failed"));
    }
    else if (v == "SITE") {
        c->write(ftp::reply(200, "OK"));   // accept SITE CHMOD etc. as a no-op
    }
    else if (v == "QUIT") { c->write(ftp::reply(221, "Goodbye")); c->disconnectFromHost(); }
    else                  c->write(ftp::reply(502, "Command not implemented"));
}

} // namespace macxterm::tools
