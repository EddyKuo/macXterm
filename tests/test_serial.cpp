#include "connect/SerialConnection.h"
#include <QtTest/QtTest>

using namespace macxterm;

class TestSerial : public QObject {
    Q_OBJECT
private slots:
    void defaults8N1() {
        core::Session s("com", core::SessionType::Serial);
        auto c = connect::SerialConnection::parseConfig(s);
        QCOMPARE(c.baud, 9600);
        QCOMPARE(c.dataBits, QSerialPort::Data8);
        QCOMPARE(c.parity, QSerialPort::NoParity);
        QCOMPARE(c.stopBits, QSerialPort::OneStop);
    }

    void parsesCustomConfig() {
        core::Session s("com", core::SessionType::Serial);
        s.setParam("port", "/dev/ttyUSB0");
        s.setParam("baud", "115200");
        s.setParam("databits", "7");
        s.setParam("parity", "even");
        s.setParam("stopbits", "2");
        s.setParam("flow", "hardware");
        auto c = connect::SerialConnection::parseConfig(s);
        QCOMPARE(c.portName, QStringLiteral("/dev/ttyUSB0"));
        QCOMPARE(c.baud, 115200);
        QCOMPARE(c.dataBits, QSerialPort::Data7);
        QCOMPARE(c.parity, QSerialPort::EvenParity);
        QCOMPARE(c.stopBits, QSerialPort::TwoStop);
        QCOMPARE(c.flow, QSerialPort::HardwareControl);
    }
};

QTEST_APPLESS_MAIN(TestSerial)
#include "test_serial.moc"
