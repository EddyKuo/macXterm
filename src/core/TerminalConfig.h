#pragma once
#include "core/Settings.h"
#include <QByteArray>
#include <QString>
#include <QVariantMap>

namespace macxterm::core {

// The terminal appearance/behaviour actually applied to one pane, after a
// session's per-bookmark overrides are layered on top of the global Settings.
// MobaXterm lets every bookmark override font/colours/scrollback/backspace
// independently of the global defaults (research/MobaXterm.md §1.1 Terminal
// tab); we mirror that with a handful of "term.*" session params.
struct TermConfig {
    QString fontFamily;
    int fontSize = 0;
    QString colorScheme;
    int scrollbackLines = 0;
    QByteArray backspaceCode;   // bytes sent when the user presses Backspace
};

// The session-param keys that carry a per-session override. Absent/empty means
// "inherit the global value".
namespace termkeys {
inline constexpr char font[]       = "term.font";
inline constexpr char fontSize[]   = "term.fontSize";
inline constexpr char scheme[]     = "term.scheme";
inline constexpr char scrollback[] = "term.scrollback";
inline constexpr char backspace[]  = "term.backspace";   // "del" (^?) | "ctrl-h" (^H)
}

// Resolve the effective terminal config for a pane: each field falls back to
// the global Settings when the session provides no valid override. Pure — no
// GUI, unit-tested. `backspace` maps "ctrl-h" → 0x08, anything else → 0x7f.
TermConfig resolveTermConfig(const Settings& global, const QVariantMap& sessionParams);

} // namespace macxterm::core
