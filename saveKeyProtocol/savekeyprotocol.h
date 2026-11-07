#ifndef SAVEKEYPROTOCOL_H
#define SAVEKEYPROTOCOL_H

#include <QObject>
#include <QDebug>
#include <QTimer>
#include <QtQml>

#pragma pack(push, 1)

// Enums are placed inside a registered namespace to enable Q_ENUM functionality in QML.
namespace SaveKey {
Q_NAMESPACE

/*!
        \enum KeyNum
        \brief Defines all key, sensor, actuator and fan identifiers used in the protocol.

        \value Key01      Main key identifier 1 (Value: 0x11).
        \value Key02      Main key identifier 2.
        \value Key03      Main key identifier 3.
        \value Key04      Main key identifier 4.
        \value Key05      Main key identifier 5.
        \value Key06      Main key identifier 6.
        \value Key07      Main key identifier 7.

        \value Sen01      Sensor identifier 1 (Value: 0x01).
        \value Sen02      Sensor identifier 2.
        \value Sen03      Sensor identifier 3.
        \value Sen04      Sensor identifier 4.
        \value Sen05      Sensor identifier 5.
        \value Sen06      Sensor identifier 6.

        \value Open1      Curtain/door 1 open command (Value: 0x31).
        \value Stop1      Curtain/door 1 stop command.
        \value Close1     Curtain/door 1 close command.
        \value Open2      Curtain/door 2 open command.
        \value Stop2      Curtain/door 2 stop command.
        \value Close2     Curtain/door 2 close command.
        \value Open3      Curtain/door 3 open command.
        \value Stop3      Curtain/door 3 stop command.
        \value Close3     Curtain/door 3 close command.

        \value Valve1     Valve control identifier 1 (Value: 0x40).
        \value Fan1       Fan control identifier 1.
        \value Fan2       Fan control identifier 2.
        \value Fan3       Fan control identifier 3.
    */
enum KeyNum : uint8_t
{
    Key01                       = 0x11,
    Key02                       ,
    Key03                       ,
    Key04                       ,
    Key05                       ,
    Key06                       ,
    Key07                       ,
    Sen01                       = 0x01,
    Sen02                       ,
    Sen03                       ,
    Sen04                       ,
    Sen05                       ,
    Sen06                       ,
    Open1                       = 0x31,
    Stop1                       ,
    Close1                      ,
    Open2                       ,
    Stop2                       ,
    Close2                      ,
    Open3                       ,
    Stop3                       ,
    Close3                      ,
    Valve1                      = 0x40,
    Fan1                        ,
    Fan2                        ,
    Fan3
};
Q_ENUM_NS(KeyNum)

/*!
        \enum HeaderType
        \brief Protocol frame header types to distinguish incoming packets.

        \value Key              Header type for key and actuator control messages (0x11).
        \value Thermostat       Header type for thermostat configuration and status messages (0x01).
        \value Curtain          Header type for curtain control messages (0x13).
    */
enum HeaderType : uint8_t
{
    Key                         = 0x11,
    Thermostat                  = 0x01,
    Curtain                     = 0x13
};
Q_ENUM_NS(HeaderType)

/*!
        \enum KeyState
        \brief Key request and response state actions.

        \value Reqoff            Request to turn target key/relay OFF (0x81).
        \value ReqOn             Request to turn target key/relay ON (0x82).
        \value ResOff           Response indicating target state is OFF (0x01).
        \value ResOn            Response indicating target state is ON (0x02).
    */
enum KeyState : uint8_t
{
    Reqoff                      = 0x81, /*!< Request to turn the target key or relay off. */
    ReqOn                       ,       /*!< Request to turn the target key or relay on. */
    ResOff                      = 0x01, /*!< Response reporting that the target is off. */
    ResOn                               /*!< Response reporting that the target is on. */
};
Q_ENUM_NS(KeyState)

/*!
        \enum ThermostatState
        \brief Operational state of the thermostat controller.
    */
enum ThermostatState : uint8_t
{
    StateNoChange               = 0xFF, /*!< Leave the current operating state unchanged. */
    Off                         = 0x00, /*!< Turn the thermostat off. */
    Heater                      = 0x01, /*!< Enable heating mode. */
    Cool                        = 0x02, /*!< Enable cooling mode. */
    constantTemp                = 0x03, /*!< Enable constant-temperature mode. */
    floorHeating                = 0x04, /*!< Enable underfloor heating mode. */
    fastHeat                    = 0x05, /*!< Enable fast-heating boost mode. */
    freshDry                    = 0x06, /*!< Enable fresh-air and dehumidification mode. */
    dry                         = 0x07  /*!< Enable dehumidification mode. */
};
Q_ENUM_NS(ThermostatState)

/*!
        \enum FanState
        \brief Thermostat fan speed levels and control modes.
    */
enum FanState : uint8_t
{
    FanNoChange                 = 0xFF, /*!< Leave the current fan setting unchanged. */
    One                         = 0x01, /*!< Select low fan speed. */
    Two                         = 0x02, /*!< Select medium fan speed. */
    Three                       = 0x03, /*!< Select high fan speed. */
    Auto                        = 0x04  /*!< Select automatic fan speed. */
};
Q_ENUM_NS(FanState)
}

