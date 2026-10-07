/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QList>
#include <QObject>
#include <QString>

namespace PlasmaKeyboardStt
{

/**
 * One model the keyboard can download, as the catalog describes it.
 */
struct SttModelEntry {
    //! Identifier used in the settings and in the download directory.
    QString id;
    //! Engine the model belongs to: "parakeet", "whisper" or "vosk".
    QString engine;
    //! Name shown in the settings.
    QString name;
    //! Language the model recognises; empty for the multilingual ones.
    QString language;
    //! Expected size in bytes; the download is rejected when it does not match.
    qint64 size = 0;
    //! Expected SHA-256, empty when the host does not publish one.
    QString sha256;
    QString url;
    //! Name of the file the model is stored under.
    QString fileName;
    //! Whether the download is an archive that has to be unpacked (the Vosk models).
    bool archive = false;
    //! Whether this is the model a fresh installation starts with.
    bool isDefault = false;
};

/**
 * The list of speech recognition models and the state of the ones that are
 * installed. The list itself is compiled into the application (a JSON resource);
 * the models live in the user's data directory.
 *
 * Both the keyboard and the settings module use this, so the paths and the
 * meaning of "installed" are the same in both.
 */
class SttModelCatalog : public QObject
{
    Q_OBJECT

public:
    explicit SttModelCatalog(QObject *parent = nullptr);

    static SttModelCatalog *instance();

    QList<SttModelEntry> models() const;
    QList<SttModelEntry> modelsForEngine(const QString &engine) const;
    SttModelEntry entry(const QString &id) const;
    SttModelEntry defaultEntry() const;

    /*! Directory all the models are kept in. */
    static QString modelsRoot();

    /*! Directory one model is kept in: <models root>/<engine>/<id>. */
    static QString installDir(const SttModelEntry &entry);

    /**
     * Path of the installed model: the model file for the single-file engines,
     * the model directory for the unpacked archives. Empty when it is not
     * installed.
     */
    static QString installedPath(const SttModelEntry &entry);

    static bool isInstalled(const SttModelEntry &entry);

    /*! The installed model of \a engine, empty when there is none. */
    static QString firstInstalledModelPath(const QString &engine);

    /**
     * Path of the model \a engine should use: the model chosen for it in the
     * settings when it is installed, otherwise the first model installed for
     * the engine. Empty when the engine has no installed model at all.
     */
    static QString activeModelPath(const QString &engine);

    /*! Remembers \a path as the model of \a engine; an empty value forgets it. */
    static void setActiveModelPath(const QString &engine, const QString &path);

    /*! Whether \a path is the installed path of one of the models of \a engine. */
    static bool isInstalledPathForEngine(const QString &engine, const QString &path);

    /*! Removes an installed model. Returns false and sets \a error on failure. */
    bool remove(const SttModelEntry &entry, QString *error);

    /*! Tells the listeners that the installed models changed. */
    void refresh();

Q_SIGNALS:
    void changed();

private:
    QList<SttModelEntry> m_models;
};

} // namespace PlasmaKeyboardStt
