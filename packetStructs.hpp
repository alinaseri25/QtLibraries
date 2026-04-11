#ifndef PACKETSTRUCTS_H
#define PACKETSTRUCTS_H

#include <stdint.h>

#define MAX_JSON_SIZE       (64*1024)
#define MAX_PAYLOAD_SIZE    (10*1024*1024)

#define jsonVersion         "00.01.05"
#define startByte            0xAA55AA55

enum EquipmentType{
    Other           = 0x01,
    Android         ,
    IOS             ,
    Desktop         ,
    Web             ,
    Stm32           ,
    ESP32
};

enum Errors{
    deviceExpired = 0x01,
    badLoginInput       ,
    userOrPassError     ,
    sessionMakerError   ,
    sessionExpired
};

enum MessageType{
    Request				= 0x01,
    Response			,
    ACK					,
    Handshake			,
    LoginResponse		,
    LoginRequest		,
    LogoutResponse		,
    LogoutRequest		,
    SessionResponse		,
    SessionRequest		,
    KeepAlive           ,
    AudioMessage        ,
    TextMessage
};

enum States{
    ok      = 0x01,
    nok
};

#pragma pack(push, 1)
struct DataHeader{
    uint32_t    startBytes = startByte;
    uint32_t    jsonSize;
    uint32_t    payloadSize;
    uint32_t    frameCRC;
};
#pragma pack(pop)

#endif
