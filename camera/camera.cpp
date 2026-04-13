#include "camera.h"

Camera::Camera(QObject *parent)
    : QObject{parent}
{
    selectedCamera = 0;
    fillCameraList();
}

Camera::~Camera()
{
    stopCamera();
}

void Camera::setVideoSink(QVideoSink *_sink)
{
    if(videoSink != nullptr)
    {
        disconnect(videoSink, &QVideoSink::videoFrameChanged,this, &Camera::processFrame);
        if(_sink != videoSink)
        {
            videoSink->deleteLater();
            videoSink = nullptr;
        }
    }
    if(_sink ==  nullptr)
    {
        _sink = new QVideoSink(nullptr);
    }
    videoSink = _sink;
    connect(videoSink, &QVideoSink::videoFrameChanged,this, &Camera::processFrame);
}

void Camera::startCamera()
{
    if(cameraState != Starting)
    {
        return;
    }
    if(selectedCamera != -1)
    {
        camera->start();
        setCameraState(CameraState::Running);
    }
    else if(mediaPlayerUrl != QUrl(QString("")))
    {
        mediaPlayer->play();
        setCameraState(CameraState::Running);
    }
    cameraFrame.inProgress = false;
}

void Camera::pauseCamera()
{
    if(cameraState != Running)
    {
        return;
    }
    if(selectedCamera != -1)
    {
        camera->stop();
        setCameraState(CameraState::Starting);
    }
    else if(mediaPlayerUrl != QUrl(QString("")))
    {
        mediaPlayer->pause();
        setCameraState(CameraState::Starting);
    }
}

void Camera::stopCamera()
{
    setCameraState(Stopping);
    if(frameCounter != nullptr)
    {
        frameCounter->stop();
        frameCounter->deleteLater();
        frameCounter = nullptr;
    }
    if(camera != nullptr)
    {
        camera->stop();
        QThread::msleep(500);
        camera->deleteLater();
        camera = nullptr;
    }
    if(mediaPlayer != nullptr)
    {
        mediaPlayer->stop();
        QThread::msleep(500);
        mediaPlayer->deleteLater();
        mediaPlayer = nullptr;
    }
    setCameraState(CameraState::Stopped);
    QThread::msleep(500);
}

void Camera::camerListRequest()
{
    fillCameraList();
    QVariantList list;
    // qDebug() << "Camera List : ";
    for (const QCameraDevice &cameraDevice : cameras) {
        QVariantMap item;
        item["description"] = cameraDevice.description();
        item["id"] = cameraDevice.id();
        item["position"] = cameraDevice.position();

        // qDebug() << cameraDevice.description() << " : " << cameraDevice.photoResolutions() ;
        // for (const QCameraFormat &format : cameraDevice.videoFormats()) {
        //     qDebug() << "FPS: " << format.maxFrameRate() << " Height: " << format.resolution().height() << " width: " << format.resolution().width();
        // }
        // qDebug() << "================================================";

        list.append(item);
    }
    emit cameraListResponse(list);
}

void Camera::cameraSelected(const QUrl &url, bool _autoReconnect)
{
    if(cameraState == Stopping)
    {
        return;
    }
    if(cameraState == Running)
    {
        pauseCamera();
    }
    autoReconnect = _autoReconnect;
    mediaPlayerUrl = url;
    selectedCamera = -1;
    stopCamera();
    setCameraState(CameraState::Starting);

    if(videoSink == nullptr)
    {
        setVideoSink(new QVideoSink(nullptr));
    }

#ifdef Q_OS_ANDROID
    mediaPlayer = new QMediaPlayer(nullptr);
    mediaPlayer->setVideoSink(videoSink);
    mediaPlayer->setSource(mediaPlayerUrl);
    connect(mediaPlayer,&QMediaPlayer::errorOccurred,this,&Camera::onError);
    connect(mediaPlayer,&QMediaPlayer::mediaStatusChanged,this,&Camera::onMediaStatusChanged);
#else
    mediaPlayer = new QMediaPlayer(nullptr);
    mediaPlayer->setVideoSink(videoSink);
    mediaPlayer->setSource(mediaPlayerUrl);
    connect(mediaPlayer,&QMediaPlayer::errorOccurred,this,&Camera::onError);
    connect(mediaPlayer,&QMediaPlayer::mediaStatusChanged,this,&Camera::onMediaStatusChanged);
#endif
    if(frameCounter == nullptr)
    {
        frameCounter = new QTimer(nullptr);
        connect(frameCounter,&QTimer::timeout,this,&Camera::onFrameCounterTimeout);
        frameCounter->setInterval(1000);
    }
    frameCounter->start();
}

