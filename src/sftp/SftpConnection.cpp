#include "sftp/SftpConnection.h"
#include "platform/Net.h"
#include <libssh2.h>
#include <libssh2_sftp.h>
#include <QFile>

#if !defined(_WIN32)
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#endif

namespace macxterm::sftp {

SftpConnection::SftpConnection(QObject* parent) : QObject(parent) { libssh2_init(0); }
SftpConnection::~SftpConnection() { disconnectSession(); libssh2_exit(); }

#if !defined(_WIN32)
bool SftpConnection::connectSession(const core::Session& s) {
    disconnectSession();
    // Open a TCP socket to host:port.
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    const QByteArray host = s.host().toUtf8();
    const QByteArray port = QByteArray::number(s.port());
    if (getaddrinfo(host.constData(), port.constData(), &hints, &res) != 0) {
        emit error(QStringLiteral("SFTP: host lookup failed")); return false;
    }
    int fd = -1;
    for (auto* p = res; p; p = p->ai_next) {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        ::close(fd); fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) { emit error(QStringLiteral("SFTP: connection refused")); return false; }

    LIBSSH2_SESSION* sess = libssh2_session_init();
    if (!sess) { ::close(fd); return false; }
    libssh2_session_set_blocking(sess, 1);
    if (libssh2_session_handshake(sess, fd) != 0) {
        emit error(QStringLiteral("SFTP: SSH handshake failed"));
        libssh2_session_free(sess); ::close(fd); return false;
    }
    const QByteArray user = s.username().toUtf8();
    const QByteArray keyfile = s.param("keyfile").toUtf8();
    bool authed;
    if (!keyfile.isEmpty()) {
        const QByteArray pass = s.param("passphrase").toUtf8();
        authed = libssh2_userauth_publickey_fromfile(
                     sess, user.constData(), nullptr, keyfile.constData(), pass.constData()) == 0;
    } else {
        const QByteArray pw = s.param("password").toUtf8();
        authed = libssh2_userauth_password(sess, user.constData(), pw.constData()) == 0;
    }
    if (!authed) {
        emit error(QStringLiteral("SFTP: authentication failed"));
        libssh2_session_disconnect(sess, "bye"); libssh2_session_free(sess); ::close(fd);
        return false;
    }
    m_sftp = libssh2_sftp_init(sess);
    if (!m_sftp) {
        emit error(QStringLiteral("SFTP: failed to start subsystem"));
        libssh2_session_disconnect(sess, "bye"); libssh2_session_free(sess); ::close(fd);
        return false;
    }
    m_session = sess; m_sock = fd; m_ownsSession = true;
    return true;
}
#else
// Windows: same libssh2 flow as the Unix path, over a Winsock socket from the
// platform net shim. (Kept separate from the Unix code so macOS/Linux stay
// byte-identical.)
bool SftpConnection::connectSession(const core::Session& s) {
    disconnectSession();
    const QByteArray host = s.host().toUtf8();
    const int fd = platform::net::connectTcp(host.constData(), s.port());
    if (fd < 0) { emit error(QStringLiteral("SFTP: connection refused")); return false; }

    LIBSSH2_SESSION* sess = libssh2_session_init();
    if (!sess) { platform::net::closeSocket(fd); return false; }
    libssh2_session_set_blocking(sess, 1);
    if (libssh2_session_handshake(sess, fd) != 0) {
        emit error(QStringLiteral("SFTP: SSH handshake failed"));
        libssh2_session_free(sess); platform::net::closeSocket(fd); return false;
    }
    const QByteArray user = s.username().toUtf8();
    const QByteArray keyfile = s.param("keyfile").toUtf8();
    bool authed;
    if (!keyfile.isEmpty()) {
        const QByteArray pass = s.param("passphrase").toUtf8();
        authed = libssh2_userauth_publickey_fromfile(
                     sess, user.constData(), nullptr, keyfile.constData(), pass.constData()) == 0;
    } else {
        const QByteArray pw = s.param("password").toUtf8();
        authed = libssh2_userauth_password(sess, user.constData(), pw.constData()) == 0;
    }
    if (!authed) {
        emit error(QStringLiteral("SFTP: authentication failed"));
        libssh2_session_disconnect(sess, "bye"); libssh2_session_free(sess);
        platform::net::closeSocket(fd);
        return false;
    }
    m_sftp = libssh2_sftp_init(sess);
    if (!m_sftp) {
        emit error(QStringLiteral("SFTP: failed to start subsystem"));
        libssh2_session_disconnect(sess, "bye"); libssh2_session_free(sess);
        platform::net::closeSocket(fd);
        return false;
    }
    m_session = sess; m_sock = fd; m_ownsSession = true;
    return true;
}
#endif

void SftpConnection::disconnectSession() {
    if (m_sftp) { libssh2_sftp_shutdown(m_sftp); m_sftp = nullptr; }
    if (m_ownsSession && m_session) {
        libssh2_session_disconnect(m_session, "bye");
        libssh2_session_free(m_session);
        // net::closeSocket is ::close on Unix (identical) / closesocket on Windows.
        if (m_sock >= 0) platform::net::closeSocket(m_sock);
    }
    m_session = nullptr; m_sock = -1; m_ownsSession = false;
}

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
        if (local.write(buf, n) != n) {   // local write failed (e.g. disk full)
            libssh2_sftp_close(h); local.close(); QFile::remove(localPath);
            emit error(QStringLiteral("SFTP download: local write failed"));
            return -1;
        }
        total += n;
    }
    libssh2_sftp_close(h);
    // n < 0 is a mid-transfer read error; n == 0 is a clean EOF. Only the clean
    // EOF is a real success — a partial byte count must not be reported as one.
    if (n < 0) {
        local.close(); QFile::remove(localPath);
        emit error(QStringLiteral("SFTP download failed mid-transfer"));
        return -1;
    }
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
    if (off < data.size()) {   // write returned <= 0 before all bytes were sent
        emit error(QStringLiteral("SFTP upload failed mid-transfer"));
        return -1;
    }
    return total;
}

