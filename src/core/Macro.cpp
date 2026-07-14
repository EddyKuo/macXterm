#include "core/Macro.h"
#include <QDataStream>
#include <QIODevice>

namespace macxterm::core {

QByteArray Macro::serialize() const {
    QByteArray blob;
    QDataStream ds(&blob, QIODevice::WriteOnly);
    ds.setVersion(QDataStream::Qt_6_0);
    ds << static_cast<quint32>(m_events.size());
    for (const QByteArray& e : m_events) ds << e;  // length-prefixed by QDataStream
    return blob;
}

Macro Macro::deserialize(const QString& name, const QByteArray& blob) {
    Macro m(name);
    QDataStream ds(blob);
    ds.setVersion(QDataStream::Qt_6_0);
    quint32 n = 0;
    ds >> n;
    for (quint32 i = 0; i < n; ++i) {
        QByteArray e;
        ds >> e;
        if (ds.status() != QDataStream::Ok) break;
        m.m_events.push_back(e);
    }
    return m;
}

} // namespace macxterm::core
