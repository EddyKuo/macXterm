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
};

} // namespace macxterm::core
