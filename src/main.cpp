/*
    SPDX-FileCopyrightText: 2024 Aleix Pol i Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2025 Kristen McWilliam <kristen@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "config-plasma-keyboard.h"
#include "inputlisteneritem.h"
#include "inputpanelintegration.h"
#include "layoutpathhelper.h"
#include "logging.h"
#include "plasmakeyboardsettings.h"
#include "restartwatcher.h"
#include "sessionlockstate.h"
#include "panelstubgeometry.h"
#include "keyboardroleprivacy.h"
#include "keyboardvisibilitystate.h"
#include "settingsreloader.h"
#include "sttmanager.h"
#include "systemlayoutsync.h"
#include "thememanager.h"
#include <plasma_keyboard_version.h>

#include <KAboutData>
#include <KConfig>
#include <KConfigGroup>
#include <KCrash>
#include <KGlobalAccel>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <LayerShellQt/Window>

#include <QAction>
#include <QCommandLineParser>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QPointer>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QRegion>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QVariantMap>
#include <QWindow>
#include <qpa/qwindowsysteminterface.h>
#include <QtWaylandClient/QWaylandClientExtensionTemplate>
#include <QtWaylandClient/private/qwaylandwindow_p.h>

#include <qwayland-kde-lockscreen-overlay-v1.h>

namespace
{
constexpr auto s_kwinService = "org.kde.KWin";
constexpr auto s_kwinPath = "/VirtualKeyboard";
constexpr auto s_kwinIface = "org.kde.kwin.VirtualKeyboard";
constexpr auto s_kwinPropertiesIface = "org.freedesktop.DBus.Properties";

int kwinMode()
{
    QDBusMessage msg =
        QDBusMessage::createMethodCall(QLatin1String(s_kwinService), QLatin1String(s_kwinPath), QLatin1String(s_kwinPropertiesIface), QStringLiteral("Get"));
    msg << QLatin1String(s_kwinIface) << QStringLiteral("mode");
    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
        return reply.arguments().first().value<QDBusVariant>().variant().toInt();
    }
    return -1;
}

void setKwinMode(int mode)
{
    QDBusMessage msg =
        QDBusMessage::createMethodCall(QLatin1String(s_kwinService), QLatin1String(s_kwinPath), QLatin1String(s_kwinPropertiesIface), QStringLiteral("Set"));
    msg << QLatin1String(s_kwinIface) << QStringLiteral("mode") << QVariant::fromValue(QDBusVariant(QVariant::fromValue(mode)));
    QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
}

bool kwinVisible()
{
    QDBusMessage msg =
        QDBusMessage::createMethodCall(QLatin1String(s_kwinService), QLatin1String(s_kwinPath), QLatin1String(s_kwinPropertiesIface), QStringLiteral("Get"));
    msg << QLatin1String(s_kwinIface) << QStringLiteral("visible");
    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
        return reply.arguments().first().value<QDBusVariant>().variant().toBool();
    }
    return false;
}

void activateKwinKeyboard()
{
    QDBusMessage msg =
        QDBusMessage::createMethodCall(QLatin1String(s_kwinService), QLatin1String(s_kwinPath), QLatin1String(s_kwinIface), QStringLiteral("forceActivate"));
    QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
}

// Runs JavaScript inside plasmashell (used to control the Plasma panels).
void evaluatePlasmaScript(const QString &script, bool blocking = false)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"),
                                                      QStringLiteral("/PlasmaShell"),
                                                      QStringLiteral("org.kde.PlasmaShell"),
                                                      QStringLiteral("evaluateScript"));
    msg << script;
    if (blocking) {
        QDBusConnection::sessionBus().call(msg, QDBus::Block, 1000);
    } else {
        QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
    }
}

// The virtual keyboard shrinks the windows it covers by itself, and KWin does
// not always give them their size back (KDE bug 459136): the configure event
// carrying the shrunken size is still on its way to the window when KWin asks
// the window to grow back, so the window stays short. The workaround is a small
// script inside KWin that keeps a snapshot of the geometry of the ordinary
// windows and restores it once the keyboard is gone.
//
// KWin loads scripts from a file, so the script is unpacked from the resources
// into the runtime directory. That is also the place a sandboxed build shares
// with the compositor, unlike the data directory of the package.
QString installWindowRestoreScript()
{
    QFile resource(QStringLiteral(":/plasma-keyboard/kwinkeyboardrestore.js"));
    if (!resource.open(QIODevice::ReadOnly)) {
        qCWarning(PlasmaKeyboard) << "Cannot read the bundled window geometry restore script";
        return QString();
    }
    const QByteArray contents = resource.readAll();
    resource.close();

    QString directory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (directory.isEmpty()) {
        directory = QDir::tempPath();
    }
    directory += QStringLiteral("/plasma-keyboard-custom");
    if (!QDir().mkpath(directory)) {
        qCWarning(PlasmaKeyboard) << "Cannot create" << directory << "for the window geometry restore script";
        return QString();
    }

    const QString path = directory + QStringLiteral("/kwinkeyboardrestore.js");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(contents) != contents.size()) {
        qCWarning(PlasmaKeyboard) << "Cannot write the window geometry restore script to" << path;
        return QString();
    }
    return path;
}

// Loads the window geometry restore script into KWin, or unloads it again.
void setWindowRestoreScript(const QString &path, bool enabled)
{
    const auto call = [&path](const QString &method) {
        QDBusMessage message =
            QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"), QStringLiteral("org.kde.kwin.Scripting"), method);
        message << path;
        return QDBusConnection::sessionBus().call(message, QDBus::Block, 1000);
    };

    // The script can still be loaded from a previous run of the keyboard: KWin
    // keeps it until it is asked to unload it, and loading it again would leave
    // two copies of it running.
    call(QStringLiteral("unloadScript"));
    if (!enabled) {
        return;
    }

    const QDBusMessage reply = call(QStringLiteral("loadScript"));
    if (reply.type() != QDBusMessage::ReplyMessage) {
        qCWarning(PlasmaKeyboard) << "Cannot load the window geometry restore script into KWin:" << reply.errorMessage();
        return;
    }
    qCDebug(PlasmaKeyboard) << "window geometry restore script loaded from" << path;
}

// Space the Plasma panels take on a screen, as reported by plasmashell. The
// compositor does not tell clients about the available area, and the keyboard
// needs it to stay above a panel that is left in place: such a panel keeps no
// reserved space of its own (see KeyboardHotkeyController::hidePanels()), so the
// keyboard has to reserve the panel's height as well and to lift itself by it.
QMargins plasmaPanelInsets(const QString &screenName, const QSize &screenSize)
{
    if (screenName.isEmpty()) {
        return QMargins();
    }
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"),
                                                      QStringLiteral("/StrutManager"),
                                                      QStringLiteral("org.kde.PlasmaShell.StrutManager"),
                                                      QStringLiteral("availableScreenRect"));
    msg << screenName;
    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 500);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return QMargins();
    }
    // The compositor answers with a plain (iiii) structure, which Qt does not
    // turn into a QRect on its own.
    const QVariant value = reply.arguments().constFirst();
    QRect rect = value.toRect();
    if (!rect.isValid() && value.canConvert<QDBusArgument>()) {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        const QDBusArgument arg = value.value<QDBusArgument>();
        arg.beginStructure();
        arg >> x >> y >> width >> height;
        arg.endStructure();
        rect = QRect(x, y, width, height);
    }
    if (!rect.isValid() || screenSize.isEmpty()) {
        return QMargins();
    }
    return QMargins(rect.x(), rect.y(), qMax(0, screenSize.width() - rect.x() - rect.width()), qMax(0, screenSize.height() - rect.y() - rect.height()));
}

// Panel containment ids mapped to their configured hiding mode, read from the
// Plasma config so the user's setting can be restored after hiding the panel.
QHash<int, QString> configuredPanelHidingModes()
{
    QHash<int, QString> modes;
    KConfig config(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/plasma-org.kde.plasma.desktop-appletsrc"),
                   KConfig::SimpleConfig);
    const KConfigGroup containments = config.group(QStringLiteral("Containments"));
    const QStringList ids = containments.groupList();
    for (const QString &id : ids) {
        const KConfigGroup containment = containments.group(id);
        if (containment.readEntry(QStringLiteral("plugin")) != QLatin1String("org.kde.panel")) {
            continue;
        }
        bool ok = false;
        const int panelId = id.toInt(&ok);
        if (ok) {
            modes.insert(panelId, containment.readEntry(QStringLiteral("hiding"), QStringLiteral("none")));
        }
    }
    return modes;
}

/**
 * Qt Virtual Keyboard shows the panel (and therefore enables the keys) only when
 * the window that holds the input item is the active window:
 * PlatformInputContext::evaluateInputPanelVisible() requires m_focusObject, and
 * the focus object is the active focus item of QGuiApplication::focusWindow().
 * The input-panel shell integration activates its window on its own (see
 * qwaylandinputpanelsurface.cpp); a layer-shell window is never activated by the
 * compositor on its own, so the activation is announced to Qt here. The
 * compositor keeps its own focus untouched, so the field being typed into does
 * not lose it.
 */
