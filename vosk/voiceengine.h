#ifndef VOICEENGINE_H
#define VOICEENGINE_H

#include <QObject>
#include <QtMultimedia>
#include <QAudioSource>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QThread>
#include <QByteArray>
#include <QDebug>
#include <QStringList>

#include <vosk_api.h>

class VoiceEngine : public QObject
{
    Q_OBJECT

public:
    explicit VoiceEngine(QObject *parent = nullptr);
    ~VoiceEngine();

    bool initialize(const QString &modelPath);
    bool start(int deviceId = -1);

    void stop();
    void enableGrammar(const QStringList &words);
    void disableGrammar();

    QList<QAudioDevice> *getAudioDevices(void);

signals:
    void finalResult(QString text);
    void partialResult(QString text);

private slots:
    void processAudio();

private:
    void release();
    void fillAudioDevices(void);

    VoskModel *model = nullptr;
    VoskRecognizer *recognizer = nullptr;

    QAudioSource *audio = nullptr;
    QIODevice *audioIO = nullptr;

    QList<QAudioDevice> devices;

    QAudioFormat format;
    bool running = false;
};

#endif // VOICEENGINE_H
