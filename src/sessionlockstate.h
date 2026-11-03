/*
    SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once

#include <QDBusConnection>
#include <QObject>

/** Cached lock state; never make an input handler wait for a D-Bus reply. */
class SessionLockState : public QObject
{
    Q_OBJECT
public:
    static SessionLockState *instance();
    explicit SessionLockState(QObject *parent = nullptr);
    explicit SessionLockState(const QDBusConnection &bus, QObject *parent = nullptr);

    bool isKnown() const
    {
        return m_known;
    }
    bool isLocked() const
    {
        return m_locked;
    }
    bool isPreparingLock() const
    {
        return m_preparingLock;
    }
    bool allowsPredictions() const
    {
        return m_known && !m_locked;
    }

Q_SIGNALS:
    void stateChanged();
    // Emitted before stateChanged so the old lock-screen panel can be hidden
    // before any normal-field prediction is requested.
    void unlocked();

private Q_SLOTS:
    void aboutToLock();
    void activeChanged(bool active);
    void ownerChanged(const QString &service, const QString &oldOwner, const QString &newOwner);

private:
    void query();
    void setState(bool known, bool locked);
    QDBusConnection m_bus;
    quint64 m_generation = 0;
    bool m_preparingLock = false;
    bool m_known = false;
    bool m_locked = false;
};
