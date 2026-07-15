#pragma once
#include "core/Session.h"
#include <QDialog>

class QLineEdit;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QFormLayout;
class QGroupBox;
class QTabWidget;

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

    // Seed the folder picker with the folders that already exist in the tree so
    // the user can file this bookmark under one (or type a new name).
    void setKnownFolders(const QStringList& folders);

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
    QComboBox* m_folder = nullptr;    // param "folder" (bookmark folder; editable)
    QComboBox* m_icon = nullptr;      // param "icon" (emoji shown in the tree)

    // Tabbed layout: General / Advanced / Terminal, so the dialog stays compact.
    QTabWidget* m_tabs = nullptr;
    int m_advTabIndex = -1;
    int m_termTabIndex = -1;

    // Advanced, per-protocol options (map to Session params the backends read).
    QWidget*    m_advanced = nullptr;   // the Advanced tab page
    QFormLayout* m_advForm = nullptr;
    // SSH
    QCheckBox* m_compression = nullptr;   // param "compression"
    QCheckBox* m_x11 = nullptr;           // param "x11" (default on)
    QCheckBox* m_agent = nullptr;         // param "agent"
    QCheckBox* m_agentForward = nullptr;  // param "agentforward"
    QSpinBox*  m_sshKeepalive = nullptr;  // param "keepalive" (seconds; 0 = off)
    QLineEdit* m_sshRemoteCmd = nullptr;  // param "remotecommand"
    QCheckBox* m_sshStayOpen = nullptr;   // param "stayopen"
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
    // VNC
    QCheckBox* m_vncViewOnly = nullptr;   // param "viewonly"

    // Terminal, per-session overrides of the global appearance/behaviour
    // (params "term.font"/"term.fontSize"/"term.scheme"/"term.scrollback"/
    // "term.backspace"; blank/sentinel = inherit the global Settings).
    QWidget*    m_terminal = nullptr;   // the Terminal tab page
    QLineEdit*  m_termFont = nullptr;
    QSpinBox*   m_termFontSize = nullptr;   // 0 = inherit
    QComboBox*  m_termScheme = nullptr;     // "" = inherit
    QSpinBox*   m_termScrollback = nullptr; // -1 = inherit, 0 = disabled
    QComboBox*  m_termBackspace = nullptr;  // DEL (default) | Control-H
};

} // namespace macxterm::ui
