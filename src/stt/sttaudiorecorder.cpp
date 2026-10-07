/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "sttaudiorecorder.h"

#include <KLocalizedString>
#include <QAudioDevice>
#include <QAudioSource>
#include <QIODevice>
#include <QLoggingCategory>
#include <QMediaDevices>

#include <algorithm>
#include <cmath>
#include <cstring>

Q_LOGGING_CATEGORY(lcSttAudio, "org.kde.plasma.keyboard.custom.stt.audio")

namespace PlasmaKeyboardStt
{

namespace
{
constexpr int TargetSampleRate = 16000;
constexpr int TargetChannels = 1;
} // namespace

SttAudioRecorder::SttAudioRecorder(QObject *parent)
    : QObject(parent)
{
}

SttAudioRecorder::~SttAudioRecorder()
{
    cancel();
}

QList<QAudioDevice> SttAudioRecorder::inputDevices()
{
    return QMediaDevices::audioInputs();
}

bool SttAudioRecorder::start(const QString &deviceId, QString *error)
{
    if (m_source) {
        cancel();
    }

    QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (!deviceId.isEmpty()) {
        const QList<QAudioDevice> devices = QMediaDevices::audioInputs();
        for (const QAudioDevice &candidate : devices) {
            if (candidate.id() == deviceId.toUtf8()) {
                device = candidate;
                break;
            }
        }
    }
    if (device.isNull()) {
        if (error) {
            *error = i18nd("plasma-keyboard-custom", "There is no microphone available for the speech recognition.");
        }
        return false;
    }

    // Ask for exactly what the engines want first; a device that cannot do it is
    // recorded in its own format and converted afterwards.
    QAudioFormat format;
    format.setSampleRate(TargetSampleRate);
    format.setChannelCount(TargetChannels);
    format.setSampleFormat(QAudioFormat::Float);
    if (!device.isFormatSupported(format)) {
        format.setSampleFormat(QAudioFormat::Int16);
    }
    if (!device.isFormatSupported(format)) {
        format = device.preferredFormat();
        qCDebug(lcSttAudio) << "the device does not record at 16 kHz, using" << format;
    }

    m_format = format;
    m_buffer.clear();
    m_level = 0;
    m_limitReported = false;

    m_source = new QAudioSource(device, format, this);
    m_io = m_source->start();
    if (!m_io) {
        if (error) {
            *error = i18nd("plasma-keyboard-custom", "Cannot start recording from “%1”.").arg(device.description());
        }
        m_source->deleteLater();
        m_source = nullptr;
        return false;
    }
    connect(m_io, &QIODevice::readyRead, this, &SttAudioRecorder::handleReadyRead);

    qCDebug(lcSttAudio) << "recording started from" << device.description() << "format" << format;
    Q_EMIT recordingChanged(true);
    return true;
}

void SttAudioRecorder::handleReadyRead()
{
    if (!m_io) {
        return;
    }
    const QByteArray chunk = m_io->readAll();
    if (chunk.isEmpty()) {
        return;
    }
    m_buffer += chunk;

    const int bytesPerFrame = qMax(1, m_format.bytesPerFrame());
    const int frames = chunk.size() / bytesPerFrame;
    float peak = 0;
    for (int i = 0; i < frames; ++i) {
        peak = std::max(peak, std::abs(sampleAt(chunk.constData() + i * bytesPerFrame)));
    }
    m_level = peak;
    Q_EMIT levelChanged(m_level);

    if (!m_limitReported && recordedMs() >= MaximumRecordingMs) {
        m_limitReported = true;
        Q_EMIT errorOccurred(i18nd("plasma-keyboard-custom", "The recording was stopped: it is limited to %1 seconds.").arg(MaximumRecordingMs / 1000));
        // The manager listens to this signal and stops the recording itself.
        m_level = 0;
    }
}

QVector<float> SttAudioRecorder::stop()
{
    if (!m_source) {
        return QVector<float>();
    }

    handleReadyRead();
    m_source->stop();
    if (m_io) {
        disconnect(m_io, &QIODevice::readyRead, this, &SttAudioRecorder::handleReadyRead);
        m_io = nullptr;
    }
    m_source->deleteLater();
    m_source = nullptr;
    m_level = 0;

    const QVector<float> recorded = toMono16k();
    const QVector<float> samples = trimSilence(recorded);
    qCDebug(lcSttAudio) << "recording stopped," << recorded.size() << "samples (" << (TargetSampleRate > 0 ? recorded.size() * 1000 / TargetSampleRate : 0)
                        << "ms)," << samples.size() << "samples after trimming the silence ("
                        << (TargetSampleRate > 0 ? samples.size() * 1000 / TargetSampleRate : 0) << "ms)";
    Q_EMIT recordingChanged(false);
    Q_EMIT levelChanged(0);
    return samples;
}

void SttAudioRecorder::cancel()
{
    if (!m_source) {
        m_buffer.clear();
        return;
    }
    m_source->stop();
    if (m_io) {
        disconnect(m_io, &QIODevice::readyRead, this, &SttAudioRecorder::handleReadyRead);
        m_io = nullptr;
    }
    m_source->deleteLater();
    m_source = nullptr;
    m_buffer.clear();
    m_level = 0;
    Q_EMIT recordingChanged(false);
    Q_EMIT levelChanged(0);
}

bool SttAudioRecorder::isRecording() const
{
    return m_source != nullptr;
}

qreal SttAudioRecorder::level() const
{
    return m_level;
}

int SttAudioRecorder::recordedMs() const
{
    if (m_format.sampleRate() <= 0) {
        return 0;
    }
    const int frames = m_buffer.size() / qMax(1, m_format.bytesPerFrame());
    return int(qint64(frames) * 1000 / m_format.sampleRate());
}

float SttAudioRecorder::sampleAt(const char *pointer) const
{
    switch (m_format.sampleFormat()) {
    case QAudioFormat::UInt8: {
        quint8 value = 0;
        std::memcpy(&value, pointer, sizeof(value));
        return (float(value) - 128.0f) / 128.0f;
    }
    case QAudioFormat::Int16: {
        qint16 value = 0;
        std::memcpy(&value, pointer, sizeof(value));
        return float(value) / 32768.0f;
    }
    case QAudioFormat::Int32: {
        qint32 value = 0;
        std::memcpy(&value, pointer, sizeof(value));
        return float(double(value) / 2147483648.0);
    }
    case QAudioFormat::Float: {
        float value = 0;
        std::memcpy(&value, pointer, sizeof(value));
        return value;
    }
    default:
        return 0.0f;
    }
}

QVector<float> SttAudioRecorder::trimSilence(const QVector<float> &samples)
{
    // The windows the level is measured in, the silence kept before the first
    // and after the last sound (so the sound itself is never cut), and how
    // loud a window has to be to count as sound: a sixteenth of the loudest
    // window (-24 dB) passes quiet speech in a noisy room while the noise
    // floor of the microphone stays below it.
    constexpr int WindowMs = 20;
    constexpr int MarginMs = 100;
    constexpr float ThresholdRatio = 1.0f / 16.0f;

    const int windowSize = TargetSampleRate * WindowMs / 1000;
    if (samples.isEmpty() || windowSize <= 0) {
        return samples;
    }

    const int windowCount = (samples.size() + windowSize - 1) / windowSize;
    QVector<float> levels(windowCount);
    float peak = 0.0f;
    for (int window = 0; window < windowCount; ++window) {
        const int begin = window * windowSize;
        const int end = qMin(begin + windowSize, samples.size());
        double sum = 0.0;
        for (int i = begin; i < end; ++i) {
            sum += double(samples.at(i)) * double(samples.at(i));
        }
        const float rms = float(std::sqrt(sum / qMax(1, end - begin)));
        levels[window] = rms;
        peak = qMax(peak, rms);
    }

    if (peak <= 0.0f) {
        return QVector<float>();
    }

    const float threshold = peak * ThresholdRatio;
    int first = 0;
    while (first < windowCount && levels.at(first) < threshold) {
        ++first;
    }
    int last = windowCount - 1;
    while (last >= first && levels.at(last) < threshold) {
        --last;
    }
    if (first > last) {
        return QVector<float>();
    }

    const int margin = TargetSampleRate * MarginMs / 1000;
    const int begin = qMax(0, first * windowSize - margin);
    const int end = qMin(samples.size(), (last + 1) * windowSize + margin);
    return samples.mid(begin, end - begin);
}

QVector<float> SttAudioRecorder::toMono16k() const
{
    const int channels = qMax(1, m_format.channelCount());
    const int bytesPerSample = qMax(1, m_format.bytesPerSample());
    const int bytesPerFrame = bytesPerSample * channels;
    const int frames = m_buffer.size() / bytesPerFrame;
    if (frames <= 0) {
        return QVector<float>();
    }

    const char *data = m_buffer.constData();
    QVector<float> mono(frames);
    for (int i = 0; i < frames; ++i) {
        float sum = 0;
        for (int channel = 0; channel < channels; ++channel) {
            sum += sampleAt(data + (i * channels + channel) * bytesPerSample);
        }
        mono[i] = sum / float(channels);
    }

    const int rate = m_format.sampleRate();
    if (rate == TargetSampleRate || rate <= 0) {
        return mono;
    }

    const int outFrames = int(qint64(frames) * TargetSampleRate / rate);
    QVector<float> resampled(outFrames);
    const double step = double(rate) / TargetSampleRate;
    for (int i = 0; i < outFrames; ++i) {
        const double position = i * step;
        const int left = int(position);
        const int right = qMin(left + 1, frames - 1);
        const double fraction = position - left;
        resampled[i] = float(mono[left] * (1.0 - fraction) + mono[right] * fraction);
    }
    return resampled;
}

} // namespace PlasmaKeyboardStt
