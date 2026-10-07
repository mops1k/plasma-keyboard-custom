/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QAudioDevice>
#include <QAudioFormat>
#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QVector>

class QAudioSource;
class QIODevice;

namespace PlasmaKeyboardStt
{

/**
 * Records the microphone into the format the speech engines expect: 16 kHz,
 * mono, floating point samples in the -1..1 range.
 *
 * The input device is used in the format it supports: when it cannot record at
 * 16 kHz the samples are recorded in its own format and converted (channel mix
 * and resampling) when the recording stops.
 */
class SttAudioRecorder : public QObject
{
    Q_OBJECT

public:
    explicit SttAudioRecorder(QObject *parent = nullptr);
    ~SttAudioRecorder() override;

    /*! The microphones available for recording. */
    static QList<QAudioDevice> inputDevices();

    /**
     * Starts recording. \a deviceId is the id of a device from inputDevices();
     * an empty value uses the default input device. Returns false and sets
     * \a error when the device cannot be opened.
     */
    bool start(const QString &deviceId, QString *error = nullptr);

    /*! Stops recording and returns the recorded audio as 16 kHz mono samples. */
    QVector<float> stop();

    /**
     * Drops the silence before and after the speech from \a samples.
     *
     * A recording starts when the microphone key is pressed and stops when it
     * is pressed again, so it usually carries a pause at both ends. The engines
     * do not need it, and a phrase is recognised better without it.
     *
     * The level is measured in windows of 20 ms; the first and the last window
     * that reach a fraction of the loudest window are kept, plus a short margin
     * so the first and the last sound are not cut off. A recording that is
     * silent from end to end gives an empty result: there is nothing to
     * recognise.
     */
    static QVector<float> trimSilence(const QVector<float> &samples);

    /*! Stops recording and throws the recorded audio away. */
    void cancel();

    bool isRecording() const;

    /*! Peak level of the last read block, 0..1, for the recording indicator. */
    qreal level() const;

    /*! Length of the current recording in milliseconds. */
    int recordedMs() const;

Q_SIGNALS:
    void recordingChanged(bool recording);
    void levelChanged(qreal level);
    void errorOccurred(const QString &message);

private:
    void handleReadyRead();
    QVector<float> toMono16k() const;
    float sampleAt(const char *pointer) const;

    QAudioSource *m_source = nullptr;
    QIODevice *m_io = nullptr;
    QAudioFormat m_format;
    QByteArray m_buffer;
    qreal m_level = 0;
    bool m_limitReported = false;

    //! Recordings longer than this are stopped: the samples are kept in memory
    //! and a stuck button must not eat the whole RAM.
    static constexpr int MaximumRecordingMs = 60000;
};

} // namespace PlasmaKeyboardStt
