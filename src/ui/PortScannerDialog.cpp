#include "ui/PortScannerDialog.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>

namespace macxterm::ui {

PortScannerDialog::PortScannerDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("Port Scanner"));
    resize(360, 400);
    auto* layout = new QVBoxLayout(this);

    auto* form = new QFormLayout;
    m_host = new QLineEdit(QStringLiteral("127.0.0.1"), this);
    m_from = new QSpinBox(this); m_from->setRange(1, 65535); m_from->setValue(1);
    m_to = new QSpinBox(this);   m_to->setRange(1, 65535);   m_to->setValue(1024);
    form->addRow(QStringLiteral("Host"), m_host);
    auto* range = new QHBoxLayout;
    range->addWidget(m_from); range->addWidget(new QLabel(QStringLiteral("–"), this)); range->addWidget(m_to);
    form->addRow(QStringLiteral("Ports"), range);
    layout->addLayout(form);

    m_scanBtn = new QPushButton(QStringLiteral("Scan"), this);
    connect(m_scanBtn, &QPushButton::clicked, this, &PortScannerDialog::scan);
    layout->addWidget(m_scanBtn);

    m_results = new QListWidget(this);
    layout->addWidget(m_results, 1);

    connect(&m_scanner, &tools::PortScanner::portOpen, this, [this](quint16 p) {
        m_results->addItem(QStringLiteral("%1  OPEN").arg(p));
    });
    connect(&m_scanner, &tools::PortScanner::finished, this, [this](int n) {
        m_scanBtn->setEnabled(true);
        m_results->addItem(QStringLiteral("— done, %1 open port(s) —").arg(n));
    });
}

void PortScannerDialog::scan() {
    m_results->clear();
    m_scanBtn->setEnabled(false);
    m_scanner.scanRange(m_host->text(), static_cast<quint16>(m_from->value()),
                        static_cast<quint16>(m_to->value()), 200);
}

} // namespace macxterm::ui
