#pragma once
#include "sftp/IRemoteFs.h"
#include "sftp/SftpEntry.h"
#include "core/Session.h"
#include <QObject>

class QTcpSocket;

namespace macxterm::sftp {

// Parse a Unix `ls -l`-style FTP LIST payload into directory entries. Pure and
// unit-tested (the network layer is exercised end-to-end against the built-in
// FtpServer). Tolerant of the common "perms links owner group size date name"
// layout; unparseable lines are skipped.
QList<SftpEntry> parseFtpList(const QByteArray& listing);

// Graphical FTP browser back-end (RFC 959, passive mode). Implements the shared
// IRemoteFs so the SFTP panel can browse FTP servers too. Blocking calls; run
// off the UI thread for slow endpoints.
class FtpClient : public QObject, public IRemoteFs {
    Q_OBJECT
public:
    explicit FtpClient(QObject* parent = nullptr);
    ~FtpClient() override;

    bool connectSession(const core::Session& session);
    void disconnectSession();

    // IRemoteFs
    bool isReady() const override;
    QString realpath(const QString& path) override;
    bool list(const QString& path, QList<SftpEntry>& out) override;
    qint64 download(const QString& remotePath, const QString& localPath) override;
    qint64 upload(const QString& localPath, const QString& remotePath) override;
    bool makeDir(const QString& path, unsigned int mode = 0755) override;
    bool removeFile(const QString& path) override;
    bool removeDir(const QString& path) override;
    bool rename(const QString& from, const QString& to) override;
    bool chmod(const QString& path, unsigned int mode) override;

signals:
    void error(const QString& message);

private:
    int sendCmd(const QString& verb, const QString& arg, QString* reply = nullptr);
    QTcpSocket* openPasv();        // send PASV, connect the data socket
    int readReply(QString* text);  // read one (possibly multi-line) control reply

    QTcpSocket* m_ctrl = nullptr;
    QString m_cwd = QStringLiteral("/");
};

} // namespace macxterm::sftp
