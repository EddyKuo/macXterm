#pragma once
#include "core/Session.h"
#include <QVariantMap>
#include <QString>

namespace macxterm::core {

// Pure mapping between a session-editor form (field name → value) and a Session,
// plus validation. Keeping this out of the QDialog makes the SessionDialog thin
// and lets the mapping/validation be unit-tested without a GUI (UI_Spec).
class SessionForm {
public:
    // Build a Session from form fields: "name","type","host","port","username",
    // plus any protocol params passed through verbatim.
    static Session toSession(const QVariantMap& fields);

    // Flatten a Session back into form fields for editing.
    static QVariantMap fromSession(const Session& s);

    // Validate: returns an empty string if OK, else a human-readable error.
    static QString validate(const QVariantMap& fields);

    // Per-protocol "Advanced" option values (mirrors the SessionDialog controls).
    struct AdvancedOptions {
        // SSH
        bool compression = false;
        bool x11 = true;            // default on
        bool agent = false;
        bool agentForward = false;
        QString gatewayUser, gatewayPassword, gatewayPassphrase;
        // RDP
        QString domain;
        int rdpWidth = 0, rdpHeight = 0;   // 0 = default resolution
        bool rdpClipboard = true;   // default on
        bool rdpDrives = false;
        bool rdpAudio = false;
        bool rdpNla = true;         // default on
        bool rdpIgnoreCert = false;
        // VNC
        bool vncViewOnly = false;
    };

    // Merge Advanced params into `fields` for the given session type. hasGateway
    // = a jump host is configured (enables the gateway sub-credentials). Keeps
    // saved sessions minimal: default-on flags serialize only when disabled,
    // default-off only when enabled.
    static void applyAdvanced(QVariantMap& fields, SessionType type, bool hasGateway,
                              const AdvancedOptions& opt);
};

} // namespace macxterm::core