// Register in the Qt meta-type system for use in signal and slot signatures
Q_DECLARE_METATYPE(SaveKey::KeyNum)
Q_DECLARE_METATYPE(SaveKey::HeaderType)
Q_DECLARE_METATYPE(SaveKey::KeyState)
Q_DECLARE_METATYPE(SaveKey::ThermostatState)
Q_DECLARE_METATYPE(SaveKey::FanState)

/*!
    \struct KeyHeader
    \brief Protocol frame structure for key-related configuration and control messages.
*/
struct KeyHeader
{
    SaveKey::HeaderType header;      /*!< Header identifier (typically HeaderType::Key) */
    SaveKey::KeyState state;         /*!< Requested or reported key state */
    uint8_t ID;                      /*!< Device hardware identifier */
    SaveKey::KeyNum KeySelect;       /*!< Target key, sensor or actuator index */
    uint8_t checksum;                /*!< Packet verification checksum */
};

/*!
    \struct ThermostatHeader
    \brief Protocol frame representing the comprehensive status of a thermostat device.
*/
struct ThermostatHeader
{
    SaveKey::HeaderType header;          /*!< Header identifier (HeaderType::Thermostat) */
    uint8_t Fixed0;                      /*!< Protocol-defined fixed byte 0 */
    uint8_t Fixed1;                      /*!< Protocol-defined fixed byte 1 */
    uint8_t ID;                          /*!< Unique thermostat device identifier */
    SaveKey::ThermostatState State;      /*!< Active operation mode */
    uint8_t Fixed2;                      /*!< Protocol-defined fixed byte 2 */
    SaveKey::FanState Fanstate;          /*!< Current fan speed selection */
    uint8_t TempSetPoint;                /*!< Desired setpoint temperature */
    uint8_t CurrentTemp;                 /*!< Ambient temperature read by the sensor */
    uint8_t FanSpeed_ValveState;         /*!< Combined state: high nibble is fan speed; low nibble is valve state. */
    uint8_t checksum;                    /*!< Packet verification checksum */
};

/*!
    \struct ThermostatConfigure
    \brief Configuration command packet sent to a thermostat unit.
*/
struct ThermostatConfigure
{
    SaveKey::HeaderType header;          /*!< Header identifier (HeaderType::Thermostat) */
    uint8_t Fixed0;                      /*!< Protocol-defined fixed byte (typically 0x85) */
    uint8_t Fixed1;                      /*!< Protocol-defined fixed byte (typically 0x00) */
    uint8_t ID;                          /*!< Target thermostat device identifier */
    SaveKey::ThermostatState State;      /*!< Mode of operation to set */
    uint8_t Fixed2;                      /*!< Protocol-defined fixed byte (typically 0xFF) */
    SaveKey::FanState Fanstate;          /*!< Fan speed level to set */
    uint8_t TempSetPoint;                /*!< Target setpoint temperature to apply */
    uint8_t checksum;                    /*!< Packet verification checksum */
};

