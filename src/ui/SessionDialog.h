#pragma once
#include "core/Session.h"
#include <QDialog>

class QLineEdit;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QFormLayout;
class QGroupBox;

namespace macxterm::ui {

// Session editor dialog (UI_Spec Session Manager). Thin wrapper over
// core::SessionForm — the field↔Session mapping and validation are tested there;
// this class only wires widgets. Accept is blocked while validation fails.
class SessionDialog : public QDialog {
    Q_OBJECT
public:
    explicit SessionDialog(QWidget* parent = nullptr);

    void setSession(const core::Session& s);
    core::Session session() const;

private slots:
    void onAccept();

private slots:
    void browseKeyFile();

private:
    QLineEdit* m_name = nullptr;
    QComboBox* m_type = nullptr;
    QLineEdit* m_host = nullptr;
    QSpinBox*  m_port = nullptr;
    QLineEdit* m_user = nullptr;
    QLineEdit* m_password = nullptr;
    QLineEdit* m_keyfile = nullptr;
    QLineEdit* m_passphrase = nullptr;
    QLineEdit* m_gateway = nullptr;   // SSH jump host: [user@]host[:port]

    // Advanced, per-protocol options (map to Session params the backends read).
    QGroupBox*  m_advanced = nullptr;
    QFormLayout* m_advForm = nullptr;
    // SSH
    QCheckBox* m_compression = nullptr;   // param "compression"
    QCheckBox* m_x11 = nullptr;           // param "x11" (default on)
    QCheckBox* m_agent = nullptr;         // param "agent"
    QCheckBox* m_agentForward = nullptr;  // param "agentforward"
    QLineEdit* m_gwUser = nullptr;        // param "gateway_user"
    QLineEdit* m_gwPassword = nullptr;    // param "gateway_password"
    QLineEdit* m_gwPassphrase = nullptr;  // param "gateway_passphrase"
    // RDP
    QLineEdit* m_domain = nullptr;        // param "domain"
    QComboBox* m_rdpResolution = nullptr; // params "width"/"height"
    QCheckBox* m_rdpClipboard = nullptr;  // param "redirect_clipboard" (default on)
    QCheckBox* m_rdpDrives = nullptr;     // param "redirect_drives"
    QCheckBox* m_rdpAudio = nullptr;      // param "redirect_audio"
    QCheckBox* m_rdpNla = nullptr;        // param "nla" (default on)
    QCheckBox* m_rdpIgnoreCert = nullptr; // param "ignorecert"
};

} // namespace macxterm::ui
