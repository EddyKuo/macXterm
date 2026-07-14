#pragma once
#include <QString>

namespace macxterm::tools {

// Parses remote system stats for the monitoring status bar (research §1.8).
// The agent runs a small command over SSH; these pure parsers turn its output
// into numbers, so they are unit-testable without a live host.
namespace monitor {

struct MemInfo { long totalKb = 0; long availKb = 0; double usedPercent = 0; bool valid = false; };

// Parse Linux /proc/meminfo content.
MemInfo parseMemInfo(const QString& procMeminfo);

// Parse the aggregate CPU line from /proc/stat ("cpu  u n s idle ...");
// returns busy fraction between two samples given both idle+total, or the
// instantaneous non-idle ratio for a single sample.
struct CpuSample { long idle = 0; long total = 0; bool valid = false; };
CpuSample parseCpuStat(const QString& procStatFirstLine);
double cpuBusyPercent(const CpuSample& a, const CpuSample& b);

} // namespace monitor
} // namespace macxterm::tools
