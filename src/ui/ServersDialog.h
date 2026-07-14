#pragma once
#include "tools/HttpServer.h"
#include "tools/FtpServer.h"
#include "tools/TftpServer.h"
#include "tools/TelnetServer.h"
#include "tools/CronServer.h"
#include "tools/SshServer.h"
#include "tools/NfsServer.h"
#include <QDialog>
#include <QString>
#include <functional>

class QLineEdit;
class QSpinBox;
class QPushButton;
class QLabel;

namespace macxterm::ui {

// Launcher for the built-in light network servers (MobaXterm's TFTP/HTTP/FTP
// daemons). Start/stop each on a chosen directory/port. No 360-second cap.
class ServersDialog : public QDialog {
    Q_OBJECT
public:
    explicit ServersDialog(QWidget* parent = nullptr);

private:
    QWidget* buildRow(const QString& name, bool needsDir,
                      std::function<bool(const QString& dir, quint16 port)> start,
                      std::function<void()> stop,
                      std::function<bool()> running,
                      std::function<quint16()> port);

    QWidget* buildCronRow();
    QWidget* buildSshRow();

    tools::HttpServer m_http;
    tools::FtpServer m_ftp;
    tools::TftpServer m_tftp;
    tools::TelnetServer m_telnet;
    tools::CronServer m_cron;
    tools::SshServer m_ssh;
    tools::NfsServer m_nfs;
};

} // namespace macxterm::ui
