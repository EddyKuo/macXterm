#include "core/CliOptions.h"
#include <QHash>

namespace macxterm::core {

CliOptions CliOptions::parse(const QStringList& args) {
    CliOptions o;
    // Flags that consume a following value → their destination field.
    const QHash<QString, QString CliOptions::*> valueFlags = {
        {"-exec",       &CliOptions::exec},
        {"-bookmark",   &CliOptions::bookmark},
        {"-runmacro",   &CliOptions::runMacro},
        {"-i",          &CliOptions::configPath},
        {"-config",     &CliOptions::configPath},   // alias of -i
        {"-openfolder", &CliOptions::openFolder},
        {"-log",        &CliOptions::logPath},
        {"-shortcuts",  &CliOptions::shortcutsPath},
    };

    for (int i = 0; i < args.size(); ++i) {
        QString a = args[i];
        QString inlineVal;
        const int eq = a.indexOf('=');
        if (a.startsWith('-') && eq > 0) { inlineVal = a.mid(eq + 1); a = a.left(eq); }

        if (a == "-newtab")      { o.newTab = true; continue; }
        if (a == "-noX")         { o.noX = true; continue; }
        if (a == "-hideterm")    { o.hideTerm = true; continue; }
        if (a == "-exitwhendone"){ o.exitWhenDone = true; continue; }
        if (a == "-dpi") {
            const QString v = !inlineVal.isEmpty() ? inlineVal
                              : (i + 1 < args.size() ? args[++i] : QString());
            o.dpi = v.toInt();
            continue;
        }

        auto it = valueFlags.find(a);
        if (it != valueFlags.end()) {
            QString CliOptions::* dst = it.value();
            if (!inlineVal.isEmpty()) o.*dst = inlineVal;
            else if (i + 1 < args.size()) o.*dst = args[++i];
        }
        // Unknown flags ignored (forward-compatible).
    }
    return o;
}

} // namespace macxterm::core
