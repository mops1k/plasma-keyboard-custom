/*
    SPDX-FileCopyrightText: 2026 Plasma Keyboard contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once

#include <QAbstractListModel>
#include <QDBusConnection>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <qqmlintegration.h>

class SessionLockState;

/** Session clipboard suggestions. No access until both session and field are
 * known safe. All Klipper calls are asynchronous; stale replies are discarded.
 * Clearing the local cache on lock never clears the desktop's clipboard.
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
        TextRole = Qt::UserRole + 1
    };
    explicit ClipboardHistory(SessionLockState *lockState, QObject *parent = nullptr);
    ClipboardHistory(SessionLockState *lockState, const QDBusConnection &bus, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const;
    bool isAvailable() const;
    // Only trusted C++ field-state updates can enable access, never QML.
    void setAccessAllowed(bool allowed);
    Q_INVOKABLE QString textAt(int row) const;
    Q_INVOKABLE void clear();

Q_SIGNALS:
    void countChanged();
    void availableChanged();

private Q_SLOTS:
    void refresh();
    void updateEnabled();

private:
    void fetchItem(int index, quint64 generation);
    void clearLocal();
    void publish();
    QDBusConnection m_bus;
    QPointer<SessionLockState> m_lockState;
    QTimer m_pollTimer;
    QStringList m_entries;
    QStringList m_pendingEntries;
    quint64 m_generation = 0;
    bool m_accessAllowed = false;
    bool m_callInFlight = false;
    bool m_refreshAgain = false;
};
