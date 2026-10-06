/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "gigaamengine.h"

#include <KLocalizedString>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QLoggingCategory>
#include <QStringList>

Q_LOGGING_CATEGORY(lcSttGigaam, "org.kde.plasma.keyboard.custom.stt.gigaam")

namespace PlasmaKeyboardStt
{

namespace
{
//! The names libtranscribe is installed under. Its soname carries the full
//! version (libtranscribe.so.0.3), so that name is tried as well.
const char *const libraryNames[] = {
    "libtranscribe.so",
    "libtranscribe.so.0.3",
    "libtranscribe.so.0",
    "transcribe",
};

/**
 * The places the library is looked for, in order: the directory the keyboard
 * was installed into (a local build keeps its libraries in the same prefix),
 * the directory of the running binary, then the names the loader knows, which
 * covers the system installation.
 */
QStringList libraryCandidates()
{
    QStringList candidates;

    // The development and test builds point the engine at the library they
    // have just built; the installed keyboard does not need this.
    const QString fromEnvironment = qEnvironmentVariable("PLASMA_KEYBOARD_TRANSCRIBE_LIBRARY");
    if (!fromEnvironment.isEmpty()) {
        candidates << fromEnvironment;
    }

    QStringList directories;
#ifdef TRANSCRIBE_LIBRARY_DIR
    directories << QStringLiteral(TRANSCRIBE_LIBRARY_DIR);
#endif
    const QString applicationDir = QCoreApplication::applicationDirPath();
    if (!applicationDir.isEmpty()) {
        directories << applicationDir;
        directories << applicationDir + QStringLiteral("/../lib");
    }

    for (const QString &directory : directories) {
        for (const char *name : libraryNames) {
            candidates << directory + QLatin1Char('/') + QString::fromLatin1(name);
        }
    }
    for (const char *name : libraryNames) {
        candidates << QString::fromLatin1(name);
    }
    return candidates;
}

//! The loaded library, kept alive as long as the functions found in it are used.
QLibrary &transcribeLibrary()
{
    static QLibrary library;
    return library;
}
} // namespace

const GigaamEngine::Api *GigaamEngine::api()
{
    static const Api *loaded = []() -> const Api * {
        static Api api;
        QLibrary &library = transcribeLibrary();
        for (const QString &candidate : libraryCandidates()) {
            library.setFileName(candidate);
            if (!library.load()) {
                continue;
            }
            api.modelLoadFile = reinterpret_cast<Api::ModelLoadFile>(library.resolve("transcribe_model_load_file"));
            api.modelFree = reinterpret_cast<Api::ModelFree>(library.resolve("transcribe_model_free"));
            api.sessionInit = reinterpret_cast<Api::SessionInit>(library.resolve("transcribe_session_init"));
            api.sessionFree = reinterpret_cast<Api::SessionFree>(library.resolve("transcribe_session_free"));
            api.run = reinterpret_cast<Api::Run>(library.resolve("transcribe_run"));
            api.fullText = reinterpret_cast<Api::FullText>(library.resolve("transcribe_full_text"));
            api.statusString = reinterpret_cast<Api::StatusString>(library.resolve("transcribe_status_string"));
            api.version = reinterpret_cast<Api::Version>(library.resolve("transcribe_version"));
            if (!api.isValid()) {
                qCWarning(lcSttGigaam) << "libtranscribe was found but does not provide the expected functions";
                library.unload();
                return nullptr;
            }
            qCDebug(lcSttGigaam) << "libtranscribe loaded from" << library.fileName() << (api.version ? api.version() : "");
            return &api;
        }
        qCDebug(lcSttGigaam) << "libtranscribe is not installed";
        return nullptr;
    }();
    return loaded;
}

GigaamEngine::GigaamEngine(QObject *parent)
    : SttEngine(parent)
{
}

GigaamEngine::~GigaamEngine()
{
    unloadModel();
}

QString GigaamEngine::id() const
{
    return QStringLiteral("gigaam");
}

QString GigaamEngine::displayName() const
{
    return QStringLiteral("GigaAM");
}

bool GigaamEngine::isAvailable(QString *reason) const
{
    if (api()) {
        return true;
    }
    if (reason) {
        *reason = i18nd("plasma-keyboard-custom", "The transcribe.cpp library (libtranscribe) is not installed.");
    }
    return false;
}

bool GigaamEngine::loadModel(const QString &modelPath, QString *error)
{
    unloadModel();

    const Api *transcribe = api();
    if (!transcribe) {
        if (error) {
            *error = i18nd("plasma-keyboard-custom", "The transcribe.cpp library (libtranscribe) is not installed.");
        }
        return false;
    }

    const QFileInfo info(modelPath);
    if (!info.exists() || !info.isFile()) {
        if (error) {
            *error = i18nd("plasma-keyboard-custom", "The GigaAM model file does not exist: %1").arg(modelPath);
        }
        return false;
    }

    // The model is loaded with the library defaults: GigaAM is a Russian model
    // and does not take a language hint.
    const int loadStatus = transcribe->modelLoadFile(QFile::encodeName(info.absoluteFilePath()).constData(), nullptr, &m_model);
    if (loadStatus != 0 || !m_model) {
        if (error) {
            *error = i18nd("plasma-keyboard-custom", "Cannot load the GigaAM model: %1")
                         .arg(transcribe->statusString ? QString::fromUtf8(transcribe->statusString(loadStatus)) : QString::number(loadStatus));
        }
        qCWarning(lcSttGigaam) << "cannot load the GigaAM model" << info.absoluteFilePath() << "status" << loadStatus;
        m_model = nullptr;
        return false;
    }

    const int sessionStatus = transcribe->sessionInit(m_model, nullptr, &m_session);
    if (sessionStatus != 0 || !m_session) {
        if (error) {
            *error = i18nd("plasma-keyboard-custom", "Cannot create the GigaAM session for “%1”.").arg(info.absoluteFilePath());
        }
        transcribe->modelFree(m_model);
        m_model = nullptr;
        m_session = nullptr;
        return false;
    }

    m_modelPath = info.absoluteFilePath();
    qCDebug(lcSttGigaam) << "gigaam model loaded" << m_modelPath;
    return true;
}

void GigaamEngine::unloadModel()
{
    const Api *transcribe = api();
    if (!transcribe) {
        m_model = nullptr;
        m_session = nullptr;
        m_modelPath.clear();
        return;
    }
    if (m_session) {
        transcribe->sessionFree(m_session);
        m_session = nullptr;
    }
    if (m_model) {
        transcribe->modelFree(m_model);
        m_model = nullptr;
        qCDebug(lcSttGigaam) << "gigaam model unloaded";
    }
    m_modelPath.clear();
}

bool GigaamEngine::hasModel() const
{
    return m_model != nullptr && m_session != nullptr;
}

QString GigaamEngine::transcribe(const QVector<float> &samples, const QString &language, QString *error)
{
    // GigaAM v3 recognises Russian only, so the hint is not used.
    Q_UNUSED(language)

    const Api *transcribe = api();
    if (!transcribe || !m_session) {
        if (error) {
            *error = i18nd("plasma-keyboard-custom", "The GigaAM model is not loaded.");
        }
        return QString();
    }
    if (samples.isEmpty()) {
        return QString();
    }

    const int status = transcribe->run(m_session, samples.constData(), samples.size(), nullptr);
    if (status != 0) {
        if (error) {
            *error = i18nd("plasma-keyboard-custom", "The GigaAM recognition failed: %1")
                         .arg(transcribe->statusString ? QString::fromUtf8(transcribe->statusString(status)) : QString::number(status));
        }
        qCWarning(lcSttGigaam) << "gigaam recognition failed with status" << status;
        return QString();
    }

    const char *text = transcribe->fullText(m_session);
    if (!text) {
        return QString();
    }
    qCDebug(lcSttGigaam) << "gigaam recognised" << samples.size() << "samples";
    return QString::fromUtf8(text).trimmed();
}

} // namespace PlasmaKeyboardStt
