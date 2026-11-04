/*
    SPDX-FileCopyrightText: 2026 Plasma Keyboard contributors

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "gamepadhandler.h"
#include "logging.h"
#include "plasmakeyboardsettings.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QTimer>
#include <Qt>

namespace
{
constexpr auto s_service = "org.shadowblip.InputPlumber";
constexpr auto s_interface = "org.shadowblip.Input.DBusDevice";
constexpr auto s_signal = "InputEvent";

// Turbo mode for the held backspace button: after this pause the deletion
// repeats every s_backspaceRepeatMs, like a held key on a hardware keyboard.
constexpr int s_backspaceInitialDelayMs = 350;
constexpr int s_backspaceRepeatMs = 50;

// Intercept modes of InputPlumber (see "inputplumber device intercept set").
// "none" leaves the gamepad to the system, i.e. to games and to Steam's
// mapping; "gamepad-only" routes it to us over D-Bus.
constexpr uint s_interceptNone = 0;
constexpr uint s_interceptGamepadOnly = 3;

QString propertyStringList(const QString &path, const QString &interface, const QString &property, const QString &signature)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(QLatin1String(s_service), path, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    msg << interface << property;
    const QDBusMessage reply = QDBusConnection::systemBus().call(msg);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return {};
    }
    const QVariant value = reply.arguments().first().value<QDBusVariant>().variant();
    if (signature == QLatin1String("as")) {
        return value.toStringList().value(0);
    }
    return {};
}

uint propertyUint(const QString &path, const QString &interface, const QString &property)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(QLatin1String(s_service), path, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    msg << interface << property;
    const QDBusMessage reply = QDBusConnection::systemBus().call(msg);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return 0;
    }
    return reply.arguments().first().value<QDBusVariant>().variant().toUInt();
}

// Find the active composite device and the object path of its "dbus" target.
QString findDbusDevicePath(QString *compositePathOut)
{
    const QString managerPath = QStringLiteral("/org/shadowblip/InputPlumber/Manager");
    QString compositePath =
        propertyStringList(managerPath, QStringLiteral("org.shadowblip.InputManager"), QStringLiteral("GamepadOrder"), QStringLiteral("as"));
    if (compositePath.isEmpty()) {
        compositePath = QStringLiteral("/org/shadowblip/InputPlumber/CompositeDevice0");
    }

    if (compositePathOut) {
        *compositePathOut = compositePath;
    }

    return propertyStringList(compositePath, QStringLiteral("org.shadowblip.Input.CompositeDevice"), QStringLiteral("DbusDevices"), QStringLiteral("as"));
}
} // namespace

GamepadHandler::GamepadHandler(QObject *parent)
    : QObject(parent)
{
    m_repeatTimer = new QTimer(this);
    m_repeatTimer->setInterval(180);
    connect(m_repeatTimer, &QTimer::timeout, this, [this] {
        if (m_repeatKey != 0) {
            Q_EMIT navigate(m_repeatKey);
        }
    });

    // X (backspace) keeps deleting while it is held; the first character is
    // removed right away, then repeats start after a short delay.
    m_backspaceTimer = new QTimer(this);
    m_backspaceTimer->setSingleShot(true);
    connect(m_backspaceTimer, &QTimer::timeout, this, [this] {
        if (!m_backspaceHeld) {
            return;
        }
        Q_EMIT backspace();
        m_backspaceTimer->start(s_backspaceRepeatMs);
    });

    // A tapped activates the highlighted key; held past the configured delay it
    // offers that key's alternate characters instead, and the list is then
    // walked with the directions.
    m_acceptHoldTimer = new QTimer(this);
    m_acceptHoldTimer->setSingleShot(true);
    connect(m_acceptHoldTimer, &QTimer::timeout, this, [this] {
        qCDebug(PlasmaKeyboard) << "GamepadHandler: A hold timer fired, held" << m_acceptHeld << "armed" << m_pressedAlternates;
        if (m_acceptHeld && !m_pressedAlternates.isEmpty()) {
            // This press has done its job, so its release must not type the key.
            m_acceptConsumed = true;
            m_alternatesOpened = true;
            qCDebug(PlasmaKeyboard) << "GamepadHandler: offering alternates" << m_pressedAlternates;
            Q_EMIT showAlternates(m_pressedAlternates);
        }
    });

    const QString path = findDbusDevicePath(&m_compositePath);
    qCDebug(PlasmaKeyboard) << "GamepadHandler: InputPlumber dbus target" << (path.isEmpty() ? QStringLiteral("<not found>") : path);
    if (path.isEmpty()) {
        return;
    }

    const bool connected = QDBusConnection::systemBus().connect(QLatin1String(s_service),
                                                                path,
                                                                QLatin1String(s_interface),
                                                                QLatin1String(s_signal),
                                                                this,
                                                                SLOT(onInputEvent(QString, double)));
    if (connected) {
        m_available = true;
        Q_EMIT availableChanged();
    }

    // Interception follows the actual state of the panel as reported by KWin,
    // which is more reliable than the window visibility of this process.
    QDBusConnection::sessionBus().connect(QStringLiteral("org.kde.KWin"),
                                          QStringLiteral("/VirtualKeyboard"),
                                          QStringLiteral("org.kde.kwin.VirtualKeyboard"),
                                          QStringLiteral("visibleChanged"),
                                          this,
                                          SLOT(refreshFromKWin()));
    QDBusConnection::sessionBus().connect(QStringLiteral("org.kde.KWin"),
                                          QStringLiteral("/VirtualKeyboard"),
                                          QStringLiteral("org.freedesktop.DBus.Properties"),
                                          QStringLiteral("PropertiesChanged"),
                                          this,
                                          SLOT(onKWinPropertiesChanged(QString, QVariantMap, QStringList)));
    refreshFromKWin();

    // Safety net in case the PropertiesChanged signal is not delivered.
    m_kwinPollTimer = new QTimer(this);
    m_kwinPollTimer->setInterval(1000);
    connect(m_kwinPollTimer, &QTimer::timeout, this, &GamepadHandler::refreshFromKWin);
    m_kwinPollTimer->start();
}

void GamepadHandler::onKWinPropertiesChanged(const QString &interfaceName, const QVariantMap &changed, const QStringList &invalidated)
{
    Q_UNUSED(invalidated);
    if (interfaceName != QLatin1String("org.kde.kwin.VirtualKeyboard")) {
        return;
    }
    if (changed.contains(QStringLiteral("visible"))) {
        setKwinVisible(changed.value(QStringLiteral("visible")).toBool());
    }
}

void GamepadHandler::refreshFromKWin()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                      QStringLiteral("/VirtualKeyboard"),
                                                      QStringLiteral("org.freedesktop.DBus.Properties"),
                                                      QStringLiteral("Get"));
    msg << QStringLiteral("org.kde.kwin.VirtualKeyboard") << QStringLiteral("visible");
    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return;
    }
    const bool visible = reply.arguments().first().value<QDBusVariant>().variant().toBool();

    // A previous instance may have died before it could give the gamepad back
    // (a crash, or a kill while the keyboard was up), which leaves the gamepad
    // intercepted forever: it would neither reach the game nor Steam again.
    if (!visible && !m_keyboardOnScreen && !m_active && interceptMode() == s_interceptGamepadOnly) {
        qCDebug(PlasmaKeyboard) << "GamepadHandler: giving back a gamepad left intercepted by an earlier instance";
        setInterceptMode(s_interceptNone);
    }

    setKwinVisible(visible);
}

void GamepadHandler::setKwinVisible(bool visible)
{
    m_kwinVisible = visible;
    recomputeActive();
}

bool GamepadHandler::keyboardOnScreen() const
{
    return m_keyboardOnScreen;
}

void GamepadHandler::setKeyboardOnScreen(bool onScreen)
{
    if (onScreen == m_keyboardOnScreen) {
        return;
    }
    m_keyboardOnScreen = onScreen;
    Q_EMIT keyboardOnScreenChanged();
    // The keyboard window can stay on screen after the text input went away, so
    // the compositor no longer reports it as visible: the gamepad keeps driving
    // the keyboard for as long as the window is there.
    recomputeActive();
}

void GamepadHandler::recomputeActive()
{
    setActive(m_kwinVisible || m_keyboardOnScreen);
}

GamepadHandler::~GamepadHandler()
{
    if (m_active) {
        setInterceptMode(m_savedInterceptMode);
    }
}

bool GamepadHandler::isAvailable() const
{
    return m_available;
}

bool GamepadHandler::isActive() const
{
    return m_active;
}

uint GamepadHandler::interceptMode() const
{
    if (m_compositePath.isEmpty()) {
        return 0;
    }
    return propertyUint(m_compositePath, QStringLiteral("org.shadowblip.Input.CompositeDevice"), QStringLiteral("InterceptMode"));
}

void GamepadHandler::setInterceptMode(uint mode)
{
    if (m_compositePath.isEmpty()) {
        return;
    }
    QDBusMessage msg =
        QDBusMessage::createMethodCall(QLatin1String(s_service), m_compositePath, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Set"));
    msg << QStringLiteral("org.shadowblip.Input.CompositeDevice") << QStringLiteral("InterceptMode")
        << QVariant::fromValue(QDBusVariant(QVariant::fromValue(mode)));
    QDBusConnection::systemBus().call(msg);
}

void GamepadHandler::setActive(bool active)
{
    if (!m_available || m_compositePath.isEmpty()) {
        return;
    }
    if (active == m_active) {
        return;
    }

    if (active) {
        m_savedInterceptMode = interceptMode();
        // Never remember an intercepted mode as the one to go back to: after a
        // crash that left the gamepad intercepted, that would keep it grabbed
        // for good.
        if (m_savedInterceptMode == s_interceptGamepadOnly) {
            m_savedInterceptMode = s_interceptNone;
        }
        // gamepad-only: gamepad input is routed over D-Bus and no longer
        // reaches the game (or Steam's mapping).
        setInterceptMode(s_interceptGamepadOnly);
    } else {
        setInterceptMode(m_savedInterceptMode);
        // No release events arrive once the gamepad is no longer intercepted.
        m_backspaceHeld = false;
        m_backspaceTimer->stop();
        m_repeatKey = 0;
        m_repeatTimer->stop();
        m_pressedDirections.clear();
    }
    m_active = active;
    qCDebug(PlasmaKeyboard) << "GamepadHandler::setActive" << active << "saved mode" << m_savedInterceptMode << "now" << interceptMode();
}

void GamepadHandler::onInputEvent(const QString &event, double value)
{
    // The gamepad only drives the keyboard while it is on screen: anything
    // arriving while it is hidden belongs to the system (a game, Steam).
    if (!m_active) {
        return;
    }

    qCDebug(PlasmaKeyboard) << "GamepadHandler: InputEvent" << event << value;
    const bool pressed = value >= 0.5;

    // InputPlumber's dbus target emits abstract UI actions, not capability
    // names (see src/input/event/dbus.rs in InputPlumber).
    if (event == QLatin1String("ui_up")) {
        handleDirection(Qt::Key_Up, pressed);
        return;
    }
    if (event == QLatin1String("ui_down")) {
        handleDirection(Qt::Key_Down, pressed);
        return;
    }
    if (event == QLatin1String("ui_left")) {
        handleDirection(Qt::Key_Left, pressed);
        return;
    }
    if (event == QLatin1String("ui_right")) {
        handleDirection(Qt::Key_Right, pressed);
        return;
    }

    // X: backspace. While held it works like turbo mode and keeps deleting.
    if (event == QLatin1String("ui_context")) {
        handleBackspace(pressed);
        return;
    }

    // A: activates the highlighted key, or offers its alternate characters
    // when it is held.
    if (event == QLatin1String("ui_accept")) {
        handleAccept(pressed);
        return;
    }

    // All remaining mappings trigger on press only.
    if (!pressed) {
        return;
    }

    if (event == QLatin1String("ui_back")) {
        // B: go back. The panel decides what that means where it stands: it
        // leaves the voice page, or closes the keyboard when there is none.
        Q_EMIT back();
    } else if (event == QLatin1String("ui_action")) {
        // Y: space.
        Q_EMIT space();
    } else if (event == QLatin1String("ui_l2")) {
        // LT: shift.
        Q_EMIT toggleShift();
    } else if (event == QLatin1String("ui_l1")) {
        // LB: symbols layer.
        Q_EMIT toggleSymbols();
    } else if (event == QLatin1String("ui_r1")) {
        // RB: switch layout.
        Q_EMIT switchLanguage();
    } else if (event == QLatin1String("ui_r2")) {
        // RT: Enter.
        Q_EMIT enter();
    } else if (event == QLatin1String("ui_select") || event == QLatin1String("ui_quick")) {
        // Select (and the QuickAccess button of handhelds): reach the rows above
        // the keyboard (clipboard entries, clear key, F1-F12), which are not
        // part of the key grid.
        Q_EMIT toggleExtraRows();
    } else if (event == QLatin1String("ui_option")) {
        // Start: close the keyboard.
        Q_EMIT hideKeyboard();
    }
}

void GamepadHandler::handleBackspace(bool pressed)
{
    if (pressed) {
        if (m_backspaceHeld) {
            return;
        }
        m_backspaceHeld = true;
        Q_EMIT backspace();
        m_backspaceTimer->start(s_backspaceInitialDelayMs);
    } else {
        m_backspaceHeld = false;
        m_backspaceTimer->stop();
    }
}

void GamepadHandler::handleAccept(bool pressed)
{
    if (pressed) {
        if (m_acceptHeld) {
            return;
        }
        m_acceptHeld = true;
        m_acceptConsumed = false;

        // A press while the list is already up means "take the highlighted
        // character": the release that opened it is long gone, and the list
        // stays on screen until something is picked or dismissed.
        if (m_alternatesOpened) {
            m_acceptConsumed = true;
            qCDebug(PlasmaKeyboard) << "GamepadHandler: A pressed while the alternates list is open, confirming";
            Q_EMIT confirmAlternates();
            return;
        }

        // Fix the list now: the panel keeps polling the highlight while the
        // button is down, and the key under it may change (or blink out for one
        // poll) before the delay is over.
        m_pressedAlternates = m_alternatesArmable ? m_alternates : QStringList();
        qCDebug(PlasmaKeyboard) << "GamepadHandler: A pressed, armable" << m_alternatesArmable << "alternates" << m_pressedAlternates;
        if (!m_pressedAlternates.isEmpty()) {
            m_acceptHoldTimer->start(PlasmaKeyboardSettings::self()->gamepadAlternatesThresholdMs());
        }
        return;
    }

    if (!m_acceptHeld) {
        return;
    }
    m_acceptHeld = false;
    m_acceptHoldTimer->stop();
    m_pressedAlternates.clear();

    // The press that is ending already opened the list or took a character, so
    // it must not type the highlighted key as well. Whether the list is still
    // up makes no difference here: the next press picks from it.
    const bool consumed = m_acceptConsumed;
    m_acceptConsumed = false;
    if (consumed) {
        return;
    }

    Q_EMIT activate();
}

void GamepadHandler::setAlternatesArmable(bool armable, const QStringList &alternates)
{
    m_alternatesArmable = armable && !alternates.isEmpty();
    m_alternates = alternates;

    // The highlight moved away from a key before the button was held long
    // enough: there is nothing to offer any more.
    //
    // The button being down keeps a running delay alive: the panel polls the
    // highlight, and a poll that finds no key must not cancel the delay that
    // was started for the characters fixed at press time.
    if (!m_alternatesArmable && !m_acceptHeld) {
        m_acceptHoldTimer->stop();
    }
}

void GamepadHandler::clearAlternatesOpen()
{
    m_alternatesOpened = false;
}

void GamepadHandler::handleDirection(int key, bool pressed)
{
    if (pressed) {
        if (!m_pressedDirections.contains(key)) {
            m_pressedDirections.insert(key);
            Q_EMIT navigate(key);
        }
        m_repeatKey = key;
        m_repeatTimer->start();
    } else {
        m_pressedDirections.remove(key);
        if (m_repeatKey == key) {
            m_repeatKey = 0;
            m_repeatTimer->stop();
        }
    }
}
