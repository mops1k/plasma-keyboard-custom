/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "sttmodelcatalog.h"

#include "plasmakeyboardsettings.h"

#include <KLocalizedString>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcSttCatalog, "org.kde.plasma.keyboard.custom.stt.catalog")

namespace PlasmaKeyboardStt
{

namespace
{
constexpr auto CatalogResource = ":/stt/sttmodelcatalog.json";

//! The model chosen for each engine is kept in the settings as "engine=path"
//! lines, so a new engine does not need a setting of its own.
QString chosenPath(const QString &engine, const QString &configured)
{
    const QString prefix = engine + QLatin1Char('=');
    const QStringList lines = configured.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.startsWith(prefix)) {
            return line.mid(prefix.size());
        }
    }
    return QString();
}

QString withChosenPath(const QString &engine, const QString &path, const QString &configured)
{
    const QString prefix = engine + QLatin1Char('=');
    QStringList lines = configured.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    lines.removeIf([&prefix](const QString &line) {
        return line.startsWith(prefix);
    });
    if (!path.isEmpty()) {
        lines.append(prefix + path);
    }
    return lines.join(QLatin1Char('\n'));
}
}

SttModelCatalog::SttModelCatalog(QObject *parent)
    : QObject(parent)
{
    QFile file(QString::fromLatin1(CatalogResource));
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcSttCatalog) << "cannot read the model catalog" << CatalogResource;
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qCWarning(lcSttCatalog) << "cannot parse the model catalog:" << parseError.errorString();
        return;
    }

    const QJsonArray entries = document.object().value(QStringLiteral("models")).toArray();
    m_models.reserve(entries.size());
    for (const QJsonValue &value : entries) {
        const QJsonObject object = value.toObject();
        SttModelEntry entry;
        entry.id = object.value(QStringLiteral("id")).toString();
        entry.engine = object.value(QStringLiteral("engine")).toString();
        entry.name = object.value(QStringLiteral("name")).toString();
        entry.language = object.value(QStringLiteral("language")).toString();
        entry.size = qint64(object.value(QStringLiteral("size")).toDouble());
        entry.sha256 = object.value(QStringLiteral("sha256")).toString();
        entry.url = object.value(QStringLiteral("url")).toString();
        entry.fileName = object.value(QStringLiteral("file")).toString();
        entry.archive = object.value(QStringLiteral("archive")).toBool();
        entry.isDefault = object.value(QStringLiteral("default")).toBool();
        if (entry.id.isEmpty() || entry.engine.isEmpty() || entry.url.isEmpty() || entry.fileName.isEmpty()) {
            qCWarning(lcSttCatalog) << "ignoring an incomplete model entry" << entry.id;
            continue;
        }
        m_models.append(entry);
    }
    qCDebug(lcSttCatalog) << "model catalog:" << m_models.size() << "models";
}

SttModelCatalog *SttModelCatalog::instance()
{
    static SttModelCatalog catalog;
    return &catalog;
}

QList<SttModelEntry> SttModelCatalog::models() const
{
    return m_models;
}

QList<SttModelEntry> SttModelCatalog::modelsForEngine(const QString &engine) const
{
    QList<SttModelEntry> result;
    for (const SttModelEntry &entry : m_models) {
        if (entry.engine == engine) {
            result.append(entry);
        }
    }
    return result;
}

SttModelEntry SttModelCatalog::entry(const QString &id) const
{
    for (const SttModelEntry &entry : m_models) {
        if (entry.id == id) {
            return entry;
        }
    }
    return SttModelEntry();
}

SttModelEntry SttModelCatalog::defaultEntry() const
{
    for (const SttModelEntry &entry : m_models) {
        if (entry.isDefault) {
            return entry;
        }
    }
    return m_models.isEmpty() ? SttModelEntry() : m_models.first();
}

QString SttModelCatalog::modelsRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/plasma-keyboard/stt");
}

QString SttModelCatalog::installDir(const SttModelEntry &entry)
{
    return modelsRoot() + QLatin1Char('/') + entry.engine + QLatin1Char('/') + entry.id;
}

QString SttModelCatalog::installedPath(const SttModelEntry &entry)
{
    const QString dir = installDir(entry);
    if (entry.archive) {
        // The archive is unpacked into the model directory; the model itself is
        // the directory Vosk is pointed at.
        const QDir modelDir(dir);
        if (modelDir.exists() && !modelDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()) {
            return dir;
        }
        return QString();
    }

    const QString file = dir + QLatin1Char('/') + entry.fileName;
    return QFileInfo::exists(file) ? file : QString();
}

bool SttModelCatalog::isInstalled(const SttModelEntry &entry)
{
    return !installedPath(entry).isEmpty();
}

QString SttModelCatalog::firstInstalledModelPath(const QString &engine)
{
    const SttModelCatalog *catalog = instance();
    for (const SttModelEntry &entry : catalog->modelsForEngine(engine)) {
        const QString path = installedPath(entry);
        if (!path.isEmpty()) {
            return path;
        }
    }
    return QString();
}

QString SttModelCatalog::activeModelPath(const QString &engine)
{
    // The model the user picked wins, but only while it is still installed: a
    // model that was removed (or a path from an older version) falls back to
    // the first installed model of the engine instead of leaving the engine
    // without a model.
    const QString chosen = chosenPath(engine, PlasmaKeyboardSettings::self()->sttModelPaths());
    if (isInstalledPathForEngine(engine, chosen)) {
        return chosen;
    }
    return firstInstalledModelPath(engine);
}

void SttModelCatalog::setActiveModelPath(const QString &engine, const QString &path)
{
    PlasmaKeyboardSettings::self()->setSttModelPaths(withChosenPath(engine, path, PlasmaKeyboardSettings::self()->sttModelPaths()));
    PlasmaKeyboardSettings::self()->save();
}

bool SttModelCatalog::isInstalledPathForEngine(const QString &engine, const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }
    const SttModelCatalog *catalog = instance();
    for (const SttModelEntry &entry : catalog->modelsForEngine(engine)) {
        if (installedPath(entry) == path) {
            return true;
        }
    }
    return false;
}

bool SttModelCatalog::remove(const SttModelEntry &entry, QString *error)
{
    const QString dir = installDir(entry);
    const QString root = modelsRoot();

    // Never remove anything outside the model directory: the settings module
    // passes the entry it got from the catalog, but the path is built from the
    // settings file in other places.
    if (!QDir::cleanPath(dir).startsWith(QDir::cleanPath(root) + QLatin1Char('/'))) {
        if (error) {
            *error = i18nd("plasma-keyboard-custom", "Refusing to remove “%1”: it is outside the model directory.").arg(dir);
        }
        return false;
    }

    QDir modelDir(dir);
    if (!modelDir.exists()) {
        return true;
    }
    if (!modelDir.removeRecursively()) {
        if (error) {
            *error = i18nd("plasma-keyboard-custom", "Cannot remove “%1”.").arg(dir);
        }
        return false;
    }

    qCDebug(lcSttCatalog) << "removed the model" << entry.id << "from" << dir;
    Q_EMIT changed();
    return true;
}

void SttModelCatalog::refresh()
{
    Q_EMIT changed();
}

} // namespace PlasmaKeyboardStt
