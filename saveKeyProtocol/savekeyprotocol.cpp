#include "savekeyprotocol.h"

SaveKeyProtocol::SaveKeyProtocol(QObject *parent)
    : QObject{parent}
{
    processBufferTimer.setInterval(50);
    connect(&processBufferTimer,&QTimer::timeout,this,&SaveKeyProtocol::processBufferTimeout);
}

bool SaveKeyProtocol::validateChecksum(const uint8_t *data, int length)
{
    if (length < 2) return false;

    uint8_t sum = 0;
    for (int i = 0; i < length - 1; ++i) {
        sum += data[i];
    }

    return (sum == data[length - 1]);
}

uint8_t SaveKeyProtocol::checksum(const uint8_t *data, int length)
{
    uint8_t sum = 0;
    for (int i = 0; i < length - 1; ++i) {
        sum += data[i];
    }

    return sum;
}

void SaveKeyProtocol::processBuffer()
{
    int index{0},removePoint{0};

    // As long as at least the minimum packet size (which is KeyHeader with 5 bytes) exists in the buffer
    while (index <= m_protocolBuffer.size() - 5)
    {
        uint8_t headerByte = m_protocolBuffer.data()[index];
        const uint8_t* ptr = (const uint8_t*)(m_protocolBuffer.constData() + index);

        // 1. Check Key type header
        if (headerByte == SaveKey::HeaderType::Key)
        {
            // In your protocol, the Key header can represent KeyHeader, ScenarioHeader, or KeysReport.
            // So we must differentiate step-by-step based on the structure and checksum.

            // A) Test for KeyHeader (length: 5 bytes)
            if (index + 5 <= m_protocolBuffer.size())
            {

                if (validateChecksum(ptr, 5))
                {
                    KeyHeader *packet = (KeyHeader*)ptr;

                    QByteArray raw(reinterpret_cast<const char*>(packet), sizeof(KeyHeader));
                    //qDebug() << raw.toHex(' ').toUpper();

                    //qDebug() << QString("keyHeaderReceived - id: %1 - keynumber: %2").arg(packet->ID).arg(packet->KeySelect);
                    emit keysResponse(packet->ID,packet->state,packet->KeySelect);

                    // Remove the processed packet from the buffer
                    index += 5;
                    removePoint = index;
                    continue; // Continue checking from the same position without incrementing the index
                }
            }

            // B) Test for ScenarioHeader (length: 8 bytes)
            if (index + 8 <= m_protocolBuffer.size())
            {
                if (validateChecksum(ptr, 8))
                {
                    ScenarioHeader *packet = (ScenarioHeader*)ptr;

                    //qDebug() << QString("scenarioHeaderReceived: - id: %1 - keynumber: %2").arg(packet->ID).arg(packet->KeySelect);
                    emit keysResponse(packet->ID,packet->state,packet->KeySelect);

                    index += 8;
                    removePoint = index;
                    continue;
                }
            }

            // C) Test for KeysReport (length: 11 bytes)
            if (index + 11 <= m_protocolBuffer.size())
            {
                if (validateChecksum(ptr, 11))
                {
                    KeysReport *packet = (KeysReport*)ptr;
                    uint8_t keys = packet->keys;
                    uint8_t sens = packet->sens;

                    for (int bit = 0; bit < 8; ++bit)
                    {
                        bool keyState = (keys >> bit) & 0x01;
                        bool senState = (sens >> bit) & 0x01;
                        (keyState)?emit keysResponse(packet->ID,SaveKey::ResOn,(SaveKey::KeyNum)(SaveKey::Key01 + bit)):emit keysResponse(packet->ID,SaveKey::ResOff,(SaveKey::KeyNum)(SaveKey::Key01 + bit));
                        (senState)?emit keysResponse(packet->ID,SaveKey::ResOn,(SaveKey::KeyNum)(SaveKey::Sen01 + bit)):emit keysResponse(packet->ID,SaveKey::ResOff,(SaveKey::KeyNum)(SaveKey::Sen01 + bit));
                    }

                    //qDebug() << QString("*keysReportReceived - id: %1 - keyState: %2 - sensState: %3").arg(packet->ID).arg(packet->keys).arg(packet->sens);

                    index += 11;
                    removePoint = index;
                    continue;
                }
            }
        }
        // 2. Check Thermostate type header (length: 11 bytes)
        else if (headerByte == SaveKey::HeaderType::Thermostat)
        {
            if (index + 11 <= m_protocolBuffer.size())
            {
                if (validateChecksum(ptr, 11))
                {
                    ThermostatHeader *packet = (ThermostatHeader*)ptr;

                    // qDebug() << QString("thermostatHeaderReceived - id: %1 - fanState: %2 - setPoint: %3 - currentTemp: %4 - fan&valve: %5 - state: %6")
                    //                 .arg(packet->ID).arg(packet->Fanstate).arg(packet->TempSetPoint).arg(packet->CurrentTemp)
                    //                 .arg(packet->FanSpeed_ValveState).arg(packet->State);

                    uint8_t combined = packet->FanSpeed_ValveState;

                    uint8_t fanSpeed   = (combined >> 4) & 0x0F;  // High nibble
                    uint8_t valveState =  combined       & 0x0F;  // Low nibble
                    emit thermostatsResponse(packet->ID,packet->State,packet->Fanstate,packet->TempSetPoint,packet->CurrentTemp,fanSpeed,valveState);

                    index += 11;
                    removePoint = index;
                    continue;
                }
            }
            // QByteArray raw(reinterpret_cast<const char*>(ptr), 9);
            // qDebug() << raw.toHex(' ').toUpper();
        }


        // If the start byte did not match any header, or the packet checksum validation failed:
        // Shift forward by one byte to eliminate corrupted frames or noise.
        index++;
    }

    m_protocolBuffer.remove(0,removePoint);
}

