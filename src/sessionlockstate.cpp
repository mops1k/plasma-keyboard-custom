/*
    SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "sessionlockstate.h"

#include <QCoreApplication>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>

namespace
{
constexpr auto service = "org.freedesktop.ScreenSaver";
constexpr auto path = "/ScreenSaver";
constexpr auto interface = "org.freedesktop.ScreenSaver";
}

SessionLockState *SessionLockState::instance()
{
    static auto *state = new SessionLockState(QCoreApplication::instance());
    return state;
}

SessionLockState::SessionLockState(QObject *parent)
    : SessionLockState(QDBusConnection::sessionBus(), parent)
{
}

SessionLockState::SessionLockState(const QDBusConnection &bus, QObject *parent)
    : QObject(parent)
    , m_bus(bus)
{
    m_bus.connect(QLatin1String(service), QLatin1String(path), QLatin1String(interface), QStringLiteral("ActiveChanged"), this, SLOT(activeChanged(bool)));
    m_bus.connect(QLatin1String(service), QLatin1String(path), QStringLiteral("org.kde.screensaver"), QStringLiteral("AboutToLock"), this, SLOT(aboutToLock()));
    auto *watcher = new QDBusServiceWatcher(QLatin1String(service), m_bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &SessionLockState::ownerChanged);
    query();
}

void SessionLockState::query()
{
    const quint64 generation = ++m_generation;
    const auto request = QDBusMessage::createMethodCall(QLatin1String(service), QLatin1String(path), QLatin1String(interface), QStringLiteral("GetActive"));
    auto *pending = new QDBusPendingCallWatcher(m_bus.asyncCall(request, 1000), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher *call) {
        const auto reply = call->reply();
        call->deleteLater();
        if (generation != m_generation || reply.type() != QDBusMessage::ReplyMessage || reply.signature() != QLatin1String("b")) {
            return;
        }
        const bool active = reply.arguments().first().toBool();
        // GetActive is false while the locker is still acquiring its grab.
        // Neither an old nor a fresh false reply proves lock cancellation.
        if (m_preparingLock && !active) {
            return;
        }
        activeChanged(active);
    });
}

void SessionLockState::aboutToLock()
{
    ++m_generation;
    if (m_known && m_locked) {
        return;
    }
    m_preparingLock = true;
    setState(true, true); // Restrict content before the greeter becomes visible.
}

void SessionLockState::activeChanged(bool active)
{
    ++m_generation;
    const bool wasPreparing = m_preparingLock;
    m_preparingLock = false;
    const bool stateUnchanged = m_known && m_locked == active;
    setState(true, active);
    if (wasPreparing && stateUnchanged) {
        Q_EMIT stateChanged();
    }
}

void SessionLockState::ownerChanged(const QString &, const QString &, const QString &newOwner)
{
    ++m_generation;
    m_preparingLock = false;
    setState(false, false);
    if (!newOwner.isEmpty()) {
        query();
    }
}

void SessionLockState::setState(bool known, bool locked)
{
    if (known == m_known && locked == m_locked) {
        return;
    }
    const bool didUnlock = m_known && m_locked && known && !locked;
    m_known = known;
    m_locked = locked;
    if (didUnlock) {
        Q_EMIT unlocked();
    }
    Q_EMIT stateChanged();
}
