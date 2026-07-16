#include "sftp/FtpClient.h"
#include "sftp/RemotePath.h"
#include <QTcpSocket>
#include <QHostAddress>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QRegularExpression>

namespace macxterm::sftp {

QList<SftpEntry> parseFtpList(const QByteArray& listing) {
    QList<SftpEntry> out;
    const QList<QByteArray> lines = listing.split('\n');
    for (const QByteArray& raw : lines) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (line.isEmpty()) continue;
        // Unix long format: "drwxr-xr-x 1 owner group SIZE Mon dd hh:mm name".
        const QStringList f = line.split(QRegularExpression(QStringLiteral("\\s+")),
                                         Qt::SkipEmptyParts);
        if (f.size() < 9) continue;                 // not a long-format line
        const QString& perms = f.first();
        if (perms.size() < 10) continue;
        SftpEntry e;
        e.isDir = perms.startsWith('d');
        e.size = f.at(4).toULongLong();
        // Name is everything after the 8 leading fields (handles spaces in names).
        int nameStart = 0, field = 0;
        for (int i = 0; i < line.size() && field < 8; ++i) {
            if (line[i] == ' ' && (i == 0 || line[i - 1] != ' ')) ++field;
            if (field == 8) { nameStart = i + 1; break; }
        }
        e.name = line.mid(nameStart).trimmed();
        if (e.name.isEmpty() || e.name == QStringLiteral(".") || e.name == QStringLiteral(".."))
            continue;
        // Map rwx perm string to mode bits (best-effort, for the perms column).
        unsigned int mode = 0;
        for (int i = 1; i <= 9 && i < perms.size(); ++i)
            if (perms[i] != '-') mode |= (1u << (9 - i));
        e.permissions = mode;
        out.append(e);
    }
    return sortListing(out);
}

FtpClient::FtpClient(QObject* parent) : QObject(parent) {}
FtpClient::~FtpClient() { disconnectSession(); }

bool FtpClient::isReady() const {
    return m_ctrl && m_ctrl->state() == QAbstractSocket::ConnectedState;
}

int FtpClient::readReply(QString* text) {
    // A reply is one or more lines; the final line is "NNN <text>" (space),
    // continuation lines are "NNN-<text>" (hyphen).
    QString acc;
    int code = 0;
    while (m_ctrl) {
        if (!m_ctrl->canReadLine() && !m_ctrl->waitForReadyRead(5000)) break;
        while (m_ctrl->canReadLine()) {
            const QString line = QString::fromUtf8(m_ctrl->readLine());
            acc += line;
            // A reply line carries a code only when it starts with 3 digits.
            // The block ends on the first "NNN <text>" line whose code matches
            // the code captured from the first line — NOT merely any line with a
            // space at index 3 (a continuation like "The service..." also has one).
            const bool hasCode = line.size() >= 3 && line[0].isDigit()
                                 && line[1].isDigit() && line[2].isDigit();
            if (hasCode && code == 0) code = line.left(3).toInt();
            if (hasCode && line.size() >= 4 && line[3] == ' '
                && line.left(3).toInt() == code) {
                if (text) *text = acc.trimmed();
                return code;
            }
        }
    }
    if (text) *text = acc.trimmed();
    return code;
}

int FtpClient::sendCmd(const QString& verb, const QString& arg, QString* reply) {
    if (!m_ctrl) return 0;
    QByteArray cmd = verb.toUtf8();
    if (!arg.isEmpty()) cmd += ' ' + arg.toUtf8();
    cmd += "\r\n";
    m_ctrl->write(cmd);
    m_ctrl->waitForBytesWritten(3000);
    return readReply(reply);
}

bool FtpClient::connectSession(const core::Session& session) {
    disconnectSession();
    m_ctrl = new QTcpSocket(this);
    const int port = session.port() > 0 ? session.port() : 21;
    m_ctrl->connectToHost(session.host(), static_cast<quint16>(port));
    if (!m_ctrl->waitForConnected(5000)) { emit error(QStringLiteral("FTP connect failed")); return false; }
    if (readReply(nullptr) / 100 != 2) { emit error(QStringLiteral("No FTP greeting")); return false; }

    const QString user = session.username().isEmpty() ? QStringLiteral("anonymous")
                                                      : session.username();
    int code = sendCmd(QStringLiteral("USER"), user);
    if (code == 331)   // password required
        code = sendCmd(QStringLiteral("PASS"), session.param("password"));
    if (code / 100 != 2) { emit error(QStringLiteral("FTP login failed")); return false; }
    sendCmd(QStringLiteral("TYPE"), QStringLiteral("I"));   // binary
    QString pwd;
    if (sendCmd(QStringLiteral("PWD"), QString(), &pwd) == 257) {
        const int a = pwd.indexOf('"'), b = pwd.lastIndexOf('"');
        if (a >= 0 && b > a) m_cwd = pwd.mid(a + 1, b - a - 1);
    }
    return true;
}

