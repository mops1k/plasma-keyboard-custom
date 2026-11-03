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
#include <utility>

namespace
{
constexpr auto service = "org.kde.klipper";
constexpr auto path = "/klipper";
constexpr auto interface = "org.kde.klipper.klipper";
constexpr int maxEntries = 50;
constexpr int pollIntervalMs = 2000;

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

int ClipboardHistory::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.count();
}

QVariant ClipboardHistory::data(const QModelIndex &index, int role) const
{
    if (!isAvailable() || !index.isValid() || index.row() < 0 || index.row() >= m_entries.count() || role != TextRole) {
        return {};
    }
    return m_entries.at(index.row());
}

QHash<int, QByteArray> ClipboardHistory::roleNames() const
{
    return {{TextRole, QByteArrayLiteral("text")}};
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

QString ClipboardHistory::textAt(int row) const
{
    return isAvailable() ? m_entries.value(row) : QString();
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
        const QString entry = value.metaType() == QMetaType::fromType<QDBusVariant>() ? value.value<QDBusVariant>().variant().toString() : value.toString();
        if (!entry.isEmpty()) {
            m_pendingEntries.append(entry);
        }
        if (entry.isEmpty() || index + 1 >= maxEntries) {
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
    if (m_pendingEntries == m_entries) {
        m_pendingEntries.clear();
        return;
    }
    beginResetModel();
    m_entries = std::exchange(m_pendingEntries, {});
    endResetModel();
    Q_EMIT countChanged();
}