void activateKeyboardWindow(QWindow *window)
{
    if (!window) {
        return;
    }
    QWindowSystemInterface::handleFocusWindowChanged(window, Qt::ActiveWindowFocusReason);
}
} // namespace

/**
 * Marks the visible layer-shell surface as safe to render while the session is
 * locked. KWin otherwise hides every session surface except the lock screen and
 * input-panel surfaces. The keyboard's one-pixel panel stub is an input-panel
 * surface, but the window which actually draws the keys is layer-shell.
 */
class LockscreenOverlay : public QWaylandClientExtensionTemplate<LockscreenOverlay>, public QtWayland::kde_lockscreen_overlay_v1
{
public:
    LockscreenOverlay()
        : QWaylandClientExtensionTemplate<LockscreenOverlay>(1)
    {
    }

    bool allowWindow(QWindow *window)
    {
        if (!window || !isActive()) {
            return false;
        }

        // Creating the platform surface is safe here: the QML window is still
        // unmapped. The protocol rejects an already mapped surface.
        window->create();
        auto *waylandWindow = dynamic_cast<QtWaylandClient::QWaylandWindow *>(window->handle());
        if (!waylandWindow || !waylandWindow->shellSurface()) {
            return false;
        }

        allow(waylandWindow->wlSurface());
        return true;
    }
};

/**
 * Joins the two windows the keyboard consists of: the visible layer-shell window
 * that draws the keys and the invisible input-panel window that KWin keeps
 * managing (when to show the keyboard, the input mode, telling the panel state
 * over D-Bus and moving the focused window out of the way).
 *
 * KWin uses the input region of the panel window as its geometry, so the region
 * has to follow the visible panel while the keyboard is docked. The visible
 * keyboard itself is shown and hidden together with what KWin reports.
 */
class KeyboardWindowBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool kwinVisible READ kwinVisible NOTIFY kwinVisibleChanged)

