/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "sttengine.h"

#include <QString>

namespace PlasmaKeyboardStt
{

/**
 * The GigaAM engine of the keyboard.
 *
 * The recognition itself lives in transcribe.cpp, a ggml based speech
 * recognition library (MIT) that is built together with the keyboard and
 * shipped as a library of its own. The separate library is not a matter of
 * taste: transcribe.cpp carries its own copy of ggml, and that copy would
 * collide with the one whisper.cpp brings into the keyboard (the same target
 * and symbol names), so the engine loads it while the application runs, like
 * the Vosk one. When the library is not installed the engine simply reports
 * itself unavailable and the other engines keep working.
 *
 * GigaAM v3 is a Russian model: the language hint is accepted and ignored.
 */
class GigaamEngine : public SttEngine
{
    Q_OBJECT

public:
    explicit GigaamEngine(QObject *parent = nullptr);
    ~GigaamEngine() override;

    QString id() const override;
    QString displayName() const override;
    bool isAvailable(QString *reason = nullptr) const override;
    bool loadModel(const QString &modelPath, QString *error) override;
    void unloadModel() override;
    bool hasModel() const override;
    QString transcribe(const QVector<float> &samples, const QString &language, QString *error) override;

private:
    /**
     * The part of the C API of transcribe.cpp the engine uses. The functions
     * are looked up when the library is loaded, so the header of the library
     * is not needed to build against it.
     */
    struct Api {
        using ModelLoadFile = int (*)(const char *path, const void *params, void **outModel);
        using ModelFree = void (*)(void *model);
        using SessionInit = int (*)(void *model, const void *params, void **outSession);
        using SessionFree = void (*)(void *session);
        using Run = int (*)(void *session, const float *pcm, int sampleCount, const void *params);
        using FullText = const char *(*)(const void *session);
        using StatusString = const char *(*)(int status);
        using Version = const char *(*)();

        ModelLoadFile modelLoadFile = nullptr;
        ModelFree modelFree = nullptr;
        SessionInit sessionInit = nullptr;
        SessionFree sessionFree = nullptr;
        Run run = nullptr;
        FullText fullText = nullptr;
        StatusString statusString = nullptr;
        Version version = nullptr;

        bool isValid() const
        {
            return modelLoadFile && modelFree && sessionInit && sessionFree && run && fullText;
        }
    };

    static const Api *api();

    void *m_model = nullptr;
    void *m_session = nullptr;
    QString m_modelPath;
};

} // namespace PlasmaKeyboardStt
