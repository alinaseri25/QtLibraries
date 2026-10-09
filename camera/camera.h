#ifndef CAMERA_H
#define CAMERA_H

#include <QObject>
#include <QTimer>
#include <QImage>
#include <QMediaDevices>
#include <QCameraDevice>
#include <QCamera>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QMediaPlayer>
#include <QThread>
#include <QFileInfo>

#include <QVariantList>
#include <QVariantMap>

/*!
 * \brief Describes the image planes and layout of a captured video frame.
 *
 * Plane pointers are populated from the current QVideoFrame and remain tied to
 * that frame's backing storage.
 */
struct CameraFrame
{
    /*! \brief Frame width in pixels. */
    int width;
    /*! \brief Frame height in pixels. */
    int height;

    /*! \brief Pointers to up to three image planes. */
    uint8_t* plane[3];
    /*! \brief Row stride in bytes for each corresponding image plane. */
    int stride[3];

    /*! \brief Guard flag used to skip processing while a consumer holds this frame. */
    bool inProgress = false;

    /*! \brief Pixel format reported by the captured video frame. */
    QVideoFrameFormat::PixelFormat format;
};

/*!
 * \brief Camera and media stream lifecycle states reported by Camera.
 */
typedef enum{
    Starting                = 0x01, /*!< A camera or media stream is being prepared. */
    Running                 ,       /*!< The selected camera or media stream is active. */
    Stopping                ,       /*!< The active camera or media stream is stopping. */
    Stopped                 ,       /*!< No camera or media stream is active. */
    Error                           /*!< The camera or media stream reported an error. */
}CameraState;

/*!
 * \brief Selects a local camera or media URL and exposes its video frames.
 *
 * Camera uses Qt Multimedia to control camera devices or play a media URL, then
 * forwards frames to a QVideoSink and reports status and frame-rate information.
 */
class Camera : public QObject
{
    Q_OBJECT
public:
    /*!
     * \brief Creates the camera controller and discovers available video inputs.
     * \param parent Optional QObject parent.
     */
    explicit Camera(QObject *parent = nullptr);

    /*! \brief Stops active capture or playback before the controller is destroyed. */
    ~Camera(void);

public slots:
    /*! \brief Stops the active camera or media stream and releases its resources. */
    void stopCamera(void);

    /*! \brief Refreshes and emits the list of available local cameras. */
    void camerListRequest(void);

    /*!
     * \brief Selects a local camera by its index and prepares it for capture.
     * \param _camera Index in the discovered camera list; invalid values select index 0.
     * \param _autoReconnect Whether to retry selection after a media error.
     */
    void cameraSelected(int _camera, bool _autoReconnect = false);

    /*!
     * \brief Selects a network stream or media file URL for playback.
     * \param url Network URL or local media file path.
     * \param _autoReconnect Whether to retry playback after a media error.
     */
    void cameraSelected(const QString &url, bool _autoReconnect = false);

    /*!
     * \brief Sets the video sink that receives frames from the active source.
     * \param _sink Video sink to receive captured or decoded frames; nullptr creates an internal sink.
     */
    void setVideoSink(QVideoSink *_sink = nullptr);

    /*! \brief Starts the selected camera or media source when its state is Starting. */
    void startCamera(void);

    /*! \brief Pauses the active camera or media source. */
    void pauseCamera(void);

private:
    /*! \brief Camera devices discovered through QMediaDevices. */
    QList<QCameraDevice> cameras;

    /*! \brief Selected camera index, current measured FPS, and frame count for the current interval. */
    int selectedCamera = -1,FPS = 0,FPSCounter = 0;

    /*! \brief Qt camera object used for local video capture, if selected. */
    QCamera *camera = nullptr;

    /*! \brief Multimedia session connecting the camera source to the video sink. */
    QMediaCaptureSession captureSession;

    /*! \brief Sink that receives video frames from the selected source. */
    QVideoSink *videoSink = nullptr;

    /*! \brief Media player used to play a network stream or local media file. */
    QMediaPlayer *mediaPlayer = nullptr;

    /*! \brief Timer that periodically publishes the measured frame rate. */
    QTimer *frameCounter = nullptr;

    /*! \brief Reserved image storage; the current implementation does not assign to it. */
    QImage lastFrame;

    /*! \brief URL of the selected network stream or local media file. */
    QUrl mediaPlayerUrl = QUrl(QString(""));

    /*! \brief Current lifecycle state of the camera or media source. */
    CameraState cameraState;

    /*! \brief Whether camera or media errors should trigger a reconnection attempt. */
    bool autoReconnect = false;

    /*! \brief Working video frame used while reading pixel data from the sink. */
    QVideoFrame f;

    /*! \brief Reusable metadata and plane pointers for the most recently processed frame. */
    CameraFrame cameraFrame{};

    /*! \brief Refreshes the cached list of local camera devices. */
    void fillCameraList(void);

    /*! \brief Creates and configures a QCamera for the currently selected device. */
    void selectCamera(void);

    /*!
     * \brief Updates the source lifecycle state and emits its status.
     * \param _state New camera or media lifecycle state.
     * \param _msg Optional human-readable status or error message.
     */
    void setCameraState(CameraState _state,const QString &_msg = QString(""));

private slots:
    /*! \brief Processes a video frame delivered by the active QVideoSink. */
    void processFrame(const QVideoFrame &frame);

    /*! \brief Publishes the measured frame rate when the sampling interval expires. */
    void onFrameCounterTimeout(void);

    /*!
     * \brief Handles a QMediaPlayer playback error.
     * \param error Qt Multimedia error code.
     * \param errorString Human-readable error details.
     */
    void onError(QMediaPlayer::Error error, const QString &errorString);

    /*!
     * \brief Handles changes to the media player's loading and playback status.
     * \param status New media status.
     */
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);

signals:
    /*!
     * \brief Emitted after a camera list request completes.
     * \param _cameras Camera entries containing description, id and position values.
     */
    void cameraListResponse(const QVariantList &_cameras);

    /*!
     * \brief Emitted when a video frame is available for a consumer.
     * \param _frame Pointer to the controller's reusable frame metadata.
     */
    void newFrameRecieved(CameraFrame *_frame);

    /*!
     * \brief Emitted when a new frames-per-second sample is available.
     * \param _FPS Measured frame rate for the previous sampling interval.
     */
    void reportFrameRate(int _FPS);

    /*!
     * \brief Emitted when the camera or media source changes state.
     * \param _state New lifecycle state.
     * \param _description Human-readable state or error description.
     */
    void cameraStatusChanged(CameraState _state,const QString &_description);
};

#endif // CAMERA_H
