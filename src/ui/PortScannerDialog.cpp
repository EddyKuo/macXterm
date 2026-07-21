#include "ui/PortScannerDialog.h"
#include "tools/Subnet.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QApplication>

namespace macxterm::ui {

PortScannerDialog::PortScannerDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Port Scanner"));
    resize(360, 400);
    auto* layout = new QVBoxLayout(this);

    auto* form = new QFormLayout;
    m_host = new QLineEdit(QStringLiteral("127.0.0.1"), this);
    m_from = new QSpinBox(this); m_from->setRange(1, 65535); m_from->setValue(1);
    m_to = new QSpinBox(this);   m_to->setRange(1, 65535);   m_to->setValue(1024);
    form->addRow(tr("Host"), m_host);
    auto* range = new QHBoxLayout;
    range->addWidget(m_from); range->addWidget(new QLabel(QStringLiteral("–"), this)); range->addWidget(m_to);
    form->addRow(tr("Ports"), range);
    layout->addLayout(form);

    m_scanBtn = new QPushButton(tr("Scan ports"), this);
    connect(m_scanBtn, &QPushButton::clicked, this, &PortScannerDialog::scan);
    layout->addWidget(m_scanBtn);

    // Subnet host discovery (CIDR): probe a single port across every host.
    auto* subForm = new QFormLayout;
    m_cidr = new QLineEdit(QStringLiteral("192.168.1.0/24"), this);
    m_probePort = new QSpinBox(this); m_probePort->setRange(1, 65535); m_probePort->setValue(22);
    subForm->addRow(tr("Subnet (CIDR)"), m_cidr);
    subForm->addRow(tr("Probe port"), m_probePort);
    layout->addLayout(subForm);
    m_subnetBtn = new QPushButton(tr("Scan subnet hosts"), this);
    connect(m_subnetBtn, &QPushButton::clicked, this, &PortScannerDialog::scanSubnet);
    layout->addWidget(m_subnetBtn);

    m_results = new QListWidget(this);
    layout->addWidget(m_results, 1);

    connect(&m_scanner, &tools::PortScanner::portOpen, this, [this](quint16 p) {
        m_results->addItem(tr("%1  OPEN").arg(p));
    });
    connect(&m_scanner, &tools::PortScanner::finished, this, [this](int n) {
        m_scanBtn->setEnabled(true);
        m_results->addItem(tr("— done, %1 open port(s) —").arg(n));
    });
}

void PortScannerDialog::scan() {
    m_results->clear();
    m_scanBtn->setEnabled(false);
    m_scanner.scanRange(m_host->text(), static_cast<quint16>(m_from->value()),
                        static_cast<quint16>(m_to->value()), 200);
}

void PortScannerDialog::scanSubnet() {
    m_results->clear();
    const QStringList hosts = tools::Subnet::hosts(m_cidr->text());
    if (hosts.isEmpty()) {
        m_results->addItem(tr("Invalid CIDR or range too large (min /16)"));
        return;
    }
    m_subnetBtn->setEnabled(false);
    const auto port = static_cast<quint16>(m_probePort->value());
    int live = 0;
    for (const QString& h : hosts) {
        if (tools::PortScanner::scanPort(h, port, 150)) {
            m_results->addItem(tr("%1:%2  UP").arg(h).arg(port));
            ++live;
        }
        QApplication::processEvents();   // keep UI responsive during the sweep
    }
    m_results->addItem(tr("— %1 host(s) up on port %2 —").arg(live).arg(port));
    m_subnetBtn->setEnabled(true);
}

} // namespace macxterm::ui
