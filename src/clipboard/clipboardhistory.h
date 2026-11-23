/*
    SPDX-FileCopyrightText: 2026 Plasma Keyboard contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once

#include <QAbstractListModel>
#include <QDBusConnection>
#include <QList>
#include <QPointer>
#include <QSqlDatabase>
#include <QTimer>
#include <qqmlintegration.h>

class SessionLockState;

/** Session clipboard suggestions. No access until both session and field are
 * known safe. All Klipper calls are asynchronous; stale replies are discarded.
 * Clearing the local cache on lock never clears the desktop's clipboard.
 *
 * Klipper 6.8 keeps images in its history as well, but its D-Bus interface
 * only offers the entries as text: an image entry comes back as a placeholder
 * character followed by the size of the image. The history database next to it
 * names the file the entry was stored in, so an image entry is recognised here
 * by that placeholder plus the database row at the same index, and its
 * thumbnail is prepared for the row above the keyboard. The database is only
 * read, and only while the clipboard is available, exactly like the D-Bus
 * history itself.
 */
class ClipboardHistory : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ClipboardHistory is created in C++ and passed to QML.")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)

public:
    enum Role {
        TextRole = Qt::UserRole + 1,
        ImageRole,
        ThumbnailRole,
    };
    explicit ClipboardHistory(SessionLockState *lockState, QObject *parent = nullptr);
    ClipboardHistory(SessionLockState *lockState, const QDBusConnection &bus, QObject *parent = nullptr);
    ~ClipboardHistory() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const;
    bool isAvailable() const;
    // Only trusted C++ field-state updates can enable access, never QML.
    void setAccessAllowed(bool allowed);
    Q_INVOKABLE QString textAt(int row) const;
    Q_INVOKABLE bool isImageAt(int row) const;
    //! Cached thumbnail of an image entry, a file:// URL, empty otherwise.
    Q_INVOKABLE QString thumbnailAt(int row) const;
    //! Absolute path of the PNG behind an image entry, empty otherwise.
    Q_INVOKABLE QString imagePathAt(int row) const;
    Q_INVOKABLE void clear();

Q_SIGNALS:
    void countChanged();
    void availableChanged();

private Q_SLOTS:
    void refresh();
    void updateEnabled();

private:
    struct Entry {
        QString text;
        QString imagePath;
        QString thumbnailUrl;
        bool isImage() const
        {
            return !imagePath.isEmpty();
        }
        bool operator==(const Entry &other) const
        {
            return text == other.text && imagePath == other.imagePath && thumbnailUrl == other.thumbnailUrl;
        }
    };
    struct KlipperImage {
        QString uuid;
        QString filePath;
    };

    void fetchItem(int index, quint64 generation);
    void clearLocal();
    void publish();
    void refreshKlipperImages();
    bool openKlipperDatabase();
    QString thumbnailFor(const QString &uuid, const QString &imagePath);
    void pruneThumbnailCache();
    const Entry *entryAt(int row) const;

    QDBusConnection m_bus;
    QPointer<SessionLockState> m_lockState;
    QTimer m_pollTimer;
    QList<Entry> m_entries;
    QList<Entry> m_pendingEntries;
    //! One row per entry of the Klipper history, in the D-Bus order; the file
    //! path is set only for the rows saved as images.
    QList<KlipperImage> m_klipperImages;
    QString m_klipperDataDir;
    QString m_thumbnailDir;
    QSqlDatabase m_database;
    bool m_databaseUnavailable = false;
    quint64 m_generation = 0;
    quint64 m_refreshCount = 0;
    bool m_accessAllowed = false;
    bool m_callInFlight = false;
    bool m_refreshAgain = false;
};
