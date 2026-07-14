#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <atomic>
#include <thread>
#include <memory>

namespace macxterm::tools {

// One decoded packet summary line (protocol, addresses, ports, length).
struct PacketSummary {
    int length = 0;
    QString protocol;   // "TCP" / "UDP" / "ICMP" / "IPv4" / "ARP" / …
    QString info;       // e.g. "192.168.1.2:51000 → 10.0.0.1:22"
};

// Pure packet decoding (link-layer → transport). No libpcap dependency, so it is
// always compiled and unit-tested. `linktype` uses libpcap DLT_* values
// (1 = Ethernet, 0 = loopback/null).
namespace PacketDecode {
PacketSummary summarize(const unsigned char* data, int caplen, int linktype);
}

// Live packet capture (MobaXterm's TCPCapture / network sniffer), backed by
// libpcap. Requires MACXTERM_HAVE_PCAP and usually elevated privilege for live
// capture; without libpcap, start() returns false and listInterfaces() is empty.
class PacketCapture : public QObject {
    Q_OBJECT
public:
    explicit PacketCapture(QObject* parent = nullptr);
    ~PacketCapture() override;

    static QStringList listInterfaces();
    static bool available();

    // Begin capturing on `iface` with an optional BPF filter (e.g. "tcp port 22").
    // Emits packet() per captured frame (queued to the caller's thread).
    bool start(const QString& iface, const QString& bpfFilter = QString());
    void stop();
    bool isRunning() const { return m_running.load(); }

signals:
    void packet(const macxterm::tools::PacketSummary& summary);
    void failed(const QString& message);

private:
    void captureLoop(QString iface, QString filter);

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stop{false};
    std::shared_ptr<std::thread> m_thread;
};

} // namespace macxterm::tools