public:
    explicit KeyboardWindowBridge(QObject *parent = nullptr)
        : QObject(parent)
        , m_lockState(*SessionLockState::instance())
    {
        connect(&m_lockState, &SessionLockState::stateChanged, this, [this] {
            applyStubMask();
            applyPanelLayout();
            syncStubVisibility();
            if (!m_lockState.isKnown() || m_lockState.isPreparingLock()) {
                m_outputRelocationTimer.stop();
                if (m_keyboard) {
                    m_keyboard->setProperty("plasmaKeyboardRelocateAfterDestroy", false);
                }
            }
        });
        connect(&m_lockState, &SessionLockState::unlocked, this, [this] {
            m_outputRelocationTimer.stop();
            if (m_keyboard) {
                m_keyboard->setProperty("plasmaKeyboardRelocateAfterDestroy", false);
            }
        });
        m_outputRelocationTimer.setSingleShot(true);
        m_outputRelocationTimer.setInterval(100);
        connect(&m_outputRelocationTimer, &QTimer::timeout, this, [this]() {
            if (!m_keyboard || !m_keyboard->isVisible() || !m_stub || m_lockState.isPreparingLock()
                || m_keyboard->screen() == m_stub->screen()) {
                return;
            }
            // A layer-shell role cannot change its output after creation.
            // Hide it once so the visibleChanged handler can recreate the role
            // on the compositor's active output, never an explicit Qt screen.
            m_keyboard->setProperty("plasmaKeyboardRelocateAfterDestroy", true);
            m_keyboard->setVisible(false);
        });
    }

    void setStubWindow(QQuickWindow *stub)
    {
        m_stub = stub;
        applyStubMask();
        // Real wl_surface enter/leave updates this screen asynchronously.
        // Do not pin the layer role to the stub's initial (primary) Qt screen.
        connect(stub, &QWindow::screenChanged, this, [this] {
            applyStubMask();
            syncKeyboardScreen();
        });
        connect(stub, &QWindow::widthChanged, this, &KeyboardWindowBridge::applyStubMask);
        connect(stub, &QWindow::heightChanged, this, &KeyboardWindowBridge::applyStubMask);
    }

    //! The layer-shell window of the keyboard, which carries the space the
    //! compositor reserves for the docked panel (see applyPanelLayout()).
    void setLayerShellWindow(LayerShellQt::Window *layerShell)
    {
        m_layerShell = layerShell;
        applyPanelLayout();
    }

    void setKeyboardWindow(QWindow *window)
    {
        m_keyboard = window;
        syncKeyboardScreen();
        connect(window, &QWindow::screenChanged, this, &KeyboardWindowBridge::syncKeyboardScreen);
        // KWin drives the keyboard through the panel stub and reports it as
        // visible only while that stub is mapped, so the stub follows the real
        // keyboard window (which Qt Virtual Keyboard shows and hides itself):
        // without this the compositor keeps the panel shown after the keyboard is
        // hidden, the gamepad mapping is not restored and the focused window keeps
        // the space reserved for the keyboard.
        connect(window, &QWindow::visibleChanged, this, &KeyboardWindowBridge::syncStubVisibility);
        // KWin stops reporting the keyboard as visible as soon as the text input
        // goes away, while the window itself stays mapped when the keyboard is
        // not hidden on focus loss: the Plasma panel has to follow the window as
        // well (see KeyboardHotkeyController::updatePanelVisibility()).
        connect(window, &QWindow::visibleChanged, this, &KeyboardWindowBridge::keyboardWindowVisibleChanged);
        // The keyboard belongs on screen only while it is asked for: Qt Virtual
        // Keyboard maps the window when it shows the panel and unmaps it when it
        // hides it. Mapping it here as well put the keyboard on screen (and made
        // the compositor reserve its space) right after the session started,
        // without anything having asked for it. The window is only brought to
        // the state the input method is in.
        window->setVisible(QGuiApplication::inputMethod()->isVisible());
        // Qt Virtual Keyboard needs the window with the input item to be active,
        // otherwise it keeps the panel (and its keys) disabled.
        if (window->isVisible()) {
            activateKeyboardWindow(window);
        }
        syncStubVisibility();
        qCDebug(PlasmaKeyboard) << "keyboard window registered, kwin visible" << m_kwinVisible << "active" << window->isActive() << "focus window"
                                << QGuiApplication::focusWindow() << "focus object" << QGuiApplication::focusObject();
    }

    //! The rectangle of the visible panel, in the coordinates of the screen.
    Q_INVOKABLE void setPanelRect(const QRect &rect)
    {
        if (rect == m_panelRect) {
            return;
        }
        m_panelRect = rect;
        applyStubMask();
        applyPanelLayout();
    }

    //! Re-applies the panel geometry to the layer-shell window: the keyboard mode
    //! changes the anchors and the space the compositor reserves for the panel.
    Q_INVOKABLE void updatePanelLayout()
    {
        applyStubMask();
        applyPanelLayout();
    }

    //! Re-reads the space the Plasma panels take on the keyboard's screen. Needed
    //! while a panel is left in place: the keyboard then has to reserve that
    //! space as well and to keep above the panel (see applyPanelLayout()).
    void refreshPanelInsets()
    {
        if (!m_keyboard || !m_keyboard->screen()) {
            m_panelInsets = QMargins();
            return;
        }
        m_panelInsets = plasmaPanelInsets(m_keyboard->screen()->name(), m_keyboard->screen()->geometry().size());
        qCDebug(PlasmaKeyboard) << "Plasma panel insets on" << m_keyboard->screen()->name() << m_panelInsets;
    }

    //! Whether the Plasma panel sits at the bottom of the screen. Only then can
    //! the keyboard be lifted above it; a panel at the top or on the side keeps
    //! its own behaviour (see applyPanelLayout()).
    bool panelAtBottom() const
    {
        return m_panelInsets.bottom() > 0 && m_panelInsets.top() == 0;
    }

    //! Tells Qt that the keyboard window is the window the input goes to. Qt
    //! Virtual Keyboard only shows the panel while that is the case, and the
    //! compositor never activates a layer-shell window on its own, so a show
    //! asked for by the keyboard itself (the shortcut) needs this first.
    void activateWindow()
    {
        activateKeyboardWindow(m_keyboard);
    }

    //! Whether KWin considers the virtual keyboard to be on screen.
    void setKwinVisible(bool visible)
    {
        if (visible == m_kwinVisible) {
            return;
        }
        m_kwinVisible = visible;
        qCDebug(PlasmaKeyboard) << "KWin reports the keyboard visible:" << visible;
        if (visible) {
            // Qt Virtual Keyboard only enables the keys while the window holding
            // the input item is active, and a layer-shell window is not activated
            // by the compositor: announce the activation to Qt as soon as the
            // keyboard is on screen.
            activateKeyboardWindow(m_keyboard);
        }
        Q_EMIT kwinVisibleChanged();
    }

    bool kwinVisible() const
    {
        return m_kwinVisible;
    }

    //! Whether the keyboard window itself is mapped. KWin reports the keyboard
    //! as hidden as soon as the text input goes away, but with
    //! hideOnInputFocusLoss off the window stays on screen and keeps the space
    //! it reserves, so the Plasma panel has to follow the window as well (see
    //! KeyboardHotkeyController::updatePanelVisibility()).
    bool keyboardWindowVisible() const
    {
        return m_keyboard && m_keyboard->isVisible();
    }

    //! The compositor re-reserves the space for the panel on its own: the docked
    //! keyboard asks for it with the layer-shell exclusive zone of its window, so
    //! switching modes only has to re-apply that zone (see applyPanelLayout()).
Q_SIGNALS:
    void kwinVisibleChanged();
    void keyboardWindowVisibleChanged();

