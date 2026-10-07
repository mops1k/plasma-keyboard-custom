/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <QDir>
#include <QFile>
#include <QTest>
#include <QVector>

#include <cmath>

#include "gigaamengine.h"
#include "parakeetengine.h"
#include "sttaudiorecorder.h"
#include "voskengine.h"
#include "whisperengine.h"

using namespace PlasmaKeyboardStt;

namespace
{
/**
 * Reads a 16 kHz mono 16 bit WAV file, the format the engines expect. Returns
 * an empty vector for anything else.
 */
QVector<float> readWav(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray data = file.readAll();
    if (data.size() < 44 || !data.startsWith("RIFF") || !data.mid(8, 4).contains("WAVE")) {
        return {};
    }

    int offset = 12;
    int channels = 0;
    int sampleRate = 0;
    QByteArray samples;
    while (offset + 8 <= data.size()) {
        const QByteArray id = data.mid(offset, 4);
        const quint32 size = *reinterpret_cast<const quint32 *>(data.constData() + offset + 4);
        const int payload = offset + 8;
        if (id == "fmt " && payload + 16 <= data.size()) {
            channels = *reinterpret_cast<const quint16 *>(data.constData() + payload + 2);
            sampleRate = *reinterpret_cast<const quint32 *>(data.constData() + payload + 4);
        } else if (id == "data") {
            samples = data.mid(payload, qMin<int>(size, data.size() - payload));
            break;
        }
        offset = payload + size + (size % 2);
    }

    if (channels != 1 || sampleRate != 16000 || samples.isEmpty()) {
        return {};
    }

    const int frames = samples.size() / 2;
    QVector<float> result(frames);
    const qint16 *raw = reinterpret_cast<const qint16 *>(samples.constData());
    for (int i = 0; i < frames; ++i) {
        result[i] = float(raw[i]) / 32768.0f;
    }
    return result;
}

//! The sample phrase and the models come from the environment; without them the
//! recognition tests are skipped, because the models are not in the repository.
QVector<float> sampleAudio()
{
    const QString wav = qEnvironmentVariable("PLASMA_KEYBOARD_STT_TEST_WAV");
    if (wav.isEmpty() || !QFile::exists(wav)) {
        return {};
    }
    return readWav(wav);
}

QString modelFromEnvironment(const char *variable)
{
    const QString model = qEnvironmentVariable(variable);
    return QFile::exists(model) ? model : QString();
}
} // namespace

/**
 * Checks the speech recognition engines. The models are not part of the
 * repository, so the recognition itself runs only when the environment points
 * at one; the engines and the recorder are still checked.
 */
class SttEngineTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void engineIdentity()
    {
        ParakeetEngine parakeet;
        QCOMPARE(parakeet.id(), QStringLiteral("parakeet"));
        QVERIFY(!parakeet.displayName().isEmpty());
        QVERIFY(parakeet.isAvailable());
        QVERIFY(!parakeet.hasModel());

        WhisperEngine whisper;
        QCOMPARE(whisper.id(), QStringLiteral("whisper"));
        QVERIFY(!whisper.displayName().isEmpty());
        QVERIFY(whisper.isAvailable());
        QVERIFY(!whisper.hasModel());

        // GigaAM is loaded from transcribe.cpp while the application runs, so
        // the engine may legitimately be unavailable in a test environment.
        GigaamEngine gigaam;
        QCOMPARE(gigaam.id(), QStringLiteral("gigaam"));
        QVERIFY(!gigaam.displayName().isEmpty());
        QVERIFY(!gigaam.hasModel());
    }

    void recorderSeesMicrophones()
    {
        // The list may be empty on a machine without a microphone; the call
        // itself must not fail.
        const QList<QAudioDevice> devices = SttAudioRecorder::inputDevices();
        qInfo() << "input devices:" << devices.size();
    }

    void parakeetTranscribes()
    {
        const QString model = modelFromEnvironment("PLASMA_KEYBOARD_STT_TEST_MODEL");
        if (model.isEmpty()) {
            QSKIP("PLASMA_KEYBOARD_STT_TEST_MODEL is not set to a Parakeet GGUF model");
        }
        const QVector<float> samples = sampleAudio();
        if (samples.isEmpty()) {
            QSKIP("PLASMA_KEYBOARD_STT_TEST_WAV is not set to a 16 kHz mono WAV file");
        }

        ParakeetEngine engine;
        QString error;
        QVERIFY2(engine.loadModel(model, &error), qPrintable(error));
        QVERIFY(engine.hasModel());

        // The v3 model is multilingual and takes no language hint.
        const QString text = engine.transcribe(samples, QString(), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        qInfo() << "parakeet recognised:" << text;
        QVERIFY(!text.trimmed().isEmpty());
    }

    void whisperTranscribes()
    {
        const QString model = modelFromEnvironment("PLASMA_KEYBOARD_STT_TEST_WHISPER_MODEL");
        if (model.isEmpty()) {
            QSKIP("PLASMA_KEYBOARD_STT_TEST_WHISPER_MODEL is not set to a Whisper GGML model");
        }
        const QVector<float> samples = sampleAudio();
        if (samples.isEmpty()) {
            QSKIP("PLASMA_KEYBOARD_STT_TEST_WAV is not set to a 16 kHz mono WAV file");
        }

        WhisperEngine engine;
        QString error;
        QVERIFY2(engine.loadModel(model, &error), qPrintable(error));
        QVERIFY(engine.hasModel());

        const QString text = engine.transcribe(samples, QStringLiteral("en"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        qInfo() << "whisper recognised:" << text;
        QVERIFY(!text.trimmed().isEmpty());
    }

    void voskTranscribes()
    {
        const QString model = qEnvironmentVariable("PLASMA_KEYBOARD_STT_TEST_VOSK_MODEL");
        if (model.isEmpty() || !QDir(model).exists()) {
            QSKIP("PLASMA_KEYBOARD_STT_TEST_VOSK_MODEL is not set to a Vosk model directory");
        }
        const QVector<float> samples = sampleAudio();
        if (samples.isEmpty()) {
            QSKIP("PLASMA_KEYBOARD_STT_TEST_WAV is not set to a 16 kHz mono WAV file");
        }

        VoskEngine engine;
        QString reason;
        if (!engine.isAvailable(&reason)) {
            QSKIP(qPrintable(reason));
        }

        QString error;
        QVERIFY2(engine.loadModel(model, &error), qPrintable(error));
        QVERIFY(engine.hasModel());

        // The model of Vosk is built for one language, so no hint is passed.
        const QString text = engine.transcribe(samples, QString(), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        qInfo() << "vosk recognised:" << text;
        QVERIFY(!text.trimmed().isEmpty());
    }

    void silenceTrimming()
    {
        constexpr int SampleRate = 16000;
        constexpr double Pi = 3.14159265358979323846;
        const int halfSecond = SampleRate / 2;

        // Half a second of silence, half a second of a tone, half a second of
        // silence: the shape a recording has when the microphone key is pressed
        // a moment before and after the phrase.
        QVector<float> samples(3 * halfSecond, 0.0f);
        for (int i = 0; i < halfSecond; ++i) {
            samples[halfSecond + i] = 0.5f * float(std::sin(2.0 * Pi * 440.0 * i / SampleRate));
        }

        const QVector<float> trimmed = SttAudioRecorder::trimSilence(samples);
        // The tone is kept, the silence around it is dropped except for the
        // margin of 100 ms on both sides.
        QCOMPARE(trimmed.size(), halfSecond + 2 * SampleRate / 10);

        // A recording that is silent from end to end has nothing to recognise.
        const QVector<float> silent(halfSecond, 0.0f);
        QVERIFY(SttAudioRecorder::trimSilence(silent).isEmpty());
        QVERIFY(SttAudioRecorder::trimSilence(QVector<float>()).isEmpty());
    }

    void gigaamTranscribes()
    {
        const QString model = modelFromEnvironment("PLASMA_KEYBOARD_STT_TEST_GIGAAM_MODEL");
        if (model.isEmpty()) {
            QSKIP("PLASMA_KEYBOARD_STT_TEST_GIGAAM_MODEL is not set to a GigaAM GGUF model");
        }
        const QVector<float> samples = sampleAudio();
        if (samples.isEmpty()) {
            QSKIP("PLASMA_KEYBOARD_STT_TEST_WAV is not set to a 16 kHz mono WAV file");
        }

        GigaamEngine engine;
        QString reason;
        if (!engine.isAvailable(&reason)) {
            QSKIP(qPrintable(reason));
        }

        QString error;
        QVERIFY2(engine.loadModel(model, &error), qPrintable(error));
        QVERIFY(engine.hasModel());

        // GigaAM v3 is a Russian model, so the language hint is not used.
        const QString text = engine.transcribe(samples, QStringLiteral("ru"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        qInfo() << "gigaam recognised:" << text;
        QVERIFY(!text.trimmed().isEmpty());
    }
};

QTEST_GUILESS_MAIN(SttEngineTest)

#include "sttenginestest.moc"
