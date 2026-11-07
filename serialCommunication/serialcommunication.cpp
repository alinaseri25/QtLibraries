#include "serialcommunication.h"
#include <QTimer>

SerialCommunication::SerialCommunication(QObject *parent)
    : QObject{parent}
{
    m_serialPort = nullptr;
}

void SerialCommunication::checkState()
{
    m_lastError = QString("");
    emit portStateChanged(m_serialPort != nullptr && m_serialPort->isOpen(),NoError);
}

void SerialCommunication::setPortDetails(const QString &portName, uint32_t baudRate)
{
    checkSerialObjectavailable();
    if(m_serialPort->isOpen())
    {
        m_serialPort->close();
        m_lastError = QString("due to configuration changed, port closed");
        emit portStateChanged(m_serialPort->isOpen(),ConfiqurationChanged,m_lastError);
    }
    m_portName = portName;
    m_baudRate = baudRate;
    m_serialPort->setBaudRate(m_baudRate);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setPortName(m_portName);
}

void SerialCommunication::startCapture()
{
    checkSerialObjectavailable();
    if(!m_serialPort->open(QIODevice::ReadWrite))
    {
        m_lastError = QString("cannot open current serial port");
        emit portStateChanged(m_serialPort->isOpen(),FailedToOpenPort,m_lastError);
    }
    checkState();
}

void SerialCommunication::stopCapture()
{
    if(m_serialPort != nullptr && m_serialPort->isOpen())
    {
        m_serialPort->close();
    }
    checkSerialObjectavailable(false);
    checkState();
}

void SerialCommunication::addRawDataToSend(const char *data, int length)
{
    QByteArray temp;
    temp.append(data,length);
    m_txBuffer.enqueue(temp);
    if(sendDataTimer != nullptr)
    {
        sendDataTimer->start();
    }
    else
    {
        checkSerialObjectavailable();
    }
}

void SerialCommunication::checkSerialObjectavailable(bool deleteIt)
{
    if(deleteIt)
    {
        if(m_serialPort != nullptr)
        {
            m_serialPort->deleteLater();
            m_serialPort = nullptr;
        }
    }
    else
    {
        if(m_serialPort == nullptr)
        {
            m_serialPort = new QSerialPort(this);
            connect(m_serialPort,&QSerialPort::readyRead,this,&SerialCommunication::onReadyRead);
            connect(m_serialPort,&QSerialPort::errorOccurred,this,&SerialCommunication::onErrorOccurred);
        }
        if(sendDataTimer == nullptr)
        {
            sendDataTimer = new QTimer(this);
            sendDataTimer->setInterval(5);
            connect(sendDataTimer,&QTimer::timeout,this,&SerialCommunication::onSendDataTimerTimeout);
            sendDataTimer->start();
        }
    }
}

void SerialCommunication::onReadyRead()
{
    emit newDataReceived(m_serialPort->readAll());
}

void SerialCommunication::onSendDataTimerTimeout()
{
    if(!m_txBuffer.empty())
    {
        if(m_serialPort->isOpen())
        {
            m_serialPort->write(m_txBuffer.dequeue());
        }
        else
        {
            m_lastError = "serial port Closed at this time\r\nplease select a serial port at first!";
            emit portStateChanged(m_serialPort->isOpen(),SerialPortClosed,m_lastError);
            m_txBuffer.clear();
            sendDataTimer->stop();
        }
    }
    else
    {
        sendDataTimer->stop();
    }
}

void SerialCommunication::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if(error == QSerialPort::NoError)
    {
        return;
    }

    m_lastError = m_serialPort->errorString();//QString("cannot open current serial port");
    emit portStateChanged(m_serialPort->isOpen(),SerialPortError,m_lastError);
}
