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

#include <QVariantList>
#include <QVariantMap>

struct CameraFrame
{
    int width;
    int height;

    uint8_t* plane[3];
    int stride[3];

    bool inProgress = false;

    QVideoFrameFormat::PixelFormat format;
};

typedef enum{
    Starting                = 0x01,
    Running                 ,
    Stopping                ,
    Stopped                 ,
    Error
}CameraState;

class Camera : public QObject
{
    Q_OBJECT
public:
    explicit Camera(QObject *parent = nullptr);
    ~Camera(void);

public slots:
    void stopCamera(void);
    void camerListRequest(void);
    void cameraSelected(int _camera, bool _autoReconnect = false);
    void cameraSelected(const QUrl &url, bool _autoReconnect = false);
    void setVideoSink(QVideoSink *_sink = nullptr);
    void startCamera(void);
    void pauseCamera(void);

private:
    QList<QCameraDevice> cameras;
    int selectedCamera = -1,FPS = 0,FPSCounter = 0;
    QCamera *camera = nullptr;
    QMediaCaptureSession captureSession;
    QVideoSink *videoSink = nullptr;
    QMediaPlayer *mediaPlayer = nullptr;
    QTimer *frameCounter = nullptr;
    QImage lastFrame;
    QUrl mediaPlayerUrl = QUrl(QString(""));
    CameraState cameraState;
    bool autoReconnect = false;

    QVideoFrame f;
    CameraFrame cameraFrame{};

    void fillCameraList(void);
    void selectCamera(void);
    void setCameraState(CameraState _state,const QString &_msg = QString(""));

private slots:
    void processFrame(const QVideoFrame &frame);
    void onFrameCounterTimeout(void);
    void onError(QMediaPlayer::Error error, const QString &errorString);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);

signals:
    void cameraListResponse(const QVariantList &_cameras);
    void newFrameRecieved(CameraFrame *_frame);
    void reportFrameRate(int _FPS);
    void cameraStatusChanged(CameraState _state,const QString &_description);
};

#endif // CAMERA_H