void SaveKeyProtocol::onThermostatSwitchOnOffRequest(uint8_t deviceId, SaveKey::KeyState state)
{
    ThermostatControl control;
    control.ID = deviceId;
    control.Fixed0 = 0x00;
    control.header = SaveKey::Thermostat;
    control.state = state;
    control.checksum = checksum((uint8_t *)&control,sizeof(ThermostatControl));
    QByteArray raw(reinterpret_cast<const char*>(&control), sizeof(ThermostatControl));
    emit sendRawData((const char *)&control,sizeof(ThermostatControl));
}

void SaveKeyProtocol::onThermostatConfigurationRequest(uint8_t deviceId, SaveKey::ThermostatState state, SaveKey::FanState fanState, uint8_t tempSetPoint)
{
    ThermostatConfigure sendPacket;
    sendPacket.header = SaveKey::Thermostat;
    sendPacket.TempSetPoint = tempSetPoint;
    sendPacket.Fanstate = fanState;
    sendPacket.State = state;
    sendPacket.ID = deviceId;
    sendPacket.Fixed0 = 0x85;
    sendPacket.Fixed1 = 0x00;
    sendPacket.Fixed2 = 0xFF;
    sendPacket.checksum = checksum((uint8_t *)&sendPacket,sizeof(ThermostatConfigure));
    QByteArray raw(reinterpret_cast<const char*>(&sendPacket), sizeof(ThermostatConfigure));
    emit sendRawData((const char *)&sendPacket,sizeof(sendPacket));
}

void SaveKeyProtocol::onKeyStateRequest(uint8_t deviceId, SaveKey::HeaderType header, SaveKey::KeyState state, SaveKey::KeyNum key)
{
    KeyHeader sendPacket;
    sendPacket.header = header;
    sendPacket.state = state;
    sendPacket.ID = deviceId;
    sendPacket.KeySelect = key;
    sendPacket.checksum = checksum((uint8_t *)&sendPacket,sizeof(KeyHeader));
    QByteArray raw(reinterpret_cast<const char*>(&sendPacket), sizeof(KeyHeader));
    emit sendRawData((const char *)&sendPacket,sizeof(sendPacket));
}

void SaveKeyProtocol::onDataReceived(const QByteArray &currentBuffer)
{
    m_protocolBuffer.append(currentBuffer);
    if(m_protocolBuffer.size() > maxBufferSize)
    {
        const qsizetype overflow = m_protocolBuffer.size() - maxBufferSize;
        m_protocolBuffer.remove(0, overflow);
    }
    processBufferTimer.start();
}

void SaveKeyProtocol::processBufferTimeout()
{
    if(!m_protocolBuffer.isEmpty())
    {
        processBuffer();
    }
    processBufferTimer.stop();
}