void Camera::cameraSelected(int _camera, bool _autoReconnect)
{
    if(cameraState == Stopping)
    {
        return;
    }
    if(cameraState == Running)
    {
        pauseCamera();
    }
    autoReconnect = _autoReconnect;
    stopCamera();
    setCameraState(CameraState::Starting);
    if(videoSink == nullptr)
    {
        setVideoSink(new QVideoSink(nullptr));
    }

    if(cameras.size() < _camera)
    {
        _camera = 0;
    }
    selectedCamera = _camera;
    mediaPlayerUrl = QUrl(QString(""));
    selectCamera();
    if(frameCounter == nullptr)
    {
        frameCounter = new QTimer(nullptr);
        connect(frameCounter,&QTimer::timeout,this,&Camera::onFrameCounterTimeout);
        frameCounter->setInterval(1000);
    }
    frameCounter->start();
}

void Camera::fillCameraList()
{
    cameras.clear();
    cameras = QMediaDevices::videoInputs();
}

void Camera::selectCamera()
{
    QCameraDevice device = cameras[selectedCamera];

    camera = new QCamera(device);

    QList<QCameraFormat> formats = device.videoFormats();
    QCameraFormat bestFormat;
    int maxResolution = 0;

    for (const QCameraFormat &format : formats) {
        QSize resolution = format.resolution();
        int pixels = resolution.width() * resolution.height();

        // فقط فرمت‌های 30fps یا بالاتر
        if (format.maxFrameRate() >= 30 && pixels > maxResolution) {
            maxResolution = pixels;
            bestFormat = format;
        }
    }

    if (bestFormat.isNull() == false) {
        camera->setCameraFormat(bestFormat);
    }

    captureSession.setCamera(camera);
    captureSession.setVideoSink(videoSink);
}

void Camera::setCameraState(CameraState _state, const QString &_msg)
{
    cameraState = _state;
    QString errMessage = _msg;
    switch (cameraState) {
    case CameraState::Error:
        errMessage = _msg;
        break;
    default:
        break;
    }
    emit cameraStatusChanged(cameraState,errMessage);
}

void Camera::processFrame(const QVideoFrame &frame)
{
    FPSCounter++;

    if(cameraFrame.inProgress)
    {
        return;
    }
    f = frame;

    cameraFrame.width  = f.width();
    cameraFrame.height = f.height();
    cameraFrame.format = f.pixelFormat();
    if (f.map(QVideoFrame::ReadOnly))
    {
        int planes = f.planeCount();

        for(int i = 0; i < planes && i < 3; i++)
        {
            cameraFrame.plane[i]  = f.bits(i);
            cameraFrame.stride[i] = f.bytesPerLine(i);
        }

        emit newFrameRecieved(&cameraFrame);
        return;
    }
    else
    {
        cameraFrame.inProgress = false;
        qWarning() << QString("Cannot read this frame Type (") << f.pixelFormat() << QString(")!");
        return;
    }
}

void Camera::onFrameCounterTimeout()
{
    if(FPS == FPSCounter)
    {
        FPSCounter = 0;
        return;
    }
    FPS = FPSCounter;
    emit reportFrameRate(FPS);
    FPSCounter = 0;
}

void Camera::onError(QMediaPlayer::Error error, const QString &errorString)
{
    setCameraState(CameraState::Error,errorString);
    if(autoReconnect)
    {
        if(selectedCamera != -1)
        {
            cameraSelected(selectedCamera,autoReconnect);
        }
        else if(mediaPlayerUrl != QUrl(QString("")))
        {
            cameraSelected(mediaPlayerUrl,autoReconnect);
        }
    }
    else
    {
        stopCamera();
    }
}

void Camera::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if(status == QMediaPlayer::InvalidMedia)
    {
        setCameraState(CameraState::Error,QString("Invalid Media"));
        if(autoReconnect)
        {
            if(selectedCamera != -1)
            {
                cameraSelected(selectedCamera,autoReconnect);
            }
            else if(mediaPlayerUrl != QUrl(QString("")))
            {
                cameraSelected(mediaPlayerUrl,autoReconnect);
            }
        }
        else
        {
            stopCamera();
        }
    }
}
