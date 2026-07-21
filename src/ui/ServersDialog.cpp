#include "ui/ServersDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QDir>

namespace macxterm::ui {

ServersDialog::ServersDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Light Servers"));
    auto* layout = new QVBoxLayout(this);

    layout->addWidget(buildRow(QStringLiteral("HTTP"), true,
        [this](const QString& dir, quint16 port) { return m_http.start(dir, port); },
        [this] { m_http.stop(); }, [this] { return m_http.isRunning(); },
        [this] { return m_http.port(); }));

    layout->addWidget(buildRow(QStringLiteral("FTP"), false,
        [this](const QString&, quint16 port) { return m_ftp.start(port); },
        [this] { m_ftp.stop(); }, [this] { return m_ftp.isRunning(); },
        [this] { return m_ftp.port(); }));

    layout->addWidget(buildRow(QStringLiteral("TFTP"), true,
        [this](const QString& dir, quint16 port) { return m_tftp.start(dir, port); },
        [this] { m_tftp.stop(); }, [this] { return m_tftp.isRunning(); },
        [this] { return m_tftp.port(); }));

    layout->addWidget(buildRow(QStringLiteral("NFS"), true,
        [this](const QString& dir, quint16 port) { return m_nfs.start(dir, port ? port : 2049); },
        [this] { m_nfs.stop(); }, [this] { return m_nfs.isRunning(); },
        [this] { return m_nfs.port(); }));

    layout->addWidget(buildRow(QStringLiteral("TELNET (shell)"), false,
        [this](const QString&, quint16 port) { return m_telnet.start(port); },
        [this] { m_telnet.stop(); }, [this] { return m_telnet.isRunning(); },
        [this] { return m_telnet.port(); }));

    layout->addWidget(buildSshRow());
    layout->addWidget(buildCronRow());
}

QWidget* ServersDialog::buildSshRow() {
    auto* box = new QGroupBox(tr("SSH / SFTP server"), this);
    auto* row = new QHBoxLayout(box);
    auto* user = new QLineEdit(QStringLiteral("user"), box);
    auto* pass = new QLineEdit(box);
    pass->setEchoMode(QLineEdit::Password);
    pass->setText(QStringLiteral("pass"));
    auto* dir = new QLineEdit(QDir::homePath(), box);
    auto* portSpin = new QSpinBox(box);
    portSpin->setRange(0, 65535);
    portSpin->setValue(2222);
    auto* status = new QLabel(tr("stopped"), box);
    auto* btn = new QPushButton(tr("Start"), box);
    connect(btn, &QPushButton::clicked, box, [=] {
        if (m_ssh.isRunning()) {
            m_ssh.stop();
            btn->setText(tr("Start")); status->setText(tr("stopped"));
        } else if (m_ssh.start(static_cast<quint16>(portSpin->value()), user->text(),
                               pass->text(), dir->text())) {
            btn->setText(tr("Stop"));
            status->setText(tr("running on 127.0.0.1:%1").arg(m_ssh.port()));
        } else {
            status->setText(tr("failed (no libssh?)"));
        }
    });
    row->addWidget(new QLabel(tr("User:"), box)); row->addWidget(user);
    row->addWidget(new QLabel(tr("Pass:"), box)); row->addWidget(pass);
    row->addWidget(new QLabel(tr("Dir:"), box)); row->addWidget(dir, 1);
    row->addWidget(new QLabel(tr("Port:"), box)); row->addWidget(portSpin);
    row->addWidget(btn); row->addWidget(status);
    return box;
}

QWidget* ServersDialog::buildCronRow() {
    auto* box = new QGroupBox(tr("CRON scheduler"), this);
    auto* row = new QHBoxLayout(box);
    auto* expr = new QLineEdit(QStringLiteral("*/5 * * * *"), box);
    expr->setToolTip(tr("min hour dom month dow"));
    auto* cmd = new QLineEdit(box);
    cmd->setPlaceholderText(tr("command to run"));
    auto* status = new QLabel(tr("stopped"), box);
    auto* add = new QPushButton(tr("Add job"), box);
    auto* toggle = new QPushButton(tr("Start"), box);
    connect(add, &QPushButton::clicked, box, [this, expr, cmd, status] {
        if (cmd->text().isEmpty()) return;
        if (m_cron.addJob(expr->text(), cmd->text())) {
            status->setText(tr("%1 job(s)").arg(m_cron.jobCount()));
            cmd->clear();
        } else {
            status->setText(tr("invalid cron expr"));
        }
    });
    connect(toggle, &QPushButton::clicked, box, [this, toggle, status] {
        if (m_cron.isRunning()) { m_cron.stop(); toggle->setText(tr("Start"));
            status->setText(tr("stopped")); }
        else { m_cron.start(); toggle->setText(tr("Stop"));
            status->setText(tr("running (%1 jobs)").arg(m_cron.jobCount())); }
    });
    row->addWidget(new QLabel(tr("Sched:"), box));
    row->addWidget(expr);
    row->addWidget(cmd, 1);
    row->addWidget(add);
    row->addWidget(toggle);
    row->addWidget(status);
    return box;
}

QWidget* ServersDialog::buildRow(const QString& name, bool needsDir,
                                 std::function<bool(const QString&, quint16)> start,
                                 std::function<void()> stop,
                                 std::function<bool()> running,
                                 std::function<quint16()> port) {
    auto* box = new QGroupBox(name, this);
    auto* row = new QHBoxLayout(box);

    QLineEdit* dir = nullptr;
    if (needsDir) {
        dir = new QLineEdit(QDir::homePath(), box);
        auto* browse = new QPushButton(QStringLiteral("…"), box);
        connect(browse, &QPushButton::clicked, box, [this, dir] {
            const QString d = QFileDialog::getExistingDirectory(this, tr("Serve directory"));
            if (!d.isEmpty()) dir->setText(d);
        });
        row->addWidget(new QLabel(tr("Dir:"), box));
        row->addWidget(dir);
        row->addWidget(browse);
    }
    auto* portSpin = new QSpinBox(box);
    portSpin->setRange(0, 65535);
    row->addWidget(new QLabel(tr("Port:"), box));
    row->addWidget(portSpin);

    auto* status = new QLabel(tr("stopped"), box);
    auto* btn = new QPushButton(tr("Start"), box);
    connect(btn, &QPushButton::clicked, box, [=] {
        if (running()) {
            stop();
            btn->setText(tr("Start"));
            status->setText(tr("stopped"));
        } else {
            const QString d = dir ? dir->text() : QString();
            if (start(d, static_cast<quint16>(portSpin->value()))) {
                btn->setText(tr("Stop"));
                status->setText(tr("running on port %1").arg(port()));
            } else {
                status->setText(tr("failed to start"));
            }
        }
    });
    row->addWidget(btn);
    row->addWidget(status);
    return box;
}

} // namespace macxterm::ui