private:
    //! Desktop layout uses the layer-shell exclusive zone. The full-screen
    //! lock view instead needs the input-panel rectangle; advertise it through
    //! two corner pixels, never a filled input region that would eat touches.
    void applyStubMask()
    {
        if (!m_stub) {
            return;
        }
        if (const auto *screen = m_stub->screen(); screen && m_stub->size() != screen->geometry().size()) {
            m_stub->resize(screen->geometry().size());
        }
        const bool locked = m_lockState.isKnown() && m_lockState.isLocked() && QGuiApplication::inputMethod()->isVisible();
        m_stub->setMask(panelStubInputRegion(m_panelRect, m_stub->size(), locked, !PlasmaKeyboardSettings::self()->floatingKeyboard()));
        // During resize KWin clips the region to the committed buffer; its
        // sizeChanged path repositions the panel when the new buffer arrives.
        // Qt Wayland attaches the input region to the next surface commit.
        m_stub->requestUpdate();
    }

    //! Tells the compositor how much space the keyboard needs. The docked panel is
    //! anchored to the bottom and reserves its height there, so the compositor
    //! moves the focused window out of the way by itself; the floating panel
    //! reserves nothing.
    void applyPanelLayout()
    {
        if (!m_layerShell) {
            return;
        }
        const QSize screen = m_keyboard && m_keyboard->screen() ? m_keyboard->screen()->geometry().size() : QSize(1280, 800);
        LayerShellQt::Window::Anchors anchors = LayerShellQt::Window::AnchorLeft;
        // The panel is left in place when the user asked for it: it then keeps no
        // reserved space of its own, so the keyboard has to keep above it (see
        // hidePanels() and refreshPanelInsets()).
        const bool panelStays = !PlasmaKeyboardSettings::self()->hidePanelWhenKeyboardVisible();
        // Only a panel at the bottom can be covered by the keyboard; it is also
        // the only case where the keyboard has to be lifted by the panel height.
        const bool liftOverPanel = panelStays && panelAtBottom();
        if (PlasmaKeyboardSettings::self()->floatingKeyboard()) {
            anchors |= LayerShellQt::Window::AnchorTop;
            m_layerShell->setDesiredSize(screen);
            m_layerShell->setExclusiveZone(0);
            m_layerShell->setMargins(QMargins());
        } else {
            anchors |= LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorBottom | LayerShellQt::Window::AnchorRight);
            m_layerShell->setDesiredSize(QSize(0, screen.height()));
            // The space at the bottom is reserved so that the compositor moves
            // the focused window out of the way and the keyboard does not float
            // over it. While the Plasma panel is hidden together with the
            // keyboard that space is the keyboard alone; when the panel stays it
            // reserves nothing itself, so the keyboard is lifted above it with
            // the bottom margin (which the compositor counts as reserved space
            // as well).
            const int panelHeight = m_panelRect.isValid() ? m_panelRect.height() : 0;
            m_layerShell->setExclusiveZone(m_lockState.allowsPredictions() ? panelHeight : 0);
            m_layerShell->setMargins(QMargins(0, 0, 0, liftOverPanel ? m_panelInsets.bottom() : 0));
        }
        m_layerShell->setAnchors(anchors);
        // Layer-shell properties only reach the compositor with the next commit
        // of the surface, so ask for one: without it the reserved space changed
        // by a setting (or by a panel that changed while the keyboard was on
        // screen) would only take effect on the next show.
        if (m_keyboard) {
            m_keyboard->requestUpdate();
        }
    }

    //! Follow actual surface output events without overriding active-output
    //! selection. A mapped immutable role is recreated only after a real move.
    void syncKeyboardScreen()
    {
        if (!m_stub || !m_keyboard || !m_stub->screen()) {
            return;
        }
        applyStubMask();
        applyPanelLayout();
        if (m_keyboard->screen() == m_stub->screen()) {
            m_outputRelocationTimer.stop();
        } else if (m_keyboard->isVisible() && !m_lockState.isPreparingLock()) {
            // screenChanged can briefly report intermediate outputs while KWin
            // moves the panel. Wait for it to settle before recreating the
            // immutable layer-shell role.
            m_outputRelocationTimer.start();
        }
    }

    //! Keep the lock-screen input-panel transport alive through compositor
    //! hide/show. Unmapping it on hide prevents KWin's next show from mapping it.
    void syncStubVisibility()
    {
        if (!m_stub) {
            return;
        }
        applyStubMask();
        const bool visible = (m_keyboard && m_keyboard->isVisible()) || (m_lockState.isKnown() && m_lockState.isLocked());
        if (m_stub->isVisible() != visible) {
            qCDebug(PlasmaKeyboard) << "panel stub mapped:" << visible;
            m_stub->setVisible(visible);
        }
    }

    SessionLockState &m_lockState;
    QPointer<QQuickWindow> m_stub;
    QPointer<QWindow> m_keyboard;
    QPointer<LayerShellQt::Window> m_layerShell;
    QTimer m_outputRelocationTimer;
    QRect m_panelRect;
    QMargins m_panelInsets;
    bool m_kwinVisible = false;
};

/**
 * Shows the keyboard when the global shortcut is pressed. KWin only shows the
 * panel for the configured input mode, so temporarily switch to AnyInput and
 * restore the previous mode once the panel is hidden again.
 */
class KeyboardHotkeyController : public QObject
{
    Q_OBJECT
public:
    explicit KeyboardHotkeyController(KeyboardWindowBridge *bridge, QObject *parent = nullptr)
        : QObject(parent)
        , m_bridge(bridge)
    {
        m_panelHidingModes = configuredPanelHidingModes();

        QDBusConnection::sessionBus().connect(QLatin1String(s_kwinService),
                                              QLatin1String(s_kwinPath),
                                              QLatin1String(s_kwinIface),
                                              QStringLiteral("visibleChanged"),
                                              this,
                                              SLOT(restoreModeIfHidden()));
        QDBusConnection::sessionBus().connect(QLatin1String(s_kwinService),
                                              QLatin1String(s_kwinPath),
                                              QLatin1String(s_kwinIface),
                                              QStringLiteral("visibleChanged"),
                                              this,
                                              SLOT(updatePanelVisibility()));
        QDBusConnection::sessionBus().connect(QLatin1String(s_kwinService),
                                              QLatin1String(s_kwinPath),
                                              QLatin1String(s_kwinPropertiesIface),
                                              QStringLiteral("PropertiesChanged"),
                                              this,
                                              SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));

