#pragma once
#include "tunnel/Tunnel.h"
#include <QDialog>

class QComboBox;
class QLineEdit;
class QSpinBox;

namespace macxterm::ui {

// SSH tunnel editor (UI_Spec flow D). Thin over tunnel::TunnelForm.
class TunnelDialog : public QDialog {
    Q_OBJECT
public:
    explicit TunnelDialog(QWidget* parent = nullptr);
    tunnel::Tunnel tunnel() const;

private slots:
    void onAccept();

private:
    QComboBox* m_kind = nullptr;
    QLineEdit* m_bindAddr = nullptr;
    QSpinBox*  m_bindPort = nullptr;
    QLineEdit* m_targetHost = nullptr;
    QSpinBox*  m_targetPort = nullptr;
};

} // namespace macxterm::ui
