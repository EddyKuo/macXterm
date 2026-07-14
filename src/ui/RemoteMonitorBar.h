#pragma once
#include "core/Session.h"
#include <QLabel>
#include <atomic>
#include <thread>
#include <memory>

namespace macxterm::ui {

// Remote system monitor shown in the status bar (MobaXterm's CPU/RAM readout).
// A worker thread runs `cat /proc/stat; cat /proc/meminfo` over SSH exec every
// few seconds and parses CPU%/RAM% with tools::RemoteMonitor; results are pushed
// to the UI via a queued signal. Off by default until start()ed for a session.
class RemoteMonitorBar : public QLabel {
    Q_OBJECT
public:
    explicit RemoteMonitorBar(QWidget* parent = nullptr);
    ~RemoteMonitorBar() override;

    void start(const core::Session& session);   // begin monitoring
    void stop();

signals:
    void sample(double cpuPercent, double memPercent);

private:
    std::atomic<bool> m_stop{false};
    std::shared_ptr<std::thread> m_worker;
};

} // namespace macxterm::ui
