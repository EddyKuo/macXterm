#include "tools/PacketCapture.h"
#include <QMetaType>

#if defined(MACXTERM_HAVE_PCAP)
#include <pcap.h>
#endif

namespace macxterm::tools {

// ── Pure decoder (always compiled) ──
namespace PacketDecode {
namespace {
QString ipv4(const unsigned char* p) {
    return QStringLiteral("%1.%2.%3.%4").arg(p[0]).arg(p[1]).arg(p[2]).arg(p[3]);
}
quint16 be16(const unsigned char* p) { return (quint16(p[0]) << 8) | p[1]; }
} // namespace

PacketSummary summarize(const unsigned char* data, int caplen, int linktype) {
    PacketSummary s;
    s.length = caplen;
    if (!data || caplen <= 0) { s.protocol = QStringLiteral("?"); return s; }

    int off = 0;
    quint16 ethertype = 0;
    if (linktype == 1) {                 // DLT_EN10MB (Ethernet)
        if (caplen < 14) { s.protocol = QStringLiteral("ETH?"); return s; }
        ethertype = be16(data + 12);
        off = 14;
    } else if (linktype == 0) {          // DLT_NULL (loopback): 4-byte family
        if (caplen < 4) { s.protocol = QStringLiteral("NULL?"); return s; }
        off = 4;
        ethertype = 0x0800;              // assume IPv4 for the common case
    } else {
        off = 0; ethertype = 0x0800;
    }

    if (ethertype == 0x0806) { s.protocol = QStringLiteral("ARP"); return s; }
    if (ethertype == 0x86DD) { s.protocol = QStringLiteral("IPv6"); return s; }
    if (ethertype != 0x0800) {
        s.protocol = QStringLiteral("0x%1").arg(ethertype, 4, 16, QChar('0'));
        return s;
    }

    // IPv4
    if (caplen < off + 20) { s.protocol = QStringLiteral("IPv4?"); return s; }
    const unsigned char* ip = data + off;
    const int ihl = (ip[0] & 0x0F) * 4;
    const int proto = ip[9];
    const QString src = ipv4(ip + 12), dst = ipv4(ip + 16);
    const int l4 = off + ihl;

    if (proto == 6 && caplen >= l4 + 4) {         // TCP
        s.protocol = QStringLiteral("TCP");
        s.info = QStringLiteral("%1:%2 → %3:%4")
                     .arg(src).arg(be16(data + l4)).arg(dst).arg(be16(data + l4 + 2));
    } else if (proto == 17 && caplen >= l4 + 4) { // UDP
        s.protocol = QStringLiteral("UDP");
        s.info = QStringLiteral("%1:%2 → %3:%4")
                     .arg(src).arg(be16(data + l4)).arg(dst).arg(be16(data + l4 + 2));
    } else if (proto == 1) {                       // ICMP
        s.protocol = QStringLiteral("ICMP");
        s.info = QStringLiteral("%1 → %2").arg(src, dst);
    } else {
        s.protocol = QStringLiteral("IPv4");
        s.info = QStringLiteral("%1 → %2 (proto %3)").arg(src, dst).arg(proto);
    }
    return s;
}
} // namespace PacketDecode

// ── Capture engine ──
PacketCapture::PacketCapture(QObject* parent) : QObject(parent) {
    qRegisterMetaType<macxterm::tools::PacketSummary>("macxterm::tools::PacketSummary");
}
PacketCapture::~PacketCapture() { stop(); }

#if defined(MACXTERM_HAVE_PCAP)
bool PacketCapture::available() { return true; }

QStringList PacketCapture::listInterfaces() {
    QStringList out;
    pcap_if_t* alldevs = nullptr;
    char err[PCAP_ERRBUF_SIZE] = {0};
    if (pcap_findalldevs(&alldevs, err) == 0) {
        for (pcap_if_t* d = alldevs; d; d = d->next) out << QString::fromUtf8(d->name);
        pcap_freealldevs(alldevs);
    }
    return out;
}

bool PacketCapture::start(const QString& iface, const QString& bpfFilter) {
    stop();
    if (iface.isEmpty()) return false;
    m_stop = false;
    m_running = true;
    m_thread = std::make_shared<std::thread>(
        [this, iface, bpfFilter] { captureLoop(iface, bpfFilter); });
    return true;
}

void PacketCapture::captureLoop(QString iface, QString filter) {
    char err[PCAP_ERRBUF_SIZE] = {0};
    pcap_t* handle = pcap_open_live(iface.toUtf8().constData(), 65535, 1, 200, err);
    if (!handle) {
        emit failed(QStringLiteral("Cannot open %1: %2").arg(iface, QString::fromUtf8(err)));
        m_running = false;
        return;
    }
    if (!filter.isEmpty()) {
        bpf_program prog;
        if (pcap_compile(handle, &prog, filter.toUtf8().constData(), 1, PCAP_NETMASK_UNKNOWN) == 0) {
            pcap_setfilter(handle, &prog);
            pcap_freecode(&prog);
        } else {
            emit failed(QStringLiteral("Bad filter: %1").arg(QString::fromUtf8(pcap_geterr(handle))));
        }
    }
    const int linktype = pcap_datalink(handle);
    while (!m_stop.load()) {
        pcap_pkthdr* hdr = nullptr;
        const unsigned char* data = nullptr;
        const int rc = pcap_next_ex(handle, &hdr, &data);
        if (rc == 1 && hdr) {
            emit packet(PacketDecode::summarize(data, static_cast<int>(hdr->caplen), linktype));
        } else if (rc == PCAP_ERROR || rc == PCAP_ERROR_BREAK) {
            break;
        }
        // rc == 0 → timeout, loop and recheck m_stop.
    }
    pcap_close(handle);
    m_running = false;
}
#else
bool PacketCapture::available() { return false; }
QStringList PacketCapture::listInterfaces() { return {}; }
bool PacketCapture::start(const QString&, const QString&) {
    emit failed(QStringLiteral("Built without libpcap"));
    return false;
}
void PacketCapture::captureLoop(QString, QString) {}
#endif

void PacketCapture::stop() {
    m_stop = true;
    if (m_thread && m_thread->joinable()) m_thread->join();
    m_thread.reset();
    m_running = false;
}

} // namespace macxterm::tools
