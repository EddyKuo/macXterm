#pragma once
#include "sftp/SftpEntry.h"
#include "core/Session.h"
#include <QString>
#include <QList>

namespace macxterm::sftp {

// Backend-neutral remote filesystem the SFTP/FTP browser drives. Both
// SftpConnection (libssh2 SFTP) and FtpClient (RFC 959) implement it so one
// panel can browse either protocol. All calls are synchronous and return
// success/bytes; implementations may also emit an `error(QString)` Qt signal
// (accessed via the concrete QObject) for status text.
class IRemoteFs {
public:
    virtual ~IRemoteFs() = default;

    virtual bool connectSession(const core::Session& session) = 0;
    virtual void disconnectSession() {}
    virtual bool isReady() const = 0;
    virtual QString realpath(const QString& path) = 0;
    virtual bool list(const QString& path, QList<SftpEntry>& out) = 0;
    virtual qint64 download(const QString& remotePath, const QString& localPath) = 0;
    virtual qint64 upload(const QString& localPath, const QString& remotePath) = 0;
    virtual bool makeDir(const QString& path, unsigned int mode = 0755) = 0;
    virtual bool removeFile(const QString& path) = 0;
    virtual bool removeDir(const QString& path) = 0;
    virtual bool rename(const QString& from, const QString& to) = 0;
    virtual bool chmod(const QString& path, unsigned int mode) = 0;
    // Optional alternate transport (SFTP offers SCP); default: unsupported.
    virtual qint64 scpDownload(const QString& /*remote*/, const QString& /*local*/) { return -1; }
};

} // namespace macxterm::sftp
