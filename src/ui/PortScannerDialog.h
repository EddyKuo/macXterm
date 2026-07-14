#pragma once
#include "tools/PortScanner.h"
#include <QDialog>

class QLineEdit;
class QSpinBox;
class QListWidget;
class QPushButton;

namespace macxterm::ui {

// A small network port scanner (MobaXterm's MobaListPorts-style tool), backed by
// tools::PortScanner. Scans a host over a port range and lists the open ports.
class PortScannerDialog : public QDialog {
    Q_OBJECT
public:
    explicit PortScannerDialog(QWidget* parent = nullptr);

private slots:
    void scan();
    void scanSubnet();   // find live hosts across a CIDR block on a chosen port

private:
    QLineEdit* m_host = nullptr;
    QSpinBox* m_from = nullptr;
    QSpinBox* m_to = nullptr;
    QLineEdit* m_cidr = nullptr;
    QSpinBox* m_probePort = nullptr;
    QListWidget* m_results = nullptr;
    QPushButton* m_scanBtn = nullptr;
    QPushButton* m_subnetBtn = nullptr;
    tools::PortScanner m_scanner;
};

} // namespace macxterm::ui
