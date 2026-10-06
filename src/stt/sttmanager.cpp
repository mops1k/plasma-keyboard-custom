/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "sttmanager.h"

#include "gigaamengine.h"
#include "parakeetengine.h"
#include "sttaudiorecorder.h"
#include "sttengine.h"
#include "sttmodelcatalog.h"
#include "voskengine.h"
#include "whisperengine.h"

#include "plasmakeyboardsettings.h"

#include <KLocalizedString>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QStandardPaths>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>

Q_LOGGING_CATEGORY(lcStt, "org.kde.plasma.keyboard.custom.stt")

namespace PlasmaKeyboardStt
{

SttManager *SttManager::s_instance = nullptr;

SttManager::SttManager(QObject *parent)
    : QObject(parent)
    , m_recorder(new SttAudioRecorder(this))
{
    s_instance = this;
    // One recognition at a time: the engine keeps one loaded model and is not
    // thread safe for parallel calls.
    m_pool.setMaxThreadCount(1);

    connect(m_recorder, &SttAudioRecorder::levelChanged, this, &SttManager::levelChanged);
    connect(m_recorder, &SttAudioRecorder::recordingChanged, this, &SttManager::recordingChanged);
    connect(m_recorder, &SttAudioRecorder::errorOccurred, this, &SttManager::handleRecorderError);

    // Downloading or removing a model changes whether the engine can be used.
    connect(SttModelCatalog::instance(), &SttModelCatalog::changed, this, [this]() {
        m_modelDirty = true;
        Q_EMIT stateChanged();
    });

    createEngine();
}

SttManager::~SttManager()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

SttManager *SttManager::instance()
{
    return s_instance;
}

QString SttManager::defaultEngineId()
{
    return QStringLiteral("parakeet");
}

QStringList SttManager::engineIds()
{
    return {QStringLiteral("parakeet"), QStringLiteral("whisper"), QStringLiteral("gigaam"), QStringLiteral("vosk")};
}

bool SttManager::isEnabled() const
{
    return PlasmaKeyboardSettings::self()->sttEnabled();
}

void SttManager::setEnabled(bool enabled)
{
    if (isEnabled() == enabled) {
        return;
    }
    PlasmaKeyboardSettings::self()->setSttEnabled(enabled);
    if (!enabled) {
        cancel();
    }
    Q_EMIT enabledChanged();
    Q_EMIT stateChanged();
}

bool SttManager::isAvailable() const
{
    if (!m_engine) {
        return false;
    }
    QString reason;
    if (!m_engine->isAvailable(&reason)) {
        return false;
    }
    const QString path = modelPath();
    return !path.isEmpty() && QFileInfo::exists(path);
}

bool SttManager::isRecording() const
{
    return m_recorder->isRecording();
}

bool SttManager::isBusy() const
{
    return m_busy;
}

qreal SttManager::level() const
{
    return m_recorder->level();
}

QString SttManager::engineId() const
{
    const QString configured = PlasmaKeyboardSettings::self()->sttEngine();
    if (configured.isEmpty()) {
        return defaultEngineId();
    }
    return configured;
}

void SttManager::setEngineId(const QString &engineId)
{
    if (this->engineId() == engineId) {
        return;
    }
    PlasmaKeyboardSettings::self()->setSttEngine(engineId);
    if (!m_busy) {
        createEngine();
    }
    Q_EMIT engineIdChanged();
    Q_EMIT stateChanged();
}

QString SttManager::engineName() const
{
    return m_engine ? m_engine->displayName() : QString();
}

QString SttManager::modelPath() const
{
    const QString configured = PlasmaKeyboardSettings::self()->sttModelPath();
    if (!configured.isEmpty()) {
        return configured;
    }
    return SttModelCatalog::firstInstalledModelPath(engineId());
}

void SttManager::setModelPath(const QString &modelPath)
{
    if (this->modelPath() == modelPath) {
        return;
    }
    PlasmaKeyboardSettings::self()->setSttModelPath(modelPath);
    m_modelDirty = true;
    Q_EMIT modelPathChanged();
    Q_EMIT stateChanged();
}

QString SttManager::language() const
{
    if (PlasmaKeyboardSettings::self()->sttLanguageMode() == QLatin1String("fixed")) {
        return PlasmaKeyboardSettings::self()->sttLanguage();
    }
    return m_keyboardLanguage;
}

QString SttManager::lastError() const
{
    return m_lastError;
}

void SttManager::startRecording(const QString &keyboardLanguage)
{
    if (!isEnabled()) {
        setLastError(i18nd("plasma-keyboard-custom", "The voice input is disabled in the keyboard settings."));
        return;
    }
    if (isRecording() || isBusy()) {
        return;
    }

    m_keyboardLanguage = keyboardLanguage;
    Q_EMIT languageChanged();

    const QString path = modelPath();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        setLastError(i18nd("plasma-keyboard-custom", "No speech recognition model is installed. Download one in the keyboard settings."));
        return;
    }

