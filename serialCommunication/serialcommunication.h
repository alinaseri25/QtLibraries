#ifndef SERIALCOMMUNICATION_H
#define SERIALCOMMUNICATION_H

#include <QObject>
#include <QQueue>
#include <QSerialPort>
#include <QDebug>

class QTimer;

class SerialCommunication : public QObject
{
    Q_OBJECT
public:
    /*!
     * \brief Creates the serial communication worker.
     * \param parent Optional QObject parent.
     *
     * The QSerialPort object and its readyRead connection are created lazily when
     * the serial port is first configured or opened.
     */
    explicit SerialCommunication(QObject *parent = nullptr);

    /*!
     * \brief Describes the outcome or current state of a serial communication operation.
     */
    enum SerialCommunicationErrors: uint8_t
    {
        NoError                 = 0x00, /*!< No serial communication error. */
        SerialPortClosed        ,       /*!< The serial port is closed. */
        ConfiqurationChanged    ,       /*!< The active port configuration changed. */
        FailedToOpenPort        ,       /*!< The serial port could not be opened. */
        SerialPortError                 /*!< An error was reported by QSerialPort. */
    };

public slots:
    /*!
        \brief Checks the current serial port state.

        Clears the last error message and emits the current open/closed state.
        Requires the serial port object to have been created.
    */
    void checkState(void);

    /*!
        \brief Sets the serial port name and baud rate.

        If the port is currently open, it is closed first and a state change
        signal is emitted. Then the new port configuration is stored and applied.

        \param portName The serial port name.
        \param baudRate The desired baud rate.
    */
    void setPortDetails(const QString &portName,uint32_t baudRate);

    /*!
        \brief Opens the serial port for reading and writing.

        Attempts to open the configured serial port and updates the current state.
    */
    void startCapture(void);

    /*!
        \brief Closes the serial port if it is open.

        Updates the current state after closing the port.
    */
    void stopCapture(void);

    /*!
     * \brief Queues raw bytes for transmission through the open serial port.
     * \param data Pointer to the bytes to enqueue; the bytes are copied into the queue.
     * \param length Number of bytes to enqueue.
    */
    void addRawDataToSend(const char *data, int length);

private:
    /*! \brief Declared receive-buffer capacity; currently unused by the implementation. */
    static constexpr size_t maxBufferSize = 1024;

    /*! \brief Serial port object used for the configured device connection. */
    QSerialPort *m_serialPort = nullptr;

    /*! \brief Reserved receive buffer; incoming bytes are currently emitted directly. */
    QByteArray m_rxBuffer;

    /*! \brief Name of the currently configured serial port. */
    QString m_portName;

    /*! \brief Baud rate configured for the serial port. */
    uint32_t m_baudRate;

    /*! \brief Most recent serial port error message. */
    QString m_lastError;

    /*! \brief FIFO queue of byte arrays waiting to be transmitted. */
    QQueue<QByteArray> m_txBuffer;

    /*! \brief Timer that schedules transmission from the pending byte queue. */
    QTimer *sendDataTimer = nullptr;

    /*!
     * \brief Creates the serial port object when needed or deletes it on request.
     * \param deleteIt If true, deletes the current serial port object; otherwise ensures
     *        the object exists.
     *
     */
    void checkSerialObjectavailable(bool deleteIt = false);

private slots:
    /*! \brief Reads available bytes from the serial port and emits them to the protocol layer. */
    void onReadyRead(void);

    /*! \brief Sends the next queued data chunk when the transmit timer fires. */
    void onSendDataTimerTimeout(void);

    /*!
     * \brief Handles errors reported by QSerialPort.
     * \param error Error code reported by the serial port.
     */
    void onErrorOccurred(QSerialPort::SerialPortError error);

signals:
    /*!
     * \brief Emitted when new bytes have been received from the serial port.
     * \param currentBuffer Newly received bytes.
     */
    void newDataReceived(const QByteArray &currentBuffer);

    /*!
     * \brief Emitted when the serial port state changes or an operation reports an error.
     * \param state true if the port is open; otherwise false.
     * \param hasError Error category associated with the state change.
     * \param errMessage Human-readable error details, if available.
     */
    void portStateChanged(bool state, SerialCommunicationErrors hasError = NoError, const QString &errMessage = QString(""));
};

#endif // SERIALCOMMUNICATION_H
