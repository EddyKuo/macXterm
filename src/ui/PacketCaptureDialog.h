#pragma once
#include "tools/PacketCapture.h"
#include <QDialog>

class QComboBox;
class QLineEdit;
class QTableWidget;
class QPushButton;
class QLabel;

namespace macxterm::ui {

// Network packet capture UI (MobaXterm's TCPCapture). Pick an interface, set an
// optional BPF filter, and watch decoded packet summaries stream in. Backed by
// tools::PacketCapture (libpcap). Live capture usually needs elevated privilege.
class PacketCaptureDialog : public QDialog {
    Q_OBJECT
public:
    explicit PacketCaptureDialog(QWidget* parent = nullptr);

private slots:
    void toggle();
    void onPacket(const macxterm::tools::PacketSummary& s);

private:
    QComboBox* m_iface = nullptr;
    QLineEdit* m_filter = nullptr;
    QTableWidget* m_table = nullptr;
    QPushButton* m_btn = nullptr;
    QLabel* m_status = nullptr;
    tools::PacketCapture m_capture;
    int m_count = 0;
};

} // namespace macxterm::ui
