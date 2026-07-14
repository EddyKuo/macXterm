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
    setWindowTitle(QStringLiteral("Light Servers"));
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
            const QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("Serve directory"));
            if (!d.isEmpty()) dir->setText(d);
        });
        row->addWidget(new QLabel(QStringLiteral("Dir:"), box));
        row->addWidget(dir);
        row->addWidget(browse);
    }
    auto* portSpin = new QSpinBox(box);
    portSpin->setRange(0, 65535);
    row->addWidget(new QLabel(QStringLiteral("Port:"), box));
    row->addWidget(portSpin);

    auto* status = new QLabel(QStringLiteral("stopped"), box);
    auto* btn = new QPushButton(QStringLiteral("Start"), box);
    connect(btn, &QPushButton::clicked, box, [=] {
        if (running()) {
            stop();
            btn->setText(QStringLiteral("Start"));
            status->setText(QStringLiteral("stopped"));
        } else {
            const QString d = dir ? dir->text() : QString();
            if (start(d, static_cast<quint16>(portSpin->value()))) {
                btn->setText(QStringLiteral("Stop"));
                status->setText(QStringLiteral("running on port %1").arg(port()));
            } else {
                status->setText(QStringLiteral("failed to start"));
            }
        }
    });
    row->addWidget(btn);
    row->addWidget(status);
    return box;
}

} // namespace macxterm::ui
