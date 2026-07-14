#pragma once
#include <QString>
#include <QStringList>

namespace macxterm::core {

// Parsed command-line options (research §1.9 CLI parity: -exec/-newtab/
// -bookmark/-i/-runmacro ...). Kept as plain data so it is unit-testable
// without a running QApplication.
struct CliOptions {
    QString exec;         // -exec "cmd"     : run a command in a new terminal
    QString bookmark;     // -bookmark name  : launch a saved session
    QString runMacro;     // -runmacro name
    QString configPath;   // -i path         : alternate config file
    QString openFolder;   // -openfolder path
    bool newTab = false;  // -newtab
    bool noX = false;     // -noX            : disable the X server
    bool hideTerm = false;// -hideterm

    // Parse an argv-style list (excluding program name). Unknown flags are
    // ignored (forward-compatible). Values may follow the flag or use '='.
    static CliOptions parse(const QStringList& args);
};

} // namespace macxterm::core
