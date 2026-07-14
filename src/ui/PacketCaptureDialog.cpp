#include "ui/PacketCaptureDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>

namespace macxterm::ui {

PacketCaptureDialog::PacketCaptureDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("Packet Capture"));
    resize(680, 460);
    auto* layout = new QVBoxLayout(this);

    auto* bar = new QHBoxLayout;
    m_iface = new QComboBox(this);
    m_iface->addItems(tools::PacketCapture::listInterfaces());
    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(QStringLiteral("BPF filter, e.g. \"tcp port 22\" (optional)"));
    m_btn = new QPushButton(QStringLiteral("Start"), this);
    connect(m_btn, &QPushButton::clicked, this, &PacketCaptureDialog::toggle);
    bar->addWidget(new QLabel(QStringLiteral("Interface:"), this));
    bar->addWidget(m_iface);
    bar->addWidget(m_filter, 1);
    bar->addWidget(m_btn);
    layout->addLayout(bar);

    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({QStringLiteral("#"), QStringLiteral("Proto"),
                                        QStringLiteral("Info")});
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table, 1);

    m_status = new QLabel(this);
    if (!tools::PacketCapture::available())
        m_status->setText(QStringLiteral("Built without libpcap — capture unavailable"));
    layout->addWidget(m_status);

    connect(&m_capture, &tools::PacketCapture::packet, this, &PacketCaptureDialog::onPacket);
    connect(&m_capture, &tools::PacketCapture::failed, this,
            [this](const QString& m) { m_status->setText(m); m_btn->setText(QStringLiteral("Start")); });
}

void PacketCaptureDialog::toggle() {
    if (m_capture.isRunning()) {
        m_capture.stop();
        m_btn->setText(QStringLiteral("Start"));
        m_status->setText(QStringLiteral("Stopped (%1 packets)").arg(m_count));
        return;
    }
    m_count = 0;
    m_table->setRowCount(0);
    if (m_capture.start(m_iface->currentText(), m_filter->text())) {
        m_btn->setText(QStringLiteral("Stop"));
        m_status->setText(QStringLiteral("Capturing on %1…").arg(m_iface->currentText()));
    }
}

void PacketCaptureDialog::onPacket(const macxterm::tools::PacketSummary& s) {
    const int row = m_table->rowCount();
    if (row > 5000) return;   // bound the table
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem(QString::number(++m_count)));
    m_table->setItem(row, 1, new QTableWidgetItem(s.protocol));
    m_table->setItem(row, 2, new QTableWidgetItem(
        s.info.isEmpty() ? QStringLiteral("%1 bytes").arg(s.length) : s.info));
    m_table->scrollToBottom();
}

} // namespace macxterm::ui