void FtpClient::disconnectSession() {
    if (m_ctrl) {
        if (m_ctrl->state() == QAbstractSocket::ConnectedState) {
            m_ctrl->write("QUIT\r\n");
            m_ctrl->waitForBytesWritten(1000);
        }
        m_ctrl->disconnectFromHost();
        m_ctrl->deleteLater();
        m_ctrl = nullptr;
    }
}

QTcpSocket* FtpClient::openPasv() {
    QString reply;
    if (sendCmd(QStringLiteral("PASV"), QString(), &reply) != 227) return nullptr;
    // Parse "227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)".
    const int a = reply.indexOf('('), b = reply.indexOf(')');
    if (a < 0 || b < a) return nullptr;
    const QStringList n = reply.mid(a + 1, b - a - 1).split(',');
    if (n.size() < 6) return nullptr;
    const QString host = QStringLiteral("%1.%2.%3.%4").arg(n[0], n[1], n[2], n[3]);
    const quint16 port = static_cast<quint16>((n[4].toInt() << 8) | n[5].toInt());
    auto* data = new QTcpSocket(this);
    data->connectToHost(host, port);
    if (!data->waitForConnected(5000)) { data->deleteLater(); return nullptr; }
    return data;
}

bool FtpClient::list(const QString& path, QList<SftpEntry>& out) {
    if (!isReady()) return false;
    QTcpSocket* data = openPasv();
    if (!data) { emit error(QStringLiteral("PASV failed")); return false; }
    // LIST with an explicit path (server resolves it) — do NOT CWD, or later
    // RETR/STOR of paths built relative to the caller's base would double up.
    const int code = sendCmd(QStringLiteral("LIST"), path);
    if (code != 150 && code != 125) { data->deleteLater(); emit error(QStringLiteral("LIST failed")); return false; }
    QByteArray body;
    while (data->state() == QAbstractSocket::ConnectedState) {
        if (!data->waitForReadyRead(5000)) break;
        body += data->readAll();
    }
    body += data->readAll();
    data->deleteLater();
    readReply(nullptr);   // 226 Transfer complete
    out = parseFtpList(body);
    return true;
}

qint64 FtpClient::download(const QString& remotePath, const QString& localPath) {
    if (!isReady()) return -1;
    QTcpSocket* data = openPasv();
    if (!data) return -1;
    if (sendCmd(QStringLiteral("RETR"), remotePath) / 100 != 1) { data->deleteLater(); return -1; }
    QFile f(localPath);
    if (!f.open(QIODevice::WriteOnly)) { data->deleteLater(); return -1; }
    qint64 total = 0;
    while (data->state() == QAbstractSocket::ConnectedState) {
        if (!data->waitForReadyRead(5000)) break;
        const QByteArray chunk = data->readAll();
        f.write(chunk);
        total += chunk.size();
    }
    const QByteArray tail = data->readAll();
    f.write(tail); total += tail.size();
    data->deleteLater();
    readReply(nullptr);   // 226
    return total;
}

qint64 FtpClient::upload(const QString& localPath, const QString& remotePath) {
    if (!isReady()) return -1;
    QFile f(localPath);
    if (!f.open(QIODevice::ReadOnly)) return -1;
    QTcpSocket* data = openPasv();
    if (!data) return -1;
    if (sendCmd(QStringLiteral("STOR"), remotePath) / 100 != 1) { data->deleteLater(); return -1; }
    const QByteArray body = f.readAll();
    data->write(body);
    data->waitForBytesWritten(10000);
    data->disconnectFromHost();
    data->deleteLater();
    readReply(nullptr);   // 226
    return body.size();
}

QString FtpClient::realpath(const QString& path) {
    return path == QStringLiteral(".") ? m_cwd : path;
}
bool FtpClient::makeDir(const QString& path, unsigned int) { return sendCmd(QStringLiteral("MKD"), path) / 100 == 2; }
bool FtpClient::removeFile(const QString& path) { return sendCmd(QStringLiteral("DELE"), path) / 100 == 2; }
bool FtpClient::removeDir(const QString& path) { return sendCmd(QStringLiteral("RMD"), path) / 100 == 2; }
bool FtpClient::rename(const QString& from, const QString& to) {
    return sendCmd(QStringLiteral("RNFR"), from) == 350 && sendCmd(QStringLiteral("RNTO"), to) / 100 == 2;
}
bool FtpClient::chmod(const QString& path, unsigned int mode) {
    return sendCmd(QStringLiteral("SITE"),
                   QStringLiteral("CHMOD %1 %2").arg(mode, 0, 8).arg(path)) / 100 == 2;
}

} // namespace macxterm::sftp
