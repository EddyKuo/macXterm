#pragma once
#include <QByteArray>
#include <QString>

namespace macxterm::tools {

// Minimal XDR (RFC 4506) reader/writer for the ONC-RPC / NFS server. Big-endian,
// 4-byte aligned. Pure and unit-tested. Only the subset the NFS server needs is
// implemented: u32/u64, fixed and variable-length opaque, and strings.
class XdrWriter {
public:
    void u32(quint32 v);
    void u64(quint64 v);
    void opaqueFixed(const QByteArray& b);        // no length prefix, padded to 4
    void opaqueVar(const QByteArray& b);          // length prefix + padded bytes
    void str(const QString& s) { opaqueVar(s.toUtf8()); }
    const QByteArray& data() const { return m_buf; }

private:
    QByteArray m_buf;
};

class XdrReader {
public:
    explicit XdrReader(const QByteArray& b) : m_buf(b) {}
    bool atEnd() const { return m_pos >= m_buf.size(); }
    int pos() const { return m_pos; }

    quint32 u32(bool* ok = nullptr);
    quint64 u64(bool* ok = nullptr);
    QByteArray opaqueVar(bool* ok = nullptr);      // length-prefixed
    QByteArray opaqueFixed(int n, bool* ok = nullptr);
    QString str(bool* ok = nullptr) { return QString::fromUtf8(opaqueVar(ok)); }

private:
    QByteArray m_buf;
    int m_pos = 0;
};

} // namespace macxterm::tools