        // The visible keyboard window can outlive what KWin reports for it (see
        // updatePanelVisibility()), so the panel state is re-evaluated as soon
        // as the window is mapped or unmapped, without waiting for the poll.
        connect(m_bridge, &KeyboardWindowBridge::keyboardWindowVisibleChanged, this, &KeyboardHotkeyController::updatePanelVisibility);

        // The PropertiesChanged signal is not always delivered; poll instead.
        auto *pollTimer = new QTimer(this);
        pollTimer->setInterval(1000);
        connect(pollTimer, &QTimer::timeout, this, &KeyboardHotkeyController::restoreModeIfHidden);
        connect(pollTimer, &QTimer::timeout, this, &KeyboardHotkeyController::updatePanelVisibility);
        pollTimer->start();

        // Apply the configured input mode on startup and whenever the settings
        // are re-read from disk. KConfigWatcher does not deliver the change in
        // this application, so SettingsReloader watches the file itself.
        connect(&m_settingsReloader, &SettingsReloader::settingsReloaded, this, [this] {
            applyConfiguredMode();
            // The panel mode and the reserved space depend on the settings, so
            // put the panel back and let updatePanelVisibility() apply the mode
            // the settings ask for.
            restorePanels();
            m_bridge->updatePanelLayout();
            updatePanelVisibility();
        });
        // A local hide key must also hide the compositor-owned panel. Keep its
        // transport mapped so the greeter can show it again without a new
        // activation. Never classify a compositor hide as a local user action.
        connect(QGuiApplication::inputMethod(), &QInputMethod::visibleChanged, this, [this] {
            const auto *lock = SessionLockState::instance();
            if (!m_applyingCompositorVisibility && !QGuiApplication::inputMethod()->isVisible()
                && lock->isKnown() && lock->isLocked() && !lock->isPreparingLock() && m_bridge && m_bridge->kwinVisible()) {
                // KWin exposes no hide method. Mode Never invokes hide(); the normal
                // visible=false handler restores the configured mode afterwards.
                setKwinMode(0);
            }
        });
        connect(SessionLockState::instance(), &SessionLockState::unlocked, this, [this] {
            m_visibility.afterUnlock();
        });
        connect(SessionLockState::instance(), &SessionLockState::stateChanged, this, &KeyboardHotkeyController::updatePanelVisibility);
        applyConfiguredMode();
        updatePanelVisibility();

        // Never leave the panel hidden behind us if we are killed.
        connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
            restorePanels(true);
        });
    }

public Q_SLOTS:
    void showKeyboard()
    {
        qCDebug(PlasmaKeyboard) << "Show-virtual-keyboard shortcut triggered";
        // Also when only this side has it shown: with hideOnInputFocusLoss off
        // the keyboard can stay up after KWin already considers it hidden.
        if (kwinVisible() || QGuiApplication::inputMethod()->isVisible()) {
            // Toggle: hide the keyboard if it is currently shown.
            QGuiApplication::inputMethod()->hide();
            return;
        }
        // AnyInput so the panel is shown regardless of the last input device.
        m_bridge->activateWindow();
        setInputPanelForceShowOnNextActivation();
        setKwinMode(2);
        activateKwinKeyboard();
    }

    // Restore the configured mode once the panel is hidden.
    void restoreModeIfHidden()
    {
        if (!kwinVisible()) {
            applyConfiguredMode();
        }
    }

    // Hide the Plasma panel(s) while the keyboard is visible, so the keyboard
    // reaches the bottom of the screen, and restore them afterwards.
    void updatePanelVisibility()
    {
        const bool compositorVisible = kwinVisible();
        const auto *lock = SessionLockState::instance();
        const auto visibilityChange = m_visibility.update(compositorVisible, lock->isKnown(), lock->isLocked(), lock->isPreparingLock(),
                                                         PlasmaKeyboardSettings::self()->hideOnInputFocusLoss());
        const bool keyboardVisible = m_visibility.compositorVisible();
        // KWin stops treating the keyboard as shown as soon as the text input
        // goes away, but with hideOnInputFocusLoss off the visible window stays
        // mapped and keeps the space it reserves. The Plasma panel has to follow
        // the window rather than the compositor report: a panel that is put back
        // while that space is still reserved has nowhere to sit at the bottom, so
        // it ends up right above the keyboard instead of below it.
        const bool keyboardOnScreen = keyboardVisible || (m_bridge && m_bridge->keyboardWindowVisible());

        // The visible keyboard window follows what the compositor reports: KWin
        // still decides when the keyboard is on screen (input mode and focus),
        // it just does it through the invisible panel window.
        if (m_bridge) {
            m_bridge->setKwinVisible(keyboardVisible);
        }
        qCDebug(PlasmaKeyboard) << "keyboard visible" << keyboardVisible << "keyboard on screen" << keyboardOnScreen << "input method visible"
                                << QGuiApplication::inputMethod()->isVisible();

        // Only edges are commands: a repeated poll must not undo a local hide.
        // During lock, compositor hide is authoritative regardless of the
        // desktop focus-loss preference. The mapped stub survives this hide.
        if (visibilityChange) {
            m_applyingCompositorVisibility = true;
            QGuiApplication::inputMethod()->setVisible(*visibilityChange);
            m_applyingCompositorVisibility = false;
        }

        // The floating keyboard does not reach the bottom of the screen, so the
        // Plasma panel is left alone while it is on. A panel that is not at the
        // bottom is left alone as well: the keyboard does not cover it, so it
        // keeps reserving its own space.
        const bool floating = PlasmaKeyboardSettings::self()->floatingKeyboard();
        const bool hidePanel = PlasmaKeyboardSettings::self()->hidePanelWhenKeyboardVisible();
        if (keyboardOnScreen != m_lastPanelLayoutVisible) {
            m_lastPanelLayoutVisible = keyboardOnScreen;
            if (!floating && keyboardOnScreen) {
                // Read the panel space before the panel is left in place: it then
                // keeps no reserved space of its own (see hidePanels()).
                m_bridge->refreshPanelInsets();
            }
            m_bridge->updatePanelLayout();
        }
        if (!floating && keyboardOnScreen && (hidePanel || m_bridge->panelAtBottom())) {
            hidePanels();
        } else {
            restorePanels();
        }
    }

