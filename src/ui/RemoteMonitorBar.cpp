#include "ui/RemoteMonitorBar.h"
#include "tools/SshExec.h"
#include "tools/RemoteMonitor.h"
#include <QStringList>
#include <chrono>

namespace macxterm::ui {

RemoteMonitorBar::RemoteMonitorBar(QWidget* parent) : QLabel(parent) {
    connect(this, &RemoteMonitorBar::sample, this, [this](double cpu, double mem) {
        setText(tr("  CPU %1%%  ·  MEM %2%%  ")
                    .arg(cpu, 0, 'f', 0).arg(mem, 0, 'f', 0));
    });
}

RemoteMonitorBar::~RemoteMonitorBar() { stop(); }

void RemoteMonitorBar::stop() {
    m_stop = true;
    if (m_worker && m_worker->joinable()) m_worker->join();
    m_worker.reset();
    m_stop = false;
}

void RemoteMonitorBar::start(const core::Session& session) {
    stop();
    setText(tr("  monitor: connecting…  "));
    core::Session s = session;
    m_worker = std::make_shared<std::thread>([this, s] {
        tools::SshExec exec;
        if (!exec.connectSession(s)) {
            emit sample(0, 0);
            return;
        }
        tools::monitor::CpuSample prev;
        while (!m_stop.load()) {
            const QByteArray out = exec.run(QStringLiteral(
                "head -1 /proc/stat; echo '==='; cat /proc/meminfo"));
            const QString text = QString::fromUtf8(out);
            const int sep = text.indexOf(QStringLiteral("==="));
            if (sep > 0) {
                const auto cur = tools::monitor::parseCpuStat(text.left(sep).trimmed());
                const auto mem = tools::monitor::parseMemInfo(text.mid(sep + 3));
                double cpu = 0;
                if (prev.valid && cur.valid) cpu = tools::monitor::cpuBusyPercent(prev, cur);
                if (cur.valid) prev = cur;
                emit sample(cpu, mem.valid ? mem.usedPercent : 0);
            }
            for (int i = 0; i < 30 && !m_stop.load(); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));  // ~3s
        }
        exec.disconnectSession();
    });
}

} // namespace macxterm::ui
