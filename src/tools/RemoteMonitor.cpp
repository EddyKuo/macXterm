#include "tools/RemoteMonitor.h"
#include <QStringList>
#include <QRegularExpression>

namespace macxterm::tools::monitor {

MemInfo parseMemInfo(const QString& text) {
    MemInfo m;
    const QStringList lines = text.split('\n');
    for (const QString& line : lines) {
        const auto parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 2) continue;
        if (parts[0] == "MemTotal:")     m.totalKb = parts[1].toLong();
        else if (parts[0] == "MemAvailable:") m.availKb = parts[1].toLong();
    }
    if (m.totalKb > 0) {
        m.usedPercent = 100.0 * (m.totalKb - m.availKb) / m.totalKb;
        m.valid = true;
    }
    return m;
}

CpuSample parseCpuStat(const QString& line) {
    CpuSample s;
    const auto parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    // "cpu" user nice system idle iowait irq softirq ...
    if (parts.isEmpty() || parts[0] != "cpu" || parts.size() < 5) return s;
    long total = 0, idle = 0;
    for (int i = 1; i < parts.size(); ++i) {
        const long v = parts[i].toLong();
        total += v;
        if (i == 4) idle = v;   // 4th numeric field is idle
    }
    s.idle = idle; s.total = total; s.valid = true;
    return s;
}

double cpuBusyPercent(const CpuSample& a, const CpuSample& b) {
    const long dTotal = b.total - a.total;
    const long dIdle = b.idle - a.idle;
    if (dTotal <= 0) return 0.0;
    return 100.0 * (dTotal - dIdle) / dTotal;
}

} // namespace macxterm::tools::monitor