qint64 SftpConnection::scpDownload(const QString& remotePath, const QString& localPath) {
    if (!m_session) return -1;
    libssh2_struct_stat st{};
    LIBSSH2_CHANNEL* ch = libssh2_scp_recv2(m_session, remotePath.toUtf8().constData(), &st);
    if (!ch) { emit error(QStringLiteral("SCP download failed")); return -1; }
    QFile local(localPath);
    if (!local.open(QIODevice::WriteOnly)) { libssh2_channel_free(ch); return -1; }
    qint64 remaining = st.st_size, total = 0;
    char buf[16384];
    while (remaining > 0) {
        const ssize_t want = static_cast<ssize_t>(qMin<qint64>(sizeof(buf), remaining));
        const ssize_t n = libssh2_channel_read(ch, buf, want);
        if (n <= 0) break;
        local.write(buf, n);
        total += n; remaining -= n;
    }
    libssh2_channel_send_eof(ch);
    libssh2_channel_free(ch);
    if (remaining > 0) {   // read stopped before the full st_size was received
        local.close(); QFile::remove(localPath);
        emit error(QStringLiteral("SCP download truncated"));
        return -1;
    }
    return total;
}

qint64 SftpConnection::scpUpload(const QString& localPath, const QString& remotePath) {
    if (!m_session) return -1;
    QFile local(localPath);
    if (!local.open(QIODevice::ReadOnly)) return -1;
    const QByteArray data = local.readAll();
    LIBSSH2_CHANNEL* ch = libssh2_scp_send64(
        m_session, remotePath.toUtf8().constData(), 0644,
        static_cast<libssh2_uint64_t>(data.size()), 0, 0);
    if (!ch) { emit error(QStringLiteral("SCP upload failed")); return -1; }
    qint64 total = 0;
    int off = 0;
    while (off < data.size()) {
        const ssize_t n = libssh2_channel_write(ch, data.constData() + off, data.size() - off);
        if (n < 0) break;
        off += n; total += n;
    }
    libssh2_channel_send_eof(ch);
    libssh2_channel_wait_eof(ch);
    libssh2_channel_wait_closed(ch);
    libssh2_channel_free(ch);
    if (off < data.size()) {   // write errored before all bytes were sent
        emit error(QStringLiteral("SCP upload truncated"));
        return -1;
    }
    return total;
}

QString SftpConnection::realpath(const QString& path) {
    if (!m_sftp) return {};
    char buf[1024];
    const int rc = libssh2_sftp_realpath(m_sftp, path.toUtf8().constData(), buf, sizeof(buf));
    if (rc <= 0) return {};
    return QString::fromUtf8(buf, rc);
}

bool SftpConnection::chmod(const QString& path, unsigned int mode) {
    if (!m_sftp) return false;
    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    attrs.flags = LIBSSH2_SFTP_ATTR_PERMISSIONS;
    attrs.permissions = mode & 07777;
    const int rc = libssh2_sftp_setstat(m_sftp, path.toUtf8().constData(), &attrs);
    if (rc != 0) emit error(QStringLiteral("chmod failed: %1").arg(path));
    return rc == 0;
}

bool SftpConnection::makeDir(const QString& path, unsigned int mode) {
    if (!m_sftp) return false;
    const int rc = libssh2_sftp_mkdir(m_sftp, path.toUtf8().constData(), mode & 07777);
    if (rc != 0) emit error(QStringLiteral("mkdir failed: %1").arg(path));
    return rc == 0;
}

bool SftpConnection::removeFile(const QString& path) {
    if (!m_sftp) return false;
    const int rc = libssh2_sftp_unlink(m_sftp, path.toUtf8().constData());
    if (rc != 0) emit error(QStringLiteral("delete failed: %1").arg(path));
    return rc == 0;
}

bool SftpConnection::removeDir(const QString& path) {
    if (!m_sftp) return false;
    const int rc = libssh2_sftp_rmdir(m_sftp, path.toUtf8().constData());
    if (rc != 0) emit error(QStringLiteral("rmdir failed: %1").arg(path));
    return rc == 0;
}

bool SftpConnection::rename(const QString& from, const QString& to) {
    if (!m_sftp) return false;
    const int rc = libssh2_sftp_rename(m_sftp, from.toUtf8().constData(), to.toUtf8().constData());
    if (rc != 0) emit error(QStringLiteral("rename failed: %1").arg(from));
    return rc == 0;
}

} // namespace macxterm::sftp
