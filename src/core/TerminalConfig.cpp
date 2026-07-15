#include "core/TerminalConfig.h"

namespace macxterm::core {

TermConfig resolveTermConfig(const Settings& global, const QVariantMap& p) {
    TermConfig cfg;

    // Font family: non-empty session override wins.
    const QString fam = p.value(termkeys::font).toString();
    cfg.fontFamily = fam.isEmpty() ? global.fontFamily() : fam;

    // Font size: a positive session override wins.
    const int size = p.value(termkeys::fontSize).toInt();
    cfg.fontSize = size > 0 ? size : global.fontSize();

    // Colour scheme: non-empty session override wins.
    const QString scheme = p.value(termkeys::scheme).toString();
    cfg.colorScheme = scheme.isEmpty() ? global.colorScheme() : scheme;

    // Scrollback: a non-negative session override wins (0 legitimately disables
    // scrollback, so only a missing/blank key inherits).
    const QVariant sb = p.value(termkeys::scrollback);
    bool sbOk = false;
    const int sbVal = sb.toInt(&sbOk);
    cfg.scrollbackLines = (sbOk && !sb.toString().isEmpty() && sbVal >= 0)
                              ? sbVal : global.scrollbackLines();

    // Backspace: "ctrl-h" sends ^H (0x08); anything else (incl. default) sends
    // DEL (0x7f), matching xterm's out-of-the-box behaviour.
    cfg.backspaceCode = (p.value(termkeys::backspace).toString() == QLatin1String("ctrl-h"))
                            ? QByteArray("\x08") : QByteArray("\x7f");
    return cfg;
}

} // namespace macxterm::core
