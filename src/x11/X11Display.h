#pragma once
#include <QString>

namespace macxterm::x11 {

// X11 display management for SSH X11-forwarding (ADR_5, FR-012). macXterm does
// not implement an X server itself — it integrates the platform's X.Org
// (XQuartz / VcXsrv / native) and manages the DISPLAY handed to forwarded apps.
// The parse/format helpers are pure and unit-tested; detection reads env/state.
struct DisplaySpec {
    QString host;      // empty = local (unix socket)
    int display = 0;
    int screen = 0;
    bool valid = false;
};

class X11Display {
public:
    // Parse a DISPLAY string like "localhost:10.0" or ":0" or "host:1".
    static DisplaySpec parse(const QString& display);
    // Format a DisplaySpec back to the DISPLAY string form.
    static QString format(const DisplaySpec& spec);

    // The DISPLAY to advertise to a forwarded app for a given forwarding
    // channel number (e.g. channel 0 -> "localhost:10.0").
    static QString forwardingDisplay(int channel, int base = 10);

    // Is a local X server reachable? (reads the DISPLAY env var by default)
    static bool serverAvailable(const QString& displayEnv);
};

} // namespace macxterm::x11