private Q_SLOTS:
    void onPropertiesChanged(const QString &interfaceName, const QVariantMap &changed, const QStringList &invalidated)
    {
        Q_UNUSED(invalidated);
        if (interfaceName != QLatin1String(s_kwinIface)) {
            return;
        }
        if (changed.contains(QStringLiteral("visible")) && !changed.value(QStringLiteral("visible")).toBool()) {
            applyConfiguredMode();
        }
    }

private:
    // NonMouseInput (1) unless the user wants the keyboard also on mouse focus.
    static int configuredMode()
    {
        return PlasmaKeyboardSettings::self()->showOnMouseFocus() ? 2 : 1;
    }

    void applyConfiguredMode()
    {
        if (kwinVisible()) {
            return;
        }
        if (kwinMode() != configuredMode()) {
            setKwinMode(configuredMode());
        }
    }

    void hidePanels()
    {
        if (m_panelsHidden) {
            return;
        }
        m_panelsHidden = true;
        // Hidden (autohide) when the user asked for it; otherwise the panel is
        // left at the bottom of the screen without reserved space of its own
        // ("windows go below"), so that the keyboard can sit above it instead of
        // pushing it up.
        const QString mode = PlasmaKeyboardSettings::self()->hidePanelWhenKeyboardVisible() ? QStringLiteral("autohide") : QStringLiteral("windowsgobelow");
        evaluatePlasmaScript(QStringLiteral("panelIds.forEach(function(id){var p=panelById(id);if(p.hiding!==\"%1\"){p.hiding=\"%1\";}});").arg(mode));
    }

    void restorePanels(bool blocking = false)
    {
        if (!m_panelsHidden) {
            return;
        }
        m_panelsHidden = false;
        QJsonObject modes;
        for (auto it = m_panelHidingModes.constBegin(); it != m_panelHidingModes.constEnd(); ++it) {
            modes.insert(QString::number(it.key()), it.value());
        }
        const QString modesJson = QString::fromUtf8(QJsonDocument(modes).toJson(QJsonDocument::Compact));
        evaluatePlasmaScript(
            QStringLiteral("var m=%1;panelIds.forEach(function(id){var p=panelById(id);p.hiding=(m[id]!==undefined?m[id]:\"none\");});").arg(modesJson),
            blocking);
    }

    SettingsReloader m_settingsReloader;
    QPointer<KeyboardWindowBridge> m_bridge;
    QHash<int, QString> m_panelHidingModes;
    bool m_panelsHidden = false;
    KeyboardVisibilityState m_visibility;
    bool m_applyingCompositorVisibility = false;
    bool m_lastPanelLayoutVisible = false;
};

// signal handler for SIGINT & SIGTERM
#ifdef Q_OS_UNIX
#include <KSignalHandler>
#include <signal.h>
#include <unistd.h>
#endif

