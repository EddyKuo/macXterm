#include "tools/TftpServer.h"
#include "tools/TftpPacket.h"
#include "sftp/RemotePath.h"
#include <QUdpSocket>
#include <QHostAddress>
#include <QFile>
#include <QFileInfo>

namespace macxterm::tools {

static constexpr int kBlockSize = 512;

TftpServer::TftpServer(QObject* parent) : QObject(parent) {}
TftpServer::~TftpServer() { stop(); }

bool TftpServer::start(const QString& rootDir, quint16 port, const QString& bindAddr) {
    stop();
    m_root = QFileInfo(rootDir).absoluteFilePath();
    m_sock = new QUdpSocket(this);
    if (!m_sock->bind(QHostAddress(bindAddr), port)) { stop(); return false; }
    connect(m_sock, &QUdpSocket::readyRead, this, &TftpServer::onDatagram);
    return true;
}

void TftpServer::stop() {
    if (m_sock) { m_sock->deleteLater(); m_sock = nullptr; }
    m_transfers.clear();
}

quint16 TftpServer::port() const { return m_sock ? m_sock->localPort() : 0; }
bool TftpServer::isRunning() const { return m_sock && m_sock->state() == QAbstractSocket::BoundState; }

void TftpServer::onDatagram() {
    while (m_sock && m_sock->hasPendingDatagrams()) {
        QByteArray buf(int(m_sock->pendingDatagramSize()), 0);
        QHostAddress peer; quint16 peerPort = 0;
        m_sock->readDatagram(buf.data(), buf.size(), &peer, &peerPort);
        const QString key = peer.toString() + ":" + QString::number(peerPort);

        const tftp::Packet p = tftp::decode(buf);
        auto sendTo = [&](const QByteArray& dg) { m_sock->writeDatagram(dg, peer, peerPort); };

        if (p.op == tftp::Op::RRQ) {
            const QString rel = sftp::RemotePath::normalize("/" + p.filename);
            const QString full = QFileInfo(m_root + rel).absoluteFilePath();
            if (!full.startsWith(m_root) || !QFileInfo(full).isFile()) {
                sendTo(tftp::encodeError(1, "File not found"));
                continue;
            }
            QFile f(full);
            if (!f.open(QIODevice::ReadOnly)) { sendTo(tftp::encodeError(2, "Access violation")); continue; }
            Transfer t; t.data = f.readAll(); t.nextBlock = 1;
            m_transfers.insert(key, t);
            sendTo(tftp::encodeData(1, t.data.left(kBlockSize)));
        } else if (p.op == tftp::Op::ACK) {
            auto it = m_transfers.find(key);
            if (it == m_transfers.end()) continue;
            if (p.block == it->nextBlock) {
                // Length of the block just acknowledged. Per RFC 1350 the
                // transfer ends only after a block *shorter* than kBlockSize;
                // for a file that is an exact multiple of the block size a
                // final 0-byte DATA block must still be sent, or the client
                // waits for it and times out.
                const int ackedStart = (it->nextBlock - 1) * kBlockSize;
                const int ackedLen = static_cast<int>(
                    qMin<qint64>(kBlockSize, it->data.size() - ackedStart));
                if (ackedLen < kBlockSize) { m_transfers.erase(it); continue; }  // last (short) block acked → done
                ++it->nextBlock;
                const int off = (it->nextBlock - 1) * kBlockSize;
                sendTo(tftp::encodeData(it->nextBlock, it->data.mid(off, kBlockSize)));
            }
        } else if (p.op == tftp::Op::WRQ) {
            sendTo(tftp::encodeError(2, "Server is read-only"));
        }
    }
}

} // namespace macxterm::tools
