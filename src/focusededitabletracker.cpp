/*
    SPDX-FileCopyrightText: 2026 Chechulin Serhii

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "focusededitabletracker.h"

#include "logging.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QGuiApplication>
#include <QScreen>
#include <QUuid>

namespace
{
constexpr uint editableState = 7;
constexpr uint screenCoordinates = 0;
// Hit testing is optional: a slow application must never hold up key forwarding.
constexpr int requestTimeoutMs = 1000;
const QString focusedEvent = QStringLiteral("object:state-changed:focused");
const QString registryService = QStringLiteral("org.a11y.atspi.Registry");
const QString registryPath = QStringLiteral("/org/a11y/atspi/registry");

QDBusMessage registryCall(const QString &method)
{
    return QDBusMessage::createMethodCall(registryService, registryPath, registryService, method);
}

bool hasState(const QList<uint> &words, uint state)
{
    const qsizetype word = state / 32;
    return word < words.size() && (words.at(word) & (uint(1) << (state % 32)));
}

QPointF mapTouchToScreen(const QPointF &position, const QScreen *screen)
{
    QPointF mapped = position;
    switch (screen->angleBetween(screen->nativeOrientation(), screen->orientation())) {
    case 90:
        mapped = QPointF(1.0 - position.y(), position.x());
        break;
    case 180:
        mapped = QPointF(1.0 - position.x(), 1.0 - position.y());
        break;
    case 270:
        mapped = QPointF(position.y(), 1.0 - position.x());
        break;
    default:
        break;
    }

    const QRect geometry = screen->geometry();
    return QPointF(geometry.x() + mapped.x() * geometry.width(), geometry.y() + mapped.y() * geometry.height());
}
}

FocusedEditableTracker::FocusedEditableTracker(QObject *parent)
    : QObject(parent)
{
    // QDBusInterface construction can itself introspect synchronously. Use raw
    // messages throughout, including discovery, registration and teardown.
    const auto request = QDBusMessage::createMethodCall(QStringLiteral("org.a11y.Bus"),
                                                        QStringLiteral("/org/a11y/bus"),
                                                        QStringLiteral("org.a11y.Bus"),
                                                        QStringLiteral("GetAddress"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request, requestTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *pending) {
        const QDBusPendingReply<QString> address = *pending;
        pending->deleteLater();
        if (address.isError() || address.value().isEmpty()) {
            return;
        }

        m_connectionName = QStringLiteral("plasma-keyboard-atspi-%1").arg(QUuid::createUuid().toString(QUuid::Id128));
        const auto bus = QDBusConnection::connectToBus(address.value(), m_connectionName);
        if (!bus.isConnected()) {
            QDBusConnection::disconnectFromBus(m_connectionName);
            m_connectionName.clear();
            return;
        }
        initialize(bus);
    });
}

FocusedEditableTracker::FocusedEditableTracker(const QDBusConnection &bus, QObject *parent)
    : QObject(parent)
{
    initialize(bus);
}

void FocusedEditableTracker::initialize(const QDBusConnection &bus)
{
    m_bus = std::make_unique<QDBusConnection>(bus);
    if (!m_bus->connect(QString(),
                        QString(),
                        QStringLiteral("org.a11y.atspi.Event.Object"),
                        QStringLiteral("StateChanged"),
                        this,
                        SLOT(stateChanged(QString, int, int, QDBusVariant, QVariantMap, QDBusMessage)))) {
        m_bus.reset();
        return;
    }

    auto request = registryCall(QStringLiteral("RegisterEvent"));
    request << focusedEvent << QStringList() << m_bus->baseService();
    auto *watcher = new QDBusPendingCallWatcher(m_bus->asyncCall(request, requestTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *pending) {
        const bool failed = pending->isError();
        pending->deleteLater();
        if (failed) {
            ++m_focusGeneration;
            m_focusedEditableRect = {};
            if (m_bus) {
                m_bus->disconnect(QString(),
                                  QString(),
                                  QStringLiteral("org.a11y.atspi.Event.Object"),
                                  QStringLiteral("StateChanged"),
                                  this,
                                  SLOT(stateChanged(QString, int, int, QDBusVariant, QVariantMap, QDBusMessage)));
            }
            m_bus.reset();
        }
    });
}

FocusedEditableTracker::~FocusedEditableTracker()
{
    if (m_bus) {
        auto request = registryCall(QStringLiteral("DeregisterEvent"));
        request << focusedEvent << m_bus->baseService();
        // No introspection or waiting while the input method is being removed.
        m_bus->send(request);
        m_bus.reset();
    }
    if (!m_connectionName.isEmpty()) {
        QDBusConnection::disconnectFromBus(m_connectionName);
    }
}

bool FocusedEditableTracker::containsNormalizedTouch(const QPointF &position) const
{
    if (!m_focusedEditableRect.isValid()) {
        return false;
    }
    // AT-SPI reports global coordinates, so the touch has to be mapped onto the
    // screen the field is on, not blindly onto the primary one. When no screen
    // can be associated with the field the position is uncertain, and the
    // keyboard is shown rather than hidden from a field the user is holding.
    const QScreen *screen = QGuiApplication::screenAt(m_focusedEditableRect.center());
    if (!screen) {
        return true;
    }
    return m_focusedEditableRect.contains(mapTouchToScreen(position, screen).toPoint());
}

void FocusedEditableTracker::stateChanged(const QString &state, int enabled, int, const QDBusVariant &, const QVariantMap &, const QDBusMessage &message)
{
    if (state.compare(QStringLiteral("focused"), Qt::CaseInsensitive) != 0 || !m_bus) {
        return;
    }

    if (!enabled) {
        if (message.service() == m_focusedService && message.path() == m_focusedPath) {
            ++m_focusGeneration;
            m_focusedService.clear();
            m_focusedPath.clear();
            m_focusedEditableRect = {};
        }
        return;
    }

    // Invalidate immediately, including when the new object is non-editable or
    // fails to reply. Older replies must never restore another field's bounds.
    const quint64 generation = ++m_focusGeneration;
    m_focusedService = message.service();
    m_focusedPath = message.path();
    m_focusedEditableRect = {};
    const auto request =
        QDBusMessage::createMethodCall(m_focusedService, m_focusedPath, QStringLiteral("org.a11y.atspi.Accessible"), QStringLiteral("GetState"));
    auto *watcher = new QDBusPendingCallWatcher(m_bus->asyncCall(request, requestTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher *pending) {
        const QDBusMessage reply = pending->reply();
        pending->deleteLater();
        if (!m_bus || generation != m_focusGeneration || reply.type() != QDBusMessage::ReplyMessage || reply.signature() != QLatin1String("au")) {
            return;
        }
        QList<uint> states;
        const QDBusArgument argument = reply.arguments().constFirst().value<QDBusArgument>();
        argument >> states;
        if (!hasState(states, editableState)) {
            return;
        }

        auto request =
            QDBusMessage::createMethodCall(m_focusedService, m_focusedPath, QStringLiteral("org.a11y.atspi.Component"), QStringLiteral("GetExtents"));
        request << screenCoordinates;
        auto *extentsWatcher = new QDBusPendingCallWatcher(m_bus->asyncCall(request, requestTimeoutMs), this);
        connect(extentsWatcher, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher *pending) {
            const QDBusMessage reply = pending->reply();
            pending->deleteLater();
            if (!m_bus || generation != m_focusGeneration || reply.type() != QDBusMessage::ReplyMessage || reply.signature() != QLatin1String("(iiii)")) {
                return;
            }

            int x = 0;
            int y = 0;
            int width = 0;
            int height = 0;
            const QDBusArgument extents = reply.arguments().constFirst().value<QDBusArgument>();
            extents.beginStructure();
            extents >> x >> y >> width >> height;
            extents.endStructure();
            const QRect rectangle(x, y, width, height);
            if (rectangle.isValid()) {
                m_focusedEditableRect = rectangle;
                qCDebug(PlasmaKeyboard) << "Focused editable AT-SPI rectangle:" << rectangle;
            }
        });
    });
}