/*!
    \struct ThermostatControl
    \brief Basic command packet to trigger thermostat states.
*/
struct ThermostatControl
{
    SaveKey::HeaderType header;          /*!< Header identifier (HeaderType::Thermostat) */
    SaveKey::KeyState state;             /*!< Target key or control action state */
    uint8_t Fixed0;                      /*!< Protocol-defined fixed byte (typically 0x00) */
    uint8_t ID;                          /*!< Target thermostat device identifier */
    uint8_t checksum;                    /*!< Packet verification checksum */
};

/*!
    \struct ScenarioHeader
    \brief Protocol frame used to execute, record, or relay specific system scenarios.
*/
struct ScenarioHeader
{
    SaveKey::HeaderType header;          /*!< Header identifier */
    SaveKey::KeyState state;             /*!< Operational state of the scenario */
    uint8_t ID;                          /*!< Scenario controller device ID */
    SaveKey::KeyNum KeySelect;           /*!< Selected scenario trigger key */
    uint8_t Fixed0;                      /*!< Reserved fixed byte */
    uint8_t ID_Replay;                   /*!< Target device ID for replay execution */
    SaveKey::KeyNum KeySelect_Replay;    /*!< Target key identifier to execute in replay */
    uint8_t checksum;                    /*!< Packet verification checksum */
};

/*!
    \struct KeysReport
    \brief Asynchronous or requested report containing system inputs and outputs.
*/
struct KeysReport
{
    SaveKey::HeaderType header;          /*!< Header identifier */
    SaveKey::KeyState state;             /*!< System report state context */
    uint8_t ID;                          /*!< Reporting device identifier */
    uint8_t keys;                        /*!< Bitmask or state values representing physical keys */
    uint8_t sens;                        /*!< Sensor state values or inputs */
    uint8_t unknown0;                    /*!< Reserved for future protocol additions */
    uint8_t unknown1;                    /*!< Reserved for future protocol additions */
    uint8_t unknown2;                    /*!< Reserved for future protocol additions */
    uint8_t unknown3;                    /*!< Reserved for future protocol additions */
    uint8_t unknown4;                    /*!< Reserved for future protocol additions */
    uint8_t checksum;                    /*!< Packet verification checksum */
};

#pragma pack(pop)


/*!
    * @class SaveKeyProtocol
    * @brief Handles the communication protocol for SaveKey devices.
    *
    * This class provides mechanisms for parsing incoming byte streams from SaveKey hardware,
    * validating frames using checksums, and generating request packets for keys and thermostats.
    * It uses an internal buffer and a timer to handle asynchronous data reception.
 */
class SaveKeyProtocol : public QObject
{
    Q_OBJECT
public:
    /*!
     * \brief Creates a SaveKey protocol parser and request encoder.
     * \param parent Optional QObject parent for ownership management.
     */
    explicit SaveKeyProtocol(QObject *parent = nullptr);

    /*! \brief Registers the SaveKey enums and protocol type with the QML type system. */
    static void registerQmlTypes() {
        qmlRegisterUncreatableMetaObject(
            SaveKey::staticMetaObject,
            "com.savekey.protocol",
            1, 0,
            "SaveKey",
            "Access to SaveKey protocol enums"
            );
        qmlRegisterType<SaveKeyProtocol>("com.savekey.protocol", 1, 0, "SaveKeyProtocol");
    }

private:
    /*! \brief Maximum number of unprocessed input bytes retained by the parser. */
    static constexpr size_t maxBufferSize = 100;

