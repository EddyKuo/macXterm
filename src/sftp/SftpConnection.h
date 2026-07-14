#pragma once
#include "sftp/SftpEntry.h"
#include "core/Session.h"
#include <QObject>
#include <QString>
#include <QList>

typedef struct _LIBSSH2_SESSION LIBSSH2_SESSION;
typedef struct _LIBSSH2_SFTP LIBSSH2_SFTP;

namespace macxterm::sftp {

// Graphical SFTP browser back-end over libssh2 (Architecture §6.3, FR-008).
// Shares the SSH transport socket; lists directories into SftpEntry rows and
// reads/writes files. Live operation needs a real sshd; the entry modelling and
// path handling it relies on (SftpEntry, RemotePath) are unit-tested separately.
class SftpConnection : public QObject {
    Q_OBJECT
public:
    explicit SftpConnection(QObject* parent = nullptr);
    ~SftpConnection() override;

    // Attach to an already-authenticated libssh2 session + its socket fd.
    bool attach(LIBSSH2_SESSION* session, int socketFd);
    void detach();
    bool isReady() const { return m_sftp != nullptr; }

    // Self-contained connect: open a dedicated SSH session from the Session's
    // params (host/port/username/password or keyfile), authenticate, and start
    // the SFTP subsystem. Blocking; run off the UI thread for slow endpoints.
    bool connectSession(const core::Session& session);
    void disconnectSession();

    // List a remote directory into sorted SftpEntry rows. Returns false on error.
    bool list(const QString& path, QList<SftpEntry>& out);

    // Download/upload a file. Returns bytes transferred, or -1 on error.
    qint64 download(const QString& remotePath, const QString& localPath);
    qint64 upload(const QString& localPath, const QString& remotePath);

signals:
    void error(const QString& message);

private:
    LIBSSH2_SESSION* m_session = nullptr;
    LIBSSH2_SFTP* m_sftp = nullptr;
    int m_sock = -1;
    bool m_ownsSession = false;   // true when we opened the session ourselves
};

} // namespace macxterm::sftp