    QString error;
    if (!m_recorder->start(PlasmaKeyboardSettings::self()->sttInputDevice(), &error)) {
        setLastError(error);
        return;
    }
    setLastError(QString());
}

void SttManager::stopRecording()
{
    if (!m_recorder->isRecording()) {
        return;
    }

    const QVector<float> samples = m_recorder->stop();
    if (samples.isEmpty()) {
        setLastError(i18nd("plasma-keyboard-custom", "Nothing was recorded."));
        return;
    }

    m_busy = true;
    Q_EMIT busyChanged();

    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
        const QString text = watcher->result();
        watcher->deleteLater();
        m_busy = false;
        Q_EMIT busyChanged();
        if (!text.isEmpty()) {
            Q_EMIT textRecognized(text);
        }
    });
    watcher->setFuture(QtConcurrent::run(&m_pool, [this, samples]() {
        return recognize(samples);
    }));
}

void SttManager::cancel()
{
    if (m_recorder->isRecording()) {
        m_recorder->cancel();
    }
}

void SttManager::reloadModel()
{
    if (m_busy) {
        return;
    }
    if (m_engine) {
        m_engine->unloadModel();
    }
    m_modelDirty = true;
    Q_EMIT stateChanged();
}

void SttManager::createEngine()
{
    const QString id = engineId();
    SttEngine *engine = nullptr;
    if (id == QLatin1String("parakeet")) {
        engine = new ParakeetEngine(this);
    } else if (id == QLatin1String("whisper")) {
        engine = new WhisperEngine(this);
    } else if (id == QLatin1String("gigaam")) {
        engine = new GigaamEngine(this);
    } else if (id == QLatin1String("vosk")) {
        engine = new VoskEngine(this);
    } else {
        qCWarning(lcStt) << "unknown speech recognition engine" << id << ", falling back to parakeet";
        engine = new ParakeetEngine(this);
    }

    if (m_engine) {
        m_engine->unloadModel();
        m_engine->deleteLater();
    }
    m_engine = engine;
    m_modelDirty = true;
    qCDebug(lcStt) << "speech recognition engine" << m_engine->id() << "selected";
    Q_EMIT stateChanged();
}

void SttManager::setLastError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }
    m_lastError = message;
    if (!message.isEmpty()) {
        qCWarning(lcStt) << message;
    }
    Q_EMIT lastErrorChanged();
}

QString SttManager::recognize(const QVector<float> &samples)
{
    SttEngine *engine = m_engine;
    if (!engine) {
        return QString();
    }

    if (m_modelDirty || !engine->hasModel()) {
        QString error;
        if (!engine->loadModel(modelPath(), &error)) {
            QMetaObject::invokeMethod(
                this,
                [this, error]() {
                    setLastError(error);
                },
                Qt::QueuedConnection);
            return QString();
        }
        m_modelDirty = false;
    }

    QString error;
    const QString text = engine->transcribe(samples, language(), &error);
    if (!error.isEmpty()) {
        QMetaObject::invokeMethod(
            this,
            [this, error]() {
                setLastError(error);
            },
            Qt::QueuedConnection);
    }
    return text;
}

void SttManager::handleRecorderError(const QString &message)
{
    setLastError(message);
    // The recorder reports its length limit through this signal: stop the
    // recording so what was said is still recognised.
    if (m_recorder->isRecording()) {
        stopRecording();
    }
}

} // namespace PlasmaKeyboardStt
