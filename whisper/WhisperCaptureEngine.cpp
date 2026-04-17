#include "../../QtLibraries/whisper/WhisperCaptureEngine.h"
#include <QDebug>
#include <cmath>

WhisperStreamingEngine::WhisperStreamingEngine(QObject *parent)
    : QObject(parent)
{
    moveToThread(&worker);
    worker.start();

    inferenceTimer = new QTimer();
    inferenceTimer->setInterval(200); // 5Hz update = latency زیر 200ms
    inferenceTimer->moveToThread(&worker);

    connect(inferenceTimer, &QTimer::timeout, this, &WhisperStreamingEngine::runInferenceWindow);
}

WhisperStreamingEngine::~WhisperStreamingEngine()
{
    worker.quit();
    worker.wait();

    if (ctx)
        whisper_free(ctx);
}

void WhisperStreamingEngine::setState(EngineState st)
{
    if (state == st) return;
    state = st;
    qDebug() << "current state : " << st;
    emit stateChanged(st);
}

//
// List audio devices
//
void WhisperStreamingEngine::requestDeviceList()
{
    QStringList ids;
    audioDevices = QMediaDevices::audioInputs();
    for (auto &d : audioDevices)
        ids << d.id();

    emit availableDevices(ids);
}

//
// Select microphone
//
void WhisperStreamingEngine::selectDevice(const int &id)
{
    deviceId = id;

    if(deviceId == -1)
    {
        selectedDevice = QMediaDevices::defaultAudioInput();
    }
    else
    {
        for (auto &d : audioDevices) {
            if (d.id() == id) {
                selectedDevice = d;
                break;
            }
        }
    }
    qDebug() << QString("Selected Audeio Device : %1").arg(selectedDevice.description());
}

//
// Load Whisper model (float API)
//
void WhisperStreamingEngine::loadModel(const QString &path)
{
    setState(EngineState::LoadingModel);

    whisper_context_params cparams = whisper_context_default_params();
    ctx = whisper_init_from_file_with_params(path.toStdString().c_str(), cparams);

    if (!ctx) {
        emit errorOccurred("Failed to load whisper model");
        setState(EngineState::Error);
        return;
    }

    // allocate ring buffer
    ring.resize(ringMaxSamples);
    ring.fill(0.0f);

    setState(EngineState::Ready);
    qDebug() << QString("Model Loaded from : %1").arg(path);
}

//
// Start streaming mode
//
void WhisperStreamingEngine::start()
{
    if (state != EngineState::Ready) {
        emit errorOccurred("Engine not ready");
        return;
    }

    if (selectedDevice.isNull()) {
        emit errorOccurred("No audio device selected");
        return;
    }

    QAudioFormat fmt;
    fmt.setSampleRate(16000);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);

    // start audio properly according to Qt 6.10
    audio = new QAudioSource(selectedDevice, fmt, this);

    audioIO = audio->start();
    if (!audioIO) {
        emit errorOccurred("Failed to start audio");
        setState(EngineState::Error);
        return;
    }

    connect(audioIO, &QIODevice::readyRead, this, &WhisperStreamingEngine::processAudio);

    inferenceTimer->start();
    setState(EngineState::Running);

    qDebug() << "Volume before:" << audio->volume();
    audio->setVolume(1.0f);
    qDebug() << "Volume after:" << audio->volume();
}

//
// Stop
//
void WhisperStreamingEngine::stop()
{
    inferenceTimer->stop();

    if (audio) {
        audio->stop();
        audio->deleteLater();
        audio = nullptr;
    }

    ringWritePos = 0;
    ringFilled = false;
    lastStableText.clear();
    lastIterationText.clear();

    setState(EngineState::Idle);
}

//
// Process microphone audio (float)
//
void WhisperStreamingEngine::processAudio()
{
    QByteArray data = audioIO->readAll();
    if (data.isEmpty())
        return;

    int numSamples = data.size() / sizeof(int16_t);
    if (numSamples <= 0)
        return;

    const int16_t *src = reinterpret_cast<const int16_t*>(data.constData());

    for (int i = 0; i < numSamples; ++i) {
        float s = static_cast<float>(src[i] * 10) / 32768.0f;
        //qDebug() << QString("s: %1 -- src[i]: %2").arg(s).arg(src[i]);

        ring[ringWritePos] = s;
        //qDebug() << QString("Whisper Data: %1").arg(floatBuffer[i]);

        ringWritePos++;
        if (ringWritePos >= ringMaxSamples) {
            ringWritePos = 0;
            ringFilled = true;
        }
    }

    samplesSinceLastInference += numSamples;
}

//
// Silence detection
//
void WhisperStreamingEngine::applySilenceDetection(const QVector<float> &samples)
{
    float avg = 0;
    for (float s : samples)
        avg += std::fabs(s);

    avg /= samples.size();

    qDebug() << "Average amplitude:" << avg;

    if (avg < silenceThreshold)
        silentFrames++;
    else
        silentFrames = 0;
}

//
// Decide if segment is stable
//
bool WhisperStreamingEngine::segmentIsStable(const QString &newText)
{
    if (newText == lastIterationText) {
        lastStableText = newText;
        return true;
    }

    lastIterationText = newText;
    return false;
}

//
// Inference Sliding Window
//
void WhisperStreamingEngine::runInferenceWindow()
{
    if (state != EngineState::Running)
        return;

    if (samplesSinceLastInference < stepSamples)
        return;

    samplesSinceLastInference = 0;

    // extract window
    QVector<float> win(windowSamples);

    int start = ringWritePos - windowSamples;
    if (start < 0) start += ringMaxSamples;

    for (int i = 0; i < windowSamples; i++) {
        int idx = (start + i) % ringMaxSamples;
        win[i] = ring[idx];
    }

    applySilenceDetection(win);

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_timestamps = false;
    params.print_special = false;
    params.no_context = true;

    int ret = whisper_full(ctx, params, win.data(), win.size());
    if (ret != 0) {
        emit errorOccurred("Whisper inference failed");
        return;
    }

    int n = whisper_full_n_segments(ctx);
    if (n == 0) return;

    QString text = QString::fromUtf8(whisper_full_get_segment_text(ctx, n - 1)).trimmed();
    if (text.isEmpty()) return;

    if (segmentIsStable(text))
        emit finalSentence(text);
}