    /*! \brief Incoming byte stream awaiting protocol frame parsing. */
    QByteArray m_protocolBuffer;

    /*! \brief Timer used to defer parsing until a short input burst has arrived. */
    QTimer processBufferTimer;

    /**
        * \brief Checks whether a packet's final byte matches its additive checksum.
        * \param data Pointer to the packet bytes.
        * \param length Total packet length, including the checksum byte.
        * \return true if the checksum is valid; otherwise false.
    */
    bool validateChecksum(const uint8_t *data, int length);

    /**
        * \brief Calculates the additive 8-bit checksum for a packet buffer.
        * \param data Pointer to the packet bytes.
        * \param length Total packet length, including the checksum byte position.
        * \return Sum of all bytes except the final checksum byte, truncated to 8 bits.
    */
    uint8_t checksum(const uint8_t *data, int length);

    /**
        * @brief Core parser that scans the internal buffer for valid protocol frames.
        *
        * This method identifies KeyHeaders, ScenarioHeaders, KeysReports, and ThermostatHeaders.
        * It handles noise by shifting the buffer and emits signals upon successful parsing.
    */
    void processBuffer(void);

signals:
    /*!
     * \brief Emits an encoded protocol packet for transmission.
     * \param data Pointer to the packet bytes.
     * \param length Packet size in bytes.
     */
    void sendRawData(const char *data, int length);

    /*!
     * \brief Emits a parsed thermostat status frame.
     * \param ID Device identifier.
     * \param State Reported operating state.
     * \param Fanstate Reported fan mode.
     * \param TempSetPoint Configured temperature setpoint.
     * \param CurrentTemp Measured temperature.
     * \param FanSpeed Decoded fan speed.
     * \param valveState Decoded valve state.
     */
    void thermostatsResponse(int ID, SaveKey::ThermostatState State, SaveKey::FanState Fanstate, int TempSetPoint, int CurrentTemp, int FanSpeed, int valveState);
    /*!
     * \brief Emitted when a key, sensor or actuator state frame has been parsed.
     * \param ID Device identifier reported by the frame.
     * \param state Parsed key state.
     * \param KeySelect Key, sensor or actuator identifier.
     */
    void keysResponse(int ID, SaveKey::KeyState state, SaveKey::KeyNum KeySelect);

public slots:
    /**
        * \brief Appends received bytes and schedules protocol frame parsing.
        *
        * Appends data to an internal buffer and restarts the processing timer.
        * \param currentBuffer Newly received bytes from the transport layer.
    */
    void onDataReceived(const QByteArray &currentBuffer);

    /**
        * \brief Encodes and emits a thermostat on/off control request.
        * @param deviceId The ID of the target thermostat device.
        * @param state The desired state (ReqOn/ReqOff).
    */
    void onThermostatSwitchOnOffRequest(uint8_t deviceId, SaveKey::KeyState state);

    /**
        * \brief Encodes and emits a thermostat configuration request.
        * @param deviceId The ID of the target thermostat device.
        * \param state Requested thermostat operating state.
        * \param fanState Requested fan mode.
        * \param tempSetPoint Requested temperature setpoint.
    */
    void onThermostatConfigurationRequest(uint8_t deviceId, SaveKey::ThermostatState state, SaveKey::FanState fanState, uint8_t tempSetPoint);

    /**
        * \brief Encodes and emits a key, sensor or actuator state request.
        * \param deviceId Target device identifier.
        * \param header Protocol frame type.
        * \param state Requested state.
        * \param key Key, sensor or actuator identifier.
    */
    void onKeyStateRequest(uint8_t deviceId, SaveKey::HeaderType header, SaveKey::KeyState state, SaveKey::KeyNum key);

private slots:
    /*! \brief Parses buffered bytes when the input processing timer expires. */
    void processBufferTimeout(void);
};

#endif // SAVEKEYPROTOCOL_H
