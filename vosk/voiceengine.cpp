#include "voiceengine.h"

VoiceEngine::VoiceEngine(QObject *parent)
    : QObject(parent)
{
    fillAudioDevices();
}

VoiceEngine::~VoiceEngine()
{
    release();
}

void VoiceEngine::release()
{
    if (audio) {
        audio->stop();
        delete audio;
        audio = nullptr;
    }

    if (recognizer) {
        vosk_recognizer_free(recognizer);
        recognizer = nullptr;
    }

    if (model) {
        vosk_model_free(model);
        model = nullptr;
    }
}

void VoiceEngine::fillAudioDevices()
{
    devices = QMediaDevices::audioInputs();
}

// =====================================================================
//  INITIALIZE
// =====================================================================
bool VoiceEngine::initialize(const QString &modelPath)
{
    vosk_set_log_level(0);
    qDebug() << "[VOICE] Loading Vosk model:" << modelPath;

    model = vosk_model_new(modelPath.toUtf8());
    if (!model) {
        qDebug() << "[VOICE] ERROR: Failed to load Vosk model.";
        return false;
    }

    recognizer = vosk_recognizer_new(model, 16000.0);
    if (!recognizer) {
        qDebug() << "[VOICE] ERROR: Failed to create recognizer.";
        return false;
    }

    qDebug() << "[VOICE] Vosk initialized in FULL MODEL MODE.";
    return true;
}


// =====================================================================
//  START AUDIO CAPTURE
// =====================================================================
bool VoiceEngine::start(int deviceId)
{
    if (!model || !recognizer) {
        qDebug() << "[VOICE] ERROR: Engine not initialized.";
        return false;
    }

    QList<QAudioDevice> devices = QMediaDevices::audioInputs();
    QAudioDevice selected = QMediaDevices::defaultAudioInput();

    // if (!deviceId.isEmpty()) {
    //     for (const auto &dev : devices) {
    //         if (dev.description().contains(deviceId, Qt::CaseInsensitive)) {
    //             selected = dev;
    //             break;
    //         }
    //     }
    // }
    if(deviceId == -1)
    {
        selected = QMediaDevices::defaultAudioInput();
    }

    qDebug() << "[VOICE] Using input device:" << selected.description();

    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    audio = new QAudioSource(selected, format, this);
    audioIO = audio->start();
    if (!audioIO) {
        qDebug() << "[VOICE] ERROR: Failed to start audio input.";
        return false;
    }

    connect(audioIO, &QIODevice::readyRead, this, &VoiceEngine::processAudio);

    running = true;
    qDebug() << "[VOICE] Audio capture started.";
    return true;
}


// =====================================================================
//  PROCESS AUDIO
// =====================================================================
void VoiceEngine::processAudio()
{
    if (!running || !audioIO)
        return;

    QByteArray data = audioIO->readAll();
    if (data.isEmpty())
        return;

    int ok = vosk_recognizer_accept_waveform(recognizer,
                                             data.constData(),
                                             data.size());

    if (ok) {
        const char *res = vosk_recognizer_result(recognizer);
        emit finalResult(QString::fromUtf8(res));
    } else {
        const char *partial = vosk_recognizer_partial_result(recognizer);
        emit partialResult(QString::fromUtf8(partial));
    }
}


// =====================================================================
//  STOP AUDIO
// =====================================================================
void VoiceEngine::stop()
{
    running = false;

    if (audio) {
        audio->stop();
        delete audio;
        audio = nullptr;
    }

    qDebug() << "[VOICE] Audio stopped.";
}


// =====================================================================
//  GRAMMAR CONTROL
// =====================================================================
void VoiceEngine::enableGrammar(const QStringList &words)
{
    if (!recognizer) return;

    QString jsonList = "[";
    for (int i = 0; i < words.size(); ++i) {
        jsonList += "\"" + words[i] + "\"";
        if (i < words.size() - 1) jsonList += ",";
    }
    jsonList += "]";

    vosk_recognizer_set_grm(recognizer, jsonList.toUtf8());

    qDebug() << "[VOICE] Grammar ENABLED:" << jsonList;
}

void VoiceEngine::disableGrammar()
{
    if (!recognizer) return;

    vosk_recognizer_set_grm(recognizer, "[]");

    qDebug() << "[VOICE] Grammar DISABLED → FULL MODEL restored.";
}

QList<QAudioDevice> *VoiceEngine::getAudioDevices()
{
    return &devices;
}
