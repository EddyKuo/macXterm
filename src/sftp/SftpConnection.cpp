#include "sftp/SftpConnection.h"
#include <libssh2.h>
#include <libssh2_sftp.h>
#include <QFile>

namespace macxterm::sftp {

SftpConnection::SftpConnection(QObject* parent) : QObject(parent) {}
SftpConnection::~SftpConnection() { detach(); }

bool SftpConnection::attach(LIBSSH2_SESSION* session, int socketFd) {
    m_session = session;
    m_sock = socketFd;
    m_sftp = libssh2_sftp_init(session);
    if (!m_sftp) { emit error(QStringLiteral("Failed to initialize SFTP subsystem")); return false; }
    return true;
}

void SftpConnection::detach() {
    if (m_sftp) { libssh2_sftp_shutdown(m_sftp); m_sftp = nullptr; }
    m_session = nullptr;
    m_sock = -1;
}

bool SftpConnection::list(const QString& path, QList<SftpEntry>& out) {
    if (!m_sftp) return false;
    LIBSSH2_SFTP_HANDLE* dir = libssh2_sftp_opendir(m_sftp, path.toUtf8().constData());
    if (!dir) { emit error(QStringLiteral("Cannot open directory: %1").arg(path)); return false; }

    QList<SftpEntry> entries;
    char buffer[512];
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = 0;
    while ((rc = libssh2_sftp_readdir(dir, buffer, sizeof(buffer), &attrs)) > 0) {
        SftpEntry e;
        e.name = QString::fromUtf8(buffer, rc);
        if (e.name == ".") continue;
        if (attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) e.size = attrs.filesize;
        if (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) {
            e.permissions = attrs.permissions & 0777;
            e.isDir = LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
        }
        if (attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) e.mtime = attrs.mtime;
        entries.push_back(e);
    }
    libssh2_sftp_closedir(dir);
    out = sortListing(entries);
    return true;
}

qint64 SftpConnection::download(const QString& remotePath, const QString& localPath) {
    if (!m_sftp) return -1;
    LIBSSH2_SFTP_HANDLE* h = libssh2_sftp_open(m_sftp, remotePath.toUtf8().constData(),
                                               LIBSSH2_FXF_READ, 0);
    if (!h) return -1;
    QFile local(localPath);
    if (!local.open(QIODevice::WriteOnly)) { libssh2_sftp_close(h); return -1; }
    qint64 total = 0;
    char buf[16384];
    ssize_t n = 0;
    while ((n = libssh2_sftp_read(h, buf, sizeof(buf))) > 0) {
        local.write(buf, n);
        total += n;
    }
    libssh2_sftp_close(h);
    return total;
}

qint64 SftpConnection::upload(const QString& localPath, const QString& remotePath) {
    if (!m_sftp) return -1;
    QFile local(localPath);
    if (!local.open(QIODevice::ReadOnly)) return -1;
    LIBSSH2_SFTP_HANDLE* h = libssh2_sftp_open(
        m_sftp, remotePath.toUtf8().constData(),
        LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
        LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR |
        LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH);
    if (!h) return -1;
    qint64 total = 0;
    QByteArray data = local.readAll();
    ssize_t n = 0;
    int off = 0;
    while (off < data.size() && (n = libssh2_sftp_write(h, data.constData() + off, data.size() - off)) > 0) {
        off += n;
        total += n;
    }
    libssh2_sftp_close(h);
    return total;
}

} // namespace macxterm::sftp
