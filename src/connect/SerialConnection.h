#pragma once
// Serial support is optional: it compiles only when the Qt SerialPort add-on is
// present (MACXTERM_HAVE_SERIALPORT, set by CMake). When absent, this header is
// empty and Serial sessions fall back to a local shell (see MainWindow::makePane).
#if defined(MACXTERM_HAVE_SERIALPORT)
#include "connect/IConnection.h"
#include <QSerialPort>

namespace macxterm::connect {

// Serial console session (research §1.1). Config is parsed from Session params
// ("port","baud","databits","parity","stopbits","flow") — parseConfig() is
// pure and unit-tested; open() needs real hardware.
class SerialConnection : public IConnection {
    Q_OBJECT
public:
    struct Config {
        QString portName;
        qint32 baud = 9600;
        QSerialPort::DataBits dataBits = QSerialPort::Data8;
        QSerialPort::Parity parity = QSerialPort::NoParity;
        QSerialPort::StopBits stopBits = QSerialPort::OneStop;
        QSerialPort::FlowControl flow = QSerialPort::NoFlowControl;
    };

    // Pure: derive a Config from a Session's params (default 9600 8N1).
    static Config parseConfig(const core::Session& session);

    explicit SerialConnection(QObject* parent = nullptr);

    bool connectSession(const core::Session& session) override;
    void disconnectSession() override;
    qint64 send(const QByteArray& data) override;
    Capabilities capabilities() const override { return {false, false, false, false}; }

private slots:
    void onReadyRead();

private:
    QSerialPort* m_port = nullptr;
};

} // namespace macxterm::connect
#endif // MACXTERM_HAVE_SERIALPORT