int main(int argc, char **argv)
{
    // Helper spawned detached by RestartWatcher after a package update: it only
    // toggles KWin's input method setting and exits. Handled before anything
    // else, so that it neither connects to Wayland nor takes the instance lock.
    if (argc > 1 && qstrcmp(argv[1], "--restart-input-method") == 0) {
        return restartInputMethod();
    }
    // Long-lived watchdog spawned below, before the gamepad/theme setup, so it
    // neither connects to Wayland nor takes the single instance lock.
    if (argc > 1 && qstrcmp(argv[1], "--watchdog") == 0) {
        return runInputMethodWatchdog();
    }

    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));

    initLayoutsPath();

    QGuiApplication application(argc, argv);

    // Only one instance may act as the input method; a stale instance would
    // keep its own (possibly shown) panel around and confuse the compositor.
    QLockFile instanceLock(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + QStringLiteral("/plasma-keyboard-custom.lock"));
    instanceLock.setStaleLockTime(0);
    if (!instanceLock.tryLock(0)) {
        qWarning() << "Another Plasma Keyboard instance is already running, exiting.";
        return 1;
    }

    // Keep a detached watchdog around. KWin does not start the input method
    // again when it dies, which would take the global shortcut with it; the
    // watchdog outlives this process and asks KWin for a replacement.
    QProcess::startDetached(QCoreApplication::applicationFilePath(), {QStringLiteral("--watchdog")});

    KLocalizedString::setApplicationDomain("plasma-keyboard");

    KAboutData aboutData(QStringLiteral("plasma-keyboard-custom"),
                         i18n("Plasma Keyboard"),
                         QStringLiteral(PLASMA_KEYBOARD_VERSION_STRING),
                         i18n("An on-screen keyboard for Plasma"),
                         KAboutLicense::GPL,
                         i18n("Copyright 2024, Aleix Pol Gonzalez"));

    aboutData.addAuthor(i18n("Aleix Pol Gonzalez"), i18n("Author"), QStringLiteral("aleixpol@kde.org"));
    aboutData.setOrganizationDomain("kde.org");
    aboutData.setDesktopFileName(QStringLiteral("org.kde.plasma.keyboard.custom"));
    application.setWindowIcon(QIcon::fromTheme(QStringLiteral("input-keyboard-virtual")));
    aboutData.setProgramLogo(application.windowIcon());

    KAboutData::setApplicationData(aboutData);

    // Global shortcut to open the keyboard. It is configurable in
    // System Settings -> Shortcuts -> Plasma Keyboard (custom).
    auto *showAction = new QAction(&application);
    showAction->setObjectName(QStringLiteral("show-virtual-keyboard"));
    showAction->setText(i18n("Show Virtual Keyboard"));
    showAction->setProperty("componentName", QStringLiteral("org.kde.plasma.keyboard.custom"));
    showAction->setProperty("componentDisplayName", i18n("Plasma Keyboard (custom)"));
    const QList<QKeySequence> defaultShortcut{QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_K)};
    KGlobalAccel::self()->setDefaultShortcut(showAction, defaultShortcut, KGlobalAccel::NoAutoloading);
    KGlobalAccel::self()->setShortcut(showAction, defaultShortcut, KGlobalAccel::NoAutoloading);
    auto *keyboardWindowBridge = new KeyboardWindowBridge(&application);
    auto *hotkeyController = new KeyboardHotkeyController(keyboardWindowBridge, &application);
    QObject::connect(showAction, &QAction::triggered, hotkeyController, &KeyboardHotkeyController::showKeyboard);

    KCrash::initialize();

    {
        QCommandLineParser parser;
        aboutData.setupCommandLine(&parser);
        parser.process(application);
        aboutData.processCommandLine(&parser);
    }

    if (!PLASMA_KEYBOARD_SOUND_ENABLED) {
        PlasmaKeyboardSettings::self()->setSoundEnabled(false);
    }

    if (!PLASMA_KEYBOARD_VIBRATION_ENABLED) {
        PlasmaKeyboardSettings::self()->setVibrationEnabled(false);
    }

    // Settings changed on disk are re-read by the settings reloader, which also
    // re-reads the configuration file itself (see SettingsReloader).

    // Expose the Ctrl/Alt latch state to the keyboard layouts.
    qmlRegisterSingletonInstance("org.kde.plasma.keyboard.custom.lib", 1, 0, "Modifiers", KeyboardModifiers::instance());

    // User themes (import/export/remove and the list of built-in themes).
    qmlRegisterSingletonInstance("org.kde.plasma.keyboard.custom.lib", 1, 0, "ThemeManager", ThemeManager::instance());

    // The visible keyboard window (layer-shell) and the panel window KWin keeps
    // managing.
    qmlRegisterSingletonInstance("org.kde.plasma.keyboard.custom.lib", 1, 0, "KeyboardWindow", keyboardWindowBridge);

    // Local speech recognition (voice input). It stays inert until it is enabled
    // in the settings; the keyboard only shows its microphone key when it is.
    auto *sttManager = new SttManager(&application);
    qmlRegisterSingletonInstance("org.kde.plasma.keyboard.custom.lib", 1, 0, "Stt", sttManager);

    // Keeps the system keyboard layout in step with the keyboard's language.
    qmlRegisterSingletonInstance("org.kde.plasma.keyboard.custom.lib", 1, 0, "SystemLayout", new SystemLayoutSync(&application));

    // KWin still drives the virtual keyboard through the input-panel window: it
    // decides when the keyboard belongs on screen (input mode, focus), reports
    // its state over D-Bus and moves the focused window out of its way. The
    // visible keyboard now lives in a layer-shell window, so this panel window
    // stays invisible and only carries the geometry of the panel: KWin uses its
    // input region as the keyboard rectangle.
    QQuickWindow panelStub;
    panelStub.setFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    panelStub.setColor(Qt::transparent);
    if (auto *screen = application.primaryScreen()) {
        panelStub.resize(screen->geometry().size());
    }
    panelStub.setMask(QRegion(0, 0, 1, 1));
    if (!initInputPanelIntegration(&panelStub, InputPanelRole::Keyboard)) {
        qCCritical(PlasmaKeyboard)
            << "Cannot run plasma-keyboard-custom standalone. You can enable it in Plasma's System Settings app, on the “Virtual Keyboard” page.";
        return 1;
    }
    panelStub.setVisible(true);
    keyboardWindowBridge->setStubWindow(&panelStub);

    QQmlApplicationEngine view;
    auto *lockscreenOverlay = new LockscreenOverlay;
    // Let the manager read the effective palette from the QML theme layer
    // (used by exportTheme).
    ThemeManager::instance()->setQmlEngine(&view);
    KLocalization::setupLocalizedContext(&view);

    QObject::connect(&view, &QQmlApplicationEngine::objectCreated, &application, [keyboardWindowBridge, lockscreenOverlay](QObject *object) {
        auto window = qobject_cast<QWindow *>(object);
        if (!window) {
            return;
        }

        // The keyboard itself is a layer-shell surface in the overlay layer, so
        // the compositor keeps it above other windows, and its position is up to
        // the keyboard (KWin does not reposition layer surfaces).
        auto *layerShell = LayerShellQt::Window::get(window);
        layerShell->setLayer(LayerShellQt::Window::LayerOverlay);
        // The window keeps a fixed size (the screen) and is anchored to its top
        // left corner: stretching it to the whole screen makes the compositor
        // send a new size whenever the available area changes (for example while
        // the Plasma panel hides), and the panel would move with it.
        // The anchors, the size and the reserved space follow the keyboard mode
        // (see KeyboardWindowBridge::applyPanelLayout()).
        LayerShellQt::Window::Anchors anchors = LayerShellQt::Window::AnchorTop;
        anchors |= LayerShellQt::Window::AnchorLeft;
        layerShell->setAnchors(anchors);
        layerShell->setDesiredSize(window->screen() ? window->screen()->geometry().size() : QSize(1280, 800));
        // The keyboard must not take the focus away from the field it types
        // into, so the compositor is not asked to activate the window; Qt is told
        // about the activation instead (see activateKeyboardWindow()).
        layerShell->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        layerShell->setExclusiveZone(0);
        layerShell->setScope(QStringLiteral("plasma-keyboard"));
        // The keyboard must not take the focus away from the field it types into.
        // Both calls need a newer layer-shell-qt than the one the package is
        // built against for older systems, so they are guarded by the version.
#ifdef HAVE_LAYERSHELLQT_ACTIVATE_ON_SHOW
        layerShell->setActivateOnShow(false);
#endif
#ifdef HAVE_LAYERSHELLQT_WANTS_ACTIVE_SCREEN
        layerShell->setWantsToBeOnActiveScreen(true);
#endif

        const auto registerKeyboardWindow = [keyboardWindowBridge, layerShell, lockscreenOverlay, window](bool authorizeLockscreen) {
            if (window->property("plasmaKeyboardBridgeRegistered").toBool()) {
                return;
            }
            window->setProperty("plasmaKeyboardBridgeRegistered", true);

            if (!authorizeLockscreen) {
                qCWarning(PlasmaKeyboard) << "The compositor does not provide the lock-screen overlay protocol";
            }

            const auto authorizeKeyboardRole = [lockscreenOverlay, window]() {
                if (!window->isVisible()) {
                    return;
                }
                const auto *lock = SessionLockState::instance();
                const bool locked = lock->isKnown() && lock->isLocked();
                const QVariant oldRole = window->property("plasmaKeyboardRoleLocked");
                const auto action = keyboardRoleAction(oldRole.isValid() ? std::optional<bool>(oldRole.toBool()) : std::nullopt,
                                                       lock->isKnown(), lock->isLocked(), window->property("plasmaKeyboardRoleAuthorized").toBool(),
                                                       window->property("plasmaKeyboardRoleAuthorizationAttempted").toBool());
                if (action == KeyboardRoleAction::Recreate) {
                    // Never authorize a desktop surface whose last buffer may
                    // contain clipboard data. Recreate a clean role after the
                    // privacy bindings have removed all auxiliary UI.
                    window->setProperty("plasmaKeyboardRelocateAfterDestroy", true);
                    window->setVisible(false);
                    return;
                }
                window->setProperty("plasmaKeyboardRoleLocked", locked);
                if (action != KeyboardRoleAction::AuthorizeRestricted) {
                    return;
                }
                window->setProperty("plasmaKeyboardRoleAuthorizationAttempted", true);
                if (lockscreenOverlay->allowWindow(window)) {
                    window->setProperty("plasmaKeyboardRoleAuthorized", true);
                    qCDebug(PlasmaKeyboard) << "Authorized a restricted keyboard role above the lock screen";
                } else {
                    qCWarning(PlasmaKeyboard) << "Could not authorize the keyboard role above the lock screen";
                }
            };
            QObject::connect(SessionLockState::instance(), &SessionLockState::stateChanged, window, authorizeKeyboardRole);

            // InputListenerItem handles QInputMethod::visibleChanged first and
            // completes QWindow::show() synchronously. This callback therefore
            // runs after LayerShellQt has created the role but before control
            // returns to the event loop and KWin can map its first buffer.
            QObject::connect(QGuiApplication::inputMethod(),
                             &QInputMethod::visibleChanged,
                             window,
                             [authorizeKeyboardRole]() {
                                 if (QGuiApplication::inputMethod()->isVisible()) {
                                     authorizeKeyboardRole();
                                 }
                             });

            QObject::connect(window, &QWindow::visibleChanged, window, [window, authorizeKeyboardRole](bool visible) {
                // visibleChanged(true) precedes platformWindow->setVisible(true):
                // the Wayland shell role is not ready yet. Authorize only from
                // the post-show QInputMethod callback or after the recreate.
                if (!visible) {
                    // A layer-shell role cannot migrate to another output. Drop
                    // the native surface after every hide so that the next show
                    // creates a new role on KWin's then-active output. Deferring
                    // this also lets Qt finish its hide/unmap path first.
                    QTimer::singleShot(0, window, [window, authorizeKeyboardRole]() {
                        if (!window->isVisible()) {
                            const bool relocate = window->property("plasmaKeyboardRelocateAfterDestroy").toBool();
                            window->setProperty("plasmaKeyboardRelocateAfterDestroy", false);
                            window->destroy();
                            window->setProperty("plasmaKeyboardRoleLocked", QVariant());
                            window->setProperty("plasmaKeyboardRoleAuthorized", QVariant());
                            window->setProperty("plasmaKeyboardRoleAuthorizationAttempted", QVariant());
                            qCDebug(PlasmaKeyboard) << "Released the hidden keyboard role so the next show can follow the active output";
                            if (relocate && QGuiApplication::inputMethod()->isVisible()) {
                                window->setVisible(true);
                                // This show is internal, so QInputMethod emits
                                // no visibility signal. Authorize immediately
                                // after the new layer role has been created.
                                authorizeKeyboardRole();
                            }
                        }
                    });
                }
            });

            qCDebug(PlasmaKeyboard) << "keyboard window configured as a layer-shell overlay";
            keyboardWindowBridge->setLayerShellWindow(layerShell);
            // Visibility follows what KWin reports for the panel window. This
            // comes after the authorization handler is installed so an already
            // visible input method cannot race the first show.
            keyboardWindowBridge->setKeyboardWindow(window);
            if (QGuiApplication::inputMethod()->isVisible()) {
                authorizeKeyboardRole();
            }
        };

        if (lockscreenOverlay->isActive()) {
            registerKeyboardWindow(true);
        } else {
            // The QML tree is loaded before the event loop starts, while Qt is
            // still receiving the Wayland registry globals. Do not map the
            // layer-shell window until KWin has advertised the privileged
            // overlay protocol and the surface can be marked as lock-safe.
            QObject::connect(lockscreenOverlay, &LockscreenOverlay::activeChanged, window, [lockscreenOverlay, registerKeyboardWindow]() {
                if (lockscreenOverlay->isActive()) {
                    registerKeyboardWindow(true);
                }
            });
            // Keep the keyboard usable on compositors which do not implement
            // kde_lockscreen_overlay_v1; they simply cannot show it while the
            // session is locked.
            QTimer::singleShot(1000, window, [registerKeyboardWindow]() {
                registerKeyboardWindow(false);
            });
        }
    });
    view.load(QUrl(QStringLiteral("qrc:/qt/qml/org/kde/plasma/keyboard/custom/main.qml")));

