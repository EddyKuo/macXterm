#include "x11/X11Display.h"

namespace macxterm::x11 {

DisplaySpec X11Display::parse(const QString& display) {
    DisplaySpec s;
    if (display.isEmpty()) return s;
    const int colon = display.lastIndexOf(':');
    if (colon < 0) return s;
    s.host = display.left(colon);           // may be empty (local)
    const QString rest = display.mid(colon + 1);
    if (rest.isEmpty()) return s;
    const int dot = rest.indexOf('.');
    bool ok = false;
    if (dot < 0) {
        s.display = rest.toInt(&ok);
        s.screen = 0;
    } else {
        s.display = rest.left(dot).toInt(&ok);
        bool ok2 = false;
        s.screen = rest.mid(dot + 1).toInt(&ok2);
        if (!ok2) return s;
    }
    s.valid = ok;
    return s;
}

QString X11Display::format(const DisplaySpec& spec) {
    return spec.host + ":" + QString::number(spec.display) + "." + QString::number(spec.screen);
}

QString X11Display::forwardingDisplay(int channel, int base) {
    DisplaySpec s;
    s.host = QStringLiteral("localhost");
    s.display = base + channel;
    s.screen = 0;
    return format(s);
}

bool X11Display::serverAvailable(const QString& displayEnv) {
    return parse(displayEnv).valid;
}

} // namespace macxterm::x11
