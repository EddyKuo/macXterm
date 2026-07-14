#include "tools/CronExpr.h"

namespace macxterm::tools {
namespace {
// Parse one cron field into a presence bitmask over [lo, hi]. Returns false on
// a malformed field. Supports *, a, a-b, a-b/step, */step and comma lists.
bool parseField(const QString& field, int lo, int hi, quint64& mask) {
    mask = 0;
    const QStringList parts = field.split(',', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return false;
    for (const QString& partIn : parts) {
        QString part = partIn;
        int step = 1;
        const int slash = part.indexOf('/');
        if (slash >= 0) {
            bool ok = false;
            step = part.mid(slash + 1).toInt(&ok);
            if (!ok || step <= 0) return false;
            part = part.left(slash);
        }
        int a = lo, b = hi;
        if (part == "*") {
            // full range with step
        } else if (part.contains('-')) {
            const int dash = part.indexOf('-');
            bool ok1 = false, ok2 = false;
            a = part.left(dash).toInt(&ok1);
            b = part.mid(dash + 1).toInt(&ok2);
            if (!ok1 || !ok2) return false;
        } else {
            bool ok = false;
            a = b = part.toInt(&ok);
            if (!ok) return false;
        }
        if (a < lo || b > hi || a > b) return false;
        for (int v = a; v <= b; v += step) mask |= (quint64(1) << v);
    }
    return true;
}

// Presence mask covering every value in [lo, hi] (an unrestricted "*" field).
quint64 fullMask(int lo, int hi) {
    quint64 m = 0;
    for (int v = lo; v <= hi; ++v) m |= (quint64(1) << v);
    return m;
}
} // namespace

bool CronExpr::parse(const QString& expr) {
    const QStringList f = expr.simplified().split(' ', Qt::SkipEmptyParts);
    if (f.size() != 5) return false;
    quint64 m = 0, h = 0, dom = 0, mon = 0, dow = 0;
    if (!parseField(f[0], 0, 59, m)) return false;
    if (!parseField(f[1], 0, 23, h)) return false;
    if (!parseField(f[2], 1, 31, dom)) return false;
    if (!parseField(f[3], 1, 12, mon)) return false;
    // Day-of-week: accept 0-7, folding 7 → 0 (Sunday).
    quint64 d = 0;
    if (!parseField(f[4], 0, 7, d)) return false;
    if (d & (quint64(1) << 7)) d |= 1;   // 7 means Sunday too
    d &= 0x7f;

    m_min = m;
    m_hour = static_cast<quint32>(h);
    m_dom = static_cast<quint32>(dom);
    m_month = static_cast<quint16>(mon);
    m_dow = static_cast<quint8>(d);
    return true;
}

bool CronExpr::matches(const QDateTime& when) const {
    if (!m_valid) return false;
    const QDate date = when.date();
    const QTime time = when.time();
    if (!(m_min & (quint64(1) << time.minute()))) return false;
    if (!(m_hour & (quint32(1) << time.hour()))) return false;
    if (!(m_month & (quint16(1) << date.month()))) return false;
    // Qt: dayOfWeek() 1=Mon..7=Sun. Convert to cron 0=Sun..6=Sat.
    const int cronDow = date.dayOfWeek() % 7;
    const bool domMatch = (m_dom & (quint32(1) << date.day())) != 0;
    const bool dowMatch = (m_dow & (quint8(1) << cronDow)) != 0;
    // Standard cron: if both dom and dow are restricted, either matching counts;
    // if one is "*" it must match the other.
    const bool domRestricted = m_dom != fullMask(1, 31);
    const bool dowRestricted = m_dow != 0x7f;
    if (domRestricted && dowRestricted) return domMatch || dowMatch;
    return domMatch && dowMatch;
}

} // namespace macxterm::tools
