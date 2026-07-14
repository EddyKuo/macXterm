#include "tools/NfsServer.h"
#include "tools/Xdr.h"
#include <QUdpSocket>
#include <QHostAddress>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <sys/stat.h>

namespace macxterm::tools {
namespace {

// ONC-RPC / NFS constants.
constexpr quint32 PROG_PORTMAP = 100000, PROG_MOUNT = 100005, PROG_NFS = 100003;
constexpr quint32 NFS3_OK = 0, NFS3ERR_NOENT = 2, NFS3ERR_ACCES = 13;

// NFS file types.
constexpr quint32 NF3REG = 1, NF3DIR = 2, NF3LNK = 5;

// Encode an NFSv3 fattr3 from a stat buffer.
void putFattr3(XdrWriter& w, const struct stat& st) {
    quint32 type = NF3REG;
    if (S_ISDIR(st.st_mode)) type = NF3DIR;
    else if (S_ISLNK(st.st_mode)) type = NF3LNK;
    w.u32(type);
    w.u32(st.st_mode & 07777);
    w.u32(static_cast<quint32>(st.st_nlink));
    w.u32(st.st_uid);
    w.u32(st.st_gid);
    w.u64(static_cast<quint64>(st.st_size));
    w.u64(static_cast<quint64>(st.st_blocks) * 512);
    w.u32(0); w.u32(0);                                   // rdev (specdata3)
    w.u64(static_cast<quint64>(st.st_dev));               // fsid
    w.u64(static_cast<quint64>(st.st_ino));               // fileid
#if defined(__APPLE__)
    const auto at = st.st_atimespec.tv_sec, mt = st.st_mtimespec.tv_sec, ct = st.st_ctimespec.tv_sec;
#else
    const auto at = st.st_atim.tv_sec, mt = st.st_mtim.tv_sec, ct = st.st_ctim.tv_sec;
#endif
    w.u32(static_cast<quint32>(at)); w.u32(0);
    w.u32(static_cast<quint32>(mt)); w.u32(0);
    w.u32(static_cast<quint32>(ct)); w.u32(0);
}

// post_op_attr: a "1" flag + fattr3 when the file exists, else "0".
void putPostOpAttr(XdrWriter& w, const QString& path) {
    struct stat st{};
    if (!path.isEmpty() && ::stat(path.toUtf8().constData(), &st) == 0) {
        w.u32(1);
        putFattr3(w, st);
    } else {
        w.u32(0);
    }
}

// Build the fixed RPC "accepted success" reply prefix for a given xid.
void putReplyHeader(XdrWriter& w, quint32 xid) {
    w.u32(xid);
    w.u32(1);       // msg_type = REPLY
    w.u32(0);       // reply_stat = MSG_ACCEPTED
    w.u32(0); w.u32(0);   // verf: flavor=AUTH_NONE, len=0
    w.u32(0);       // accept_stat = SUCCESS
}

} // namespace

NfsServer::NfsServer(QObject* parent) : QObject(parent) {}

QByteArray NfsServer::fileHandle(const QString& path) {
    const QString clean = QDir::cleanPath(path);
    if (m_pathToHandle.contains(clean)) return m_pathToHandle.value(clean);
    QByteArray fh(32, '\0');
    const quint32 id = m_nextHandle++;
    fh[0] = char(id >> 24); fh[1] = char(id >> 16); fh[2] = char(id >> 8); fh[3] = char(id);
    m_pathToHandle.insert(clean, fh);
    m_handleToPath.insert(fh, clean);
    return fh;
}

bool NfsServer::start(const QString& exportDir, quint16 port) {
    stop();
    m_export = QDir::cleanPath(exportDir);
    m_port = port;
    fileHandle(m_export);   // handle #1 = export root

    m_nfs = new QUdpSocket(this);
    if (!m_nfs->bind(QHostAddress::LocalHost, port)) { stop(); return false; }
    connect(m_nfs, &QUdpSocket::readyRead, this, &NfsServer::onReadyRead);

    // The portmapper on 111 is best-effort (needs privilege on some systems).
    m_portmap = new QUdpSocket(this);
    if (m_portmap->bind(QHostAddress::LocalHost, 111))
        connect(m_portmap, &QUdpSocket::readyRead, this, &NfsServer::onReadyRead);
    else { m_portmap->deleteLater(); m_portmap = nullptr; }
    return true;
}

void NfsServer::stop() {
    if (m_nfs) { m_nfs->close(); m_nfs->deleteLater(); m_nfs = nullptr; }
    if (m_portmap) { m_portmap->close(); m_portmap->deleteLater(); m_portmap = nullptr; }
    m_handleToPath.clear(); m_pathToHandle.clear(); m_nextHandle = 1;
}

bool NfsServer::isRunning() const { return m_nfs && m_nfs->state() == QAbstractSocket::BoundState; }

void NfsServer::onReadyRead() {
    for (QUdpSocket* s : {m_nfs, m_portmap}) {
        if (!s) continue;
        while (s->hasPendingDatagrams()) {
            QByteArray buf(int(s->pendingDatagramSize()), '\0');
            QHostAddress from; quint16 fromPort = 0;
            s->readDatagram(buf.data(), buf.size(), &from, &fromPort);
            const QByteArray reply = handleDatagram(buf);
            if (!reply.isEmpty()) s->writeDatagram(reply, from, fromPort);
        }
    }
}

QByteArray NfsServer::handleDatagram(const QByteArray& request) {
    XdrReader r(request);
    bool ok = true;
    const quint32 xid = r.u32(&ok);
    const quint32 msgType = r.u32(&ok);      // 0 = CALL
    const quint32 rpcvers = r.u32(&ok);
    const quint32 prog = r.u32(&ok);
    const quint32 vers = r.u32(&ok);
    const quint32 proc = r.u32(&ok);
    if (!ok || msgType != 0 || rpcvers != 2) return {};
    // Skip credentials + verifier (flavor + opaque each).
    r.u32(&ok); r.opaqueVar(&ok);   // cred
    r.u32(&ok); r.opaqueVar(&ok);   // verf
    if (!ok) return {};

    XdrWriter w;
    putReplyHeader(w, xid);

    // ── Portmapper: GETPORT (proc 3) → our NFS port for NFS/MOUNT. ──
    if (prog == PROG_PORTMAP) {
        if (proc == 0) return w.data();                 // NULL
        if (proc == 3) { w.u32(m_port); return w.data(); }
        w.u32(0); return w.data();
    }

    // ── MOUNT v3 ──
    if (prog == PROG_MOUNT) {
        if (proc == 0) return w.data();                 // NULL
        if (proc == 1) {                                // MNT(dirpath) → root fh
            const QString dir = r.str(&ok);
            Q_UNUSED(dir);
            w.u32(NFS3_OK);
            w.opaqueVar(fileHandle(m_export));          // fhandle3
            w.u32(1); w.u32(0);                          // one auth flavor: AUTH_NONE
            return w.data();
        }
        if (proc == 3) { w.u32(0); return w.data(); }   // UMNT
        if (proc == 5) { w.u32(0); return w.data(); }   // EXPORT: empty list
        w.u32(NFS3ERR_ACCES); return w.data();
    }

    // ── NFS v3 ──
    if (prog == PROG_NFS && vers == 3) {
        if (proc == 0) return w.data();                 // NULL

        auto readFh = [&](bool* okp) { return r.opaqueVar(okp); };

        if (proc == 1) {                                // GETATTR(fh)
            const QString path = pathForHandle(readFh(&ok));
            struct stat st{};
            if (ok && !path.isEmpty() && ::stat(path.toUtf8().constData(), &st) == 0) {
                w.u32(NFS3_OK); putFattr3(w, st);
            } else w.u32(NFS3ERR_NOENT);
            return w.data();
        }
        if (proc == 3) {                                // LOOKUP(dirfh, name)
            const QString dir = pathForHandle(readFh(&ok));
            const QString name = r.str(&ok);
            const QString child = QDir(dir).filePath(name);
            struct stat st{};
            if (ok && !dir.isEmpty() && ::stat(child.toUtf8().constData(), &st) == 0) {
                w.u32(NFS3_OK);
                w.opaqueVar(fileHandle(child));         // object fh
                putPostOpAttr(w, child);                // obj attrs
                putPostOpAttr(w, dir);                  // dir attrs
            } else {
                w.u32(NFS3ERR_NOENT);
                putPostOpAttr(w, dir);
            }
            return w.data();
        }
        if (proc == 4) {                                // ACCESS(fh, mask)
            const QString path = pathForHandle(readFh(&ok));
            const quint32 mask = r.u32(&ok);
            w.u32(NFS3_OK);
            putPostOpAttr(w, path);
            w.u32(mask & 0x1F);                          // grant read/lookup bits
            return w.data();
        }
        if (proc == 5) {                                // READLINK(fh)
            const QString path = pathForHandle(readFh(&ok));
            const QByteArray target = QFile::symLinkTarget(path).toUtf8();
            w.u32(NFS3_OK); putPostOpAttr(w, path); w.opaqueVar(target);
            return w.data();
        }
        if (proc == 6) {                                // READ(fh, offset, count)
            const QString path = pathForHandle(readFh(&ok));
            const quint64 offset = r.u64(&ok);
            const quint32 count = r.u32(&ok);
            QFile f(path);
            if (ok && f.open(QIODevice::ReadOnly)) {
                f.seek(offset);
                const QByteArray data = f.read(qMin<quint64>(count, 65536));
                const bool eof = f.atEnd();
                w.u32(NFS3_OK); putPostOpAttr(w, path);
                w.u32(static_cast<quint32>(data.size()));
                w.u32(eof ? 1 : 0);
                w.opaqueVar(data);
            } else { w.u32(NFS3ERR_ACCES); putPostOpAttr(w, path); }
            return w.data();
        }
        if (proc == 16 || proc == 17) {                 // READDIR / READDIRPLUS
            const QString path = pathForHandle(readFh(&ok));
            // cookie(u64), cookieverf(8) [, dircount, maxcount for plus]
            const quint64 cookie = r.u64(&ok);
            r.opaqueFixed(8, &ok);                       // cookieverf
            w.u32(NFS3_OK);
            putPostOpAttr(w, path);
            w.opaqueFixed(QByteArray(8, '\0'));          // cookieverf
            const QStringList entries = QDir(path, QString(), QDir::Name,
                QDir::AllEntries | QDir::Hidden | QDir::System).entryList();
            quint64 idx = 0;
            for (const QString& name : entries) {
                if (idx++ < cookie) continue;
                const QString child = QDir(path).filePath(name);
                struct stat st{};
                ::stat(child.toUtf8().constData(), &st);
                w.u32(1);                                // value follows
                w.u64(static_cast<quint64>(st.st_ino ? st.st_ino : idx));   // fileid
                w.str(name);                             // name
                w.u64(idx);                              // cookie
                if (proc == 17) {                        // READDIRPLUS extras
                    putPostOpAttr(w, child);             // name_attributes
                    w.u32(1); w.opaqueVar(fileHandle(child));   // name_handle
                }
                if (idx - cookie >= 64) break;           // cap per response
            }
            w.u32(0);                                    // no more entries
            w.u32(idx >= quint64(entries.size()) ? 1 : 0);   // eof
            return w.data();
        }
        if (proc == 18) {                               // FSSTAT(fh)
            const QString path = pathForHandle(readFh(&ok));
            w.u32(NFS3_OK); putPostOpAttr(w, path);
            w.u64(1ull << 40); w.u64(1ull << 39); w.u64(1ull << 39);   // tbytes/fbytes/abytes
            w.u64(1ull << 20); w.u64(1ull << 19); w.u64(1ull << 19);   // tfiles/ffiles/afiles
            w.u32(0);                                    // invarsec
            return w.data();
        }
        if (proc == 19) {                               // FSINFO(fh)
            const QString path = pathForHandle(readFh(&ok));
            w.u32(NFS3_OK); putPostOpAttr(w, path);
            w.u32(65536); w.u32(65536); w.u32(4096);     // rtmax/rtpref/rtmult
            w.u32(65536); w.u32(65536); w.u32(4096);     // wtmax/wtpref/wtmult
            w.u32(4096);                                 // dtpref
            w.u64(1ull << 40);                           // maxfilesize
            w.u32(1); w.u32(0);                          // time_delta
            w.u32(0x1B);                                 // properties
            return w.data();
        }
        if (proc == 20) {                               // PATHCONF(fh)
            const QString path = pathForHandle(readFh(&ok));
            w.u32(NFS3_OK); putPostOpAttr(w, path);
            w.u32(32000); w.u32(255);                    // linkmax / name_max
            w.u32(0); w.u32(1); w.u32(1); w.u32(0);      // no_trunc/chown_restricted/case*
            return w.data();
        }
        // Unsupported (write ops etc.).
        w.u32(NFS3ERR_ACCES);
        return w.data();
    }

    // Unknown program.
    return {};
}

} // namespace macxterm::tools
