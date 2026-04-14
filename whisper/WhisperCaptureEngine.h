#ifndef WHISPER_STREAMING_ENGINE_H
#define WHISPER_STREAMING_ENGINE_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QAudioSource>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QVector>
#include <QByteArray>

#include "whisper.h"

class WhisperStreamingEngine : public QObject
{
    Q_OBJECT

public:
    explicit WhisperStreamingEngine(QObject *parent = nullptr);
    ~WhisperStreamingEngine();

    enum class EngineState {
        Idle,
        LoadingModel,
        Ready,
        Running,
        Error
    };
    Q_ENUM(EngineState)

signals:
    void stateChanged(EngineState state);
    void finalSentence(const QString &sentence);
    void errorOccurred(const QString &error);
    void availableDevices(const QStringList &devices);

public slots:
    void requestDeviceList();
    void selectDevice(const int &id);
    void loadModel(const QString &path);
    void start();
    void stop();

private:
    void setState(EngineState);
    void processAudio();
    void runInferenceWindow();
    void applySilenceDetection(const QVector<float> &samples);
    bool segmentIsStable(const QString &newText);

private:
    QThread worker;

    // Whisper
    whisper_context *ctx = nullptr;

    // Qt Audio
    QAudioSource *audio = nullptr;
    QIODevice *audioIO = nullptr;
    QAudioDevice selectedDevice;
    int deviceId;
    QList<QAudioDevice> audioDevices;

    // Audio Ring Buffer (float32, 16000Hz, mono)
    QVector<float> ring;
    int ringMaxSamples = 16000 * 30;  // 30 seconds
    int ringWritePos = 0;
    bool ringFilled = false;

    // inference window
    int windowSamples = 16000 * 10;   // last 10 seconds
    int stepSamples   = 16000 * 1;    // run inference every 1 sec

    int samplesSinceLastInference = 0;

    // stabilization
    QString lastStableText;
    QString lastIterationText;

    // silence detection
    float silenceThreshold = 0.015f;
    int silentFrames = 0;
    int silenceFramesRequired = 1600 * 2; // 2 seconds of silence

    EngineState state = EngineState::Idle;

    QTimer *inferenceTimer = nullptr;
};

#endif // WHISPER_STREAMING_ENGINE_H
