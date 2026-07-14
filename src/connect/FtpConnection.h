#pragma once
#include "connect/IConnection.h"

class QTcpSocket;

namespace macxterm::connect {

// FTP (RFC 959) control-channel client as a terminal session: the control
// dialog is streamed to/from the terminal so the user can type FTP commands and
// see replies (USER is auto-sent from the session's username on connect). Data
// transfers (LIST/RETR) are handled by the SFTP-style browser in a later phase.
class FtpConnection : public IConnection {
    Q_OBJECT
public:
    explicit FtpConnection(QObject* parent = nullptr);

    bool connectSession(const core::Session& session) override;
    void disconnectSession() override;
    qint64 send(const QByteArray& data) override;
    Capabilities capabilities() const override { return {false, false, false, false}; }

private slots:
    void onReadyRead();

private:
    QTcpSocket* m_sock = nullptr;
    QString m_username;
    bool m_sentUser = false;
};

} // namespace macxterm::connect
