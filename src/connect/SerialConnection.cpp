#include "connect/SerialConnection.h"

namespace macxterm::connect {

SerialConnection::Config SerialConnection::parseConfig(const core::Session& session) {
    Config c;
    c.portName = session.param("port");
    bool ok = false;
    const qint32 b = session.param("baud").toInt(&ok);
    if (ok && b > 0) c.baud = b;

    switch (session.param("databits", "8").toInt()) {
        case 5: c.dataBits = QSerialPort::Data5; break;
        case 6: c.dataBits = QSerialPort::Data6; break;
        case 7: c.dataBits = QSerialPort::Data7; break;
        default: c.dataBits = QSerialPort::Data8; break;
    }
    const QString parity = session.param("parity", "none").toLower();
    if (parity == "even") c.parity = QSerialPort::EvenParity;
    else if (parity == "odd") c.parity = QSerialPort::OddParity;
    else c.parity = QSerialPort::NoParity;

    if (session.param("stopbits", "1") == "2") c.stopBits = QSerialPort::TwoStop;
    else c.stopBits = QSerialPort::OneStop;

    const QString flow = session.param("flow", "none").toLower();
    if (flow == "hardware" || flow == "rtscts") c.flow = QSerialPort::HardwareControl;
    else if (flow == "software" || flow == "xonxoff") c.flow = QSerialPort::SoftwareControl;
    else c.flow = QSerialPort::NoFlowControl;
    return c;
}

SerialConnection::SerialConnection(QObject* parent) : IConnection(parent) {}

bool SerialConnection::connectSession(const core::Session& session) {
    setState(State::Connecting);
    const Config c = parseConfig(session);
    m_port = new QSerialPort(this);
    m_port->setPortName(c.portName);
    m_port->setBaudRate(c.baud);
    m_port->setDataBits(c.dataBits);
    m_port->setParity(c.parity);
    m_port->setStopBits(c.stopBits);
    m_port->setFlowControl(c.flow);
    connect(m_port, &QSerialPort::readyRead, this, &SerialConnection::onReadyRead);
    if (!m_port->open(QIODevice::ReadWrite)) {
        setState(State::Failed);
        emit errorOccurred(m_port->errorString());
        return false;
    }
    setState(State::Connected);
    return true;
}

void SerialConnection::onReadyRead() {
    if (m_port) emit dataReceived(m_port->readAll());
}

qint64 SerialConnection::send(const QByteArray& data) {
    return m_port ? m_port->write(data) : -1;
}

void SerialConnection::disconnectSession() {
    if (m_port) { m_port->close(); m_port->deleteLater(); m_port = nullptr; }
    setState(State::Disconnected);
}

} // namespace macxterm::connect
