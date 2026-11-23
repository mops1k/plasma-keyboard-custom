/*
    SPDX-FileCopyrightText: 2026 Plasma Keyboard contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "clipboardhistory.h"
#include "plasmakeyboardsettings.h"
#include "sessionlockstate.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUrl>
#include <utility>

namespace
{
constexpr auto service = "org.kde.klipper";
constexpr auto path = "/klipper";
constexpr auto interface = "org.kde.klipper.klipper";
constexpr int maxEntries = 50;
constexpr int pollIntervalMs = 2000;
//! Klipper presents an image entry of its history as this character followed by
//! the size of the image, because its D-Bus interface can only carry text (see
//! the class documentation).
const QChar imagePlaceholder(0x25A8);
//! Thumbnails are small; anything bigger than this is not worth loading into
//! the keyboard only to paste it.
constexpr qint64 maxImageBytes = 32 * 1024 * 1024;
constexpr int thumbnailSize = 256;
constexpr int maxThumbnails = 256;

QDBusMessage request(const QString &method)
{
    auto message = QDBusMessage::createMethodCall(QLatin1String(service), QLatin1String(path), QLatin1String(interface), method);
    // Never start a clipboard service from the input method.
    message.setAutoStartService(false);
    return message;
}
}

ClipboardHistory::ClipboardHistory(SessionLockState *lockState, QObject *parent)
    : ClipboardHistory(lockState, QDBusConnection::sessionBus(), parent)
{
}

ClipboardHistory::ClipboardHistory(SessionLockState *lockState, const QDBusConnection &bus, QObject *parent)
    : QAbstractListModel(parent)
    , m_bus(bus)
    , m_lockState(lockState)
    , m_klipperDataDir(qEnvironmentVariable("PLASMA_KEYBOARD_KLIPPER_DATA_DIR",
                                            QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/klipper")))
    , m_thumbnailDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/clipboard-thumbnails"))
{
    Q_ASSERT(m_lockState);
    m_bus.connect(QLatin1String(service), QLatin1String(path), QLatin1String(interface), QStringLiteral("clipboardHistoryUpdated"), this, SLOT(refresh()));
    m_pollTimer.setInterval(pollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &ClipboardHistory::refresh);
    connect(PlasmaKeyboardSettings::self(), &PlasmaKeyboardSettings::clipboardEnabledChanged, this, &ClipboardHistory::updateEnabled);
    connect(lockState, &SessionLockState::stateChanged, this, &ClipboardHistory::updateEnabled);
    connect(lockState, &QObject::destroyed, this, &ClipboardHistory::updateEnabled);
    updateEnabled();
}

ClipboardHistory::~ClipboardHistory()
{
    if (m_database.isValid()) {
        const QString name = m_database.connectionName();
        m_database.close();
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(name);
    }
}

int ClipboardHistory::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.count();
}

QVariant ClipboardHistory::data(const QModelIndex &index, int role) const
{
    if (!isAvailable()) {
        return {};
    }
    const Entry *entry = entryAt(index.row());
    if (!entry) {
        return {};
    }
    switch (role) {
    case TextRole:
        return entry->text;
    case ImageRole:
        return entry->isImage();
    case ThumbnailRole:
        return entry->thumbnailUrl;
    default:
        return {};
    }
}

QHash<int, QByteArray> ClipboardHistory::roleNames() const
{
    return {{TextRole, QByteArrayLiteral("text")}, {ImageRole, QByteArrayLiteral("image")}, {ThumbnailRole, QByteArrayLiteral("thumbnail")}};
}

int ClipboardHistory::count() const
{
    return isAvailable() ? m_entries.count() : 0;
}

bool ClipboardHistory::isAvailable() const
{
    return m_accessAllowed && m_lockState && m_lockState->allowsPredictions() && PlasmaKeyboardSettings::self()->clipboardEnabled();
}

void ClipboardHistory::setAccessAllowed(bool allowed)
{
    if (m_accessAllowed == allowed) {
        return;
    }
    m_accessAllowed = allowed;
    updateEnabled();
}

const ClipboardHistory::Entry *ClipboardHistory::entryAt(int row) const
{
    if (row < 0 || row >= m_entries.count()) {
        return nullptr;
    }
    return &m_entries.at(row);
}

QString ClipboardHistory::textAt(int row) const
{
    const Entry *entry = isAvailable() ? entryAt(row) : nullptr;
    return entry ? entry->text : QString();
}

bool ClipboardHistory::isImageAt(int row) const
{
    const Entry *entry = isAvailable() ? entryAt(row) : nullptr;
    return entry && entry->isImage();
}

QString ClipboardHistory::thumbnailAt(int row) const
{
    const Entry *entry = isAvailable() ? entryAt(row) : nullptr;
    return entry ? entry->thumbnailUrl : QString();
}

QString ClipboardHistory::imagePathAt(int row) const
{
    const Entry *entry = isAvailable() ? entryAt(row) : nullptr;
    return entry ? entry->imagePath : QString();
}

void ClipboardHistory::clearLocal()
{
    m_pendingEntries.clear();
    if (m_entries.isEmpty()) {
        return;
    }
    beginResetModel();
    m_entries.clear();
    endResetModel();
    Q_EMIT countChanged();
}

void ClipboardHistory::clear()
{
    if (!isAvailable()) {
        return;
    }
    ++m_generation;
    m_refreshAgain = false;
    clearLocal();
    m_bus.asyncCall(request(QStringLiteral("clearClipboardHistory")), 1000);
}

void ClipboardHistory::updateEnabled()
{
    ++m_generation;
    m_refreshAgain = false;
    m_pendingEntries.clear();
    if (!isAvailable()) {
        m_pollTimer.stop();
        clearLocal();
    } else {
        refresh();
        m_pollTimer.start();
    }
    Q_EMIT availableChanged();
}

void ClipboardHistory::refresh()
{
    // Guard every entry point, including the Klipper signal and direct calls.
    if (!isAvailable()) {
        return;
    }
    if (m_callInFlight) {
        m_refreshAgain = true;
        return;
    }
    m_refreshAgain = false;
    m_pendingEntries.clear();
    refreshKlipperImages();
    fetchItem(0, m_generation);
}

void ClipboardHistory::fetchItem(int index, quint64 generation)
{
    if (!isAvailable() || generation != m_generation) {
        return;
    }
    m_callInFlight = true;
    auto message = request(QStringLiteral("getClipboardHistoryItem"));
    message << index;
    auto *pending = new QDBusPendingCallWatcher(m_bus.asyncCall(message, 1000), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this, [this, index, generation](QDBusPendingCallWatcher *call) {
        m_callInFlight = false;
        call->deleteLater();
        // Do not unwrap any stale/protected clipboard text, even from a call
        // that was sent before the lock notification arrived.
        if (!isAvailable() || generation != m_generation) {
            if (m_refreshAgain && isAvailable()) {
                refresh();
            }
            return;
        }
        const auto reply = call->reply();
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            // Don't publish a truncated or stale history on a service failure.
            clearLocal();
            return;
        }
        const auto value = reply.arguments().first();
        const QString text = value.metaType() == QMetaType::fromType<QDBusVariant>() ? value.value<QDBusVariant>().variant().toString() : value.toString();

        Entry entry;
        const bool hasKlipperImage = index < m_klipperImages.count() && !m_klipperImages.at(index).filePath.isEmpty();
        // Klipper puts the placeholder in front of the size of the image
        // ("▨ 1920×1080"), so the entry starts with it.
        if (text.startsWith(imagePlaceholder) && hasKlipperImage) {
            const KlipperImage &image = m_klipperImages.at(index);
            entry.imagePath = image.filePath;
            entry.thumbnailUrl = thumbnailFor(image.uuid, image.filePath);
            if (entry.thumbnailUrl.isEmpty()) {
                // The file is there but cannot be decoded or cached: leave the
                // entry as the placeholder text it already was.
                entry.imagePath.clear();
                entry.text = text;
            }
        } else {
            entry.text = text;
        }
        if (!entry.text.isEmpty() || entry.isImage()) {
            m_pendingEntries.append(entry);
        }
        if (text.isEmpty() || index + 1 >= maxEntries) {
            publish();
            if (m_refreshAgain) {
                refresh();
            }
        } else {
            fetchItem(index + 1, generation);
        }
    });
}

void ClipboardHistory::publish()
{
    if (++m_refreshCount % 100 == 0) {
        pruneThumbnailCache();
    }
    if (m_pendingEntries == m_entries) {
        m_pendingEntries.clear();
        return;
    }
    beginResetModel();
    m_entries = std::exchange(m_pendingEntries, {});
    endResetModel();
    Q_EMIT countChanged();
}

bool ClipboardHistory::openKlipperDatabase()
{
    if (m_databaseUnavailable) {
        return false;
    }
    if (m_database.isOpen()) {
        return true;
    }
    QString databasePath = m_klipperDataDir + QStringLiteral("/history3.sqlite");
    if (!QFileInfo::exists(databasePath)) {
        // An older or newer Klipper may name its history database differently.
        const QDir dir(m_klipperDataDir);
        const QStringList candidates = dir.entryList({QStringLiteral("history*.sqlite")}, QDir::Files, QDir::Name);
        if (candidates.isEmpty()) {
            return false;
        }
        // Only the newest database matters; Klipper migrates its history and
        // leaves the older files behind.
        databasePath = dir.filePath(candidates.last());
    }

    if (!m_database.isValid()) {
        m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("klipper-clipboard-history-%1").arg(quintptr(this), 0, 16));
        m_database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    }
    m_database.setDatabaseName(databasePath);
    if (!m_database.open()) {
        m_databaseUnavailable = true;
        qWarning("clipboard: cannot read %s: %s", qPrintable(databasePath), qPrintable(m_database.lastError().text()));
        return false;
    }
    return true;
}

void ClipboardHistory::refreshKlipperImages()
{
    m_klipperImages.clear();
    if (!openKlipperDatabase()) {
        return;
    }

    QSqlQuery query(m_database);
    const QString statement = QStringLiteral(
                                  "SELECT m.uuid, m.mimetypes,"
                                  " (SELECT a.data_uuid FROM aux a WHERE a.uuid = m.uuid AND a.mimetype LIKE 'image/%' LIMIT 1)"
                                  " FROM main m ORDER BY m.added_time DESC LIMIT %1")
                                  .arg(maxEntries);
    if (!query.exec(statement)) {
        // The schema is Klipper's own: if it changed, images are simply not
        // offered, the text history keeps working.
        m_databaseUnavailable = true;
        qWarning("clipboard: cannot read the Klipper history: %s", qPrintable(query.lastError().text()));
        return;
    }

    const QString dataDir = m_klipperDataDir + QStringLiteral("/data");
    while (query.next()) {
        KlipperImage row;
        const QString uuid = query.value(0).toString();
        const QString mimeTypes = query.value(1).toString();
        if (mimeTypes.contains(QLatin1String("image/"))) {
            QString dataUuid = query.value(2).toString();
            if (dataUuid.isEmpty()) {
                dataUuid = uuid;
            }
            const QString filePath = dataDir + QLatin1Char('/') + uuid + QLatin1Char('/') + dataUuid;
            if (QFileInfo(filePath).size() > 0 && QFileInfo(filePath).size() <= maxImageBytes) {
                row.uuid = uuid;
                row.filePath = filePath;
            }
        }
        m_klipperImages.append(row);
    }
}

QString ClipboardHistory::thumbnailFor(const QString &uuid, const QString &imagePath)
{
    if (uuid.isEmpty()) {
        return {};
    }
    const QString thumbnailPath = m_thumbnailDir + QLatin1Char('/') + uuid + QStringLiteral(".png");
    if (QFileInfo(thumbnailPath).size() > 0) {
        return QUrl::fromLocalFile(thumbnailPath).toString();
    }

    QImageReader reader(imagePath);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid()) {
        const QSize scaled = size.scaled(QSize(thumbnailSize, thumbnailSize), Qt::KeepAspectRatio);
        if (!scaled.isEmpty() && scaled != size) {
            reader.setScaledSize(scaled);
        }
    }
    const QImage image = reader.read();
    if (image.isNull() || !QDir().mkpath(m_thumbnailDir)) {
        return {};
    }
    // Written through a temporary name, so that a half-written thumbnail is
    // never picked up by the next refresh.
    const QString temporaryPath = thumbnailPath + QStringLiteral(".tmp");
    if (!image.save(temporaryPath, "PNG")) {
        QFile::remove(temporaryPath);
        return {};
    }
    QFile::remove(thumbnailPath);
    if (!QFile::rename(temporaryPath, thumbnailPath)) {
        return {};
    }
    return QUrl::fromLocalFile(thumbnailPath).toString();
}

void ClipboardHistory::pruneThumbnailCache()
{
    QDir dir(m_thumbnailDir);
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.png")}, QDir::Files, QDir::Time | QDir::Reversed);
    for (int i = 0; i < files.count() - maxThumbnails; ++i) {
        QFile::remove(files.at(i).absoluteFilePath());
    }
}
