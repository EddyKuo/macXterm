#pragma once
#include <QString>

namespace macxterm::platform {

// Windows shell integration (MobaXterm parity): register a URL protocol handler
// and a session-file association so `ssh://` / `macxterm:` links and double-clicked
// `.mxtsession` files open in macXterm. All writes go to HKCU\Software\Classes
// (per-user, no administrator rights). No-ops returning false on non-Windows.
class WinIntegration {
public:
    // Register the `macxterm:` URL scheme → this executable. Returns false on error.
    static bool registerProtocolHandler();

    // Associate `.mxtsession` files with this executable. Returns false on error.
    static bool registerFileAssociation();

    // Convenience: do both, returning true only if both succeeded.
    static bool registerAll();

    static bool available();   // true on Windows
};

} // namespace macxterm::platform