#ifdef Q_OS_UNIX
    /**
     * Set up signal handler for SIGINT and SIGTERM
     */
    KSignalHandler::self()->watchSignal(SIGINT);
    KSignalHandler::self()->watchSignal(SIGTERM);
    QObject::connect(KSignalHandler::self(), &KSignalHandler::signalReceived, &application, [](int signal) {
        if (signal == SIGINT || signal == SIGTERM) {
            qCDebug(PlasmaKeyboard) << "Received signal" << signal << ", exiting now.";
            QCoreApplication::quit();
        }
    });
#endif

    qCDebug(PlasmaKeyboard) << "Starting Plasma Keyboard application";

    // Restart with the new binary after the package was updated (see
    // RestartWatcher).
    new RestartWatcher(&application);

    // KWin does not always give windows back the size the virtual keyboard took
    // from them (KDE bug 459136), so a small script inside KWin watches the
    // keyboard and returns the geometry to the windows it shortened.
    const QString windowRestoreScript = installWindowRestoreScript();
    if (!windowRestoreScript.isEmpty()) {
        setWindowRestoreScript(windowRestoreScript, true);
        QObject::connect(&application, &QCoreApplication::aboutToQuit, &application, [windowRestoreScript] {
            setWindowRestoreScript(windowRestoreScript, false);
        });
    }

    return application.exec();
}

#include "main.moc"
