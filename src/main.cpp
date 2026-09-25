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
#include "settingsreloader.h"
#include "sttmanager.h"
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
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDir>
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
    {
    }

    void setStubWindow(QQuickWindow *stub)
    {
        m_stub = stub;
        applyStubMask();
        // The stub's screen is corrected by QWaylandInputPanelSurface::applyConfigure()
        // once the compositor tells it which output the panel actually belongs
        // to (see qwaylandinputpanelsurface.cpp); that correction arrives after
        // a Wayland round trip, i.e. later than this call. The layer-shell
        // keyboard window never receives that information itself, so it is
        // kept in sync with whatever screen the stub ends up on instead.
        connect(stub, &QWindow::screenChanged, this, &KeyboardWindowBridge::syncKeyboardScreen);
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
        // KWin drives the keyboard through the panel stub and reports it as
        // visible only while that stub is mapped, so the stub follows the real
        // keyboard window (which Qt Virtual Keyboard shows and hides itself):
        // without this the compositor keeps the panel shown after the keyboard is
        // hidden, the gamepad mapping is not restored and the focused window keeps
        // the space reserved for the keyboard.
        connect(window, &QWindow::visibleChanged, this, &KeyboardWindowBridge::syncStubVisibility);
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

    //! The compositor re-reserves the space for the panel on its own: the docked
    //! keyboard asks for it with the layer-shell exclusive zone of its window, so
    //! switching modes only has to re-apply that zone (see applyPanelLayout()).
Q_SIGNALS:
    void kwinVisibleChanged();

private:
    //! The panel stub never takes part in the layout: the space for the docked
    //! keyboard is reserved by the compositor through the layer-shell exclusive
    //! zone of the keyboard window, so the stub keeps a one pixel input region and
    //! cannot swallow the touches meant for the keys.
    void applyStubMask()
    {
        if (!m_stub) {
            return;
        }
        m_stub->setMask(QRegion(0, 0, 1, 1));
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
        if (PlasmaKeyboardSettings::self()->floatingKeyboard()) {
            anchors |= LayerShellQt::Window::AnchorTop;
            m_layerShell->setDesiredSize(screen);
            m_layerShell->setExclusiveZone(0);
        } else {
            anchors |= LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorBottom | LayerShellQt::Window::AnchorRight);
            m_layerShell->setDesiredSize(QSize(0, screen.height()));
            m_layerShell->setExclusiveZone(m_panelRect.isValid() ? m_panelRect.height() : 0);
        }
        m_layerShell->setAnchors(anchors);
    }

    //! Keeps the layer-shell keyboard window on the same screen as the panel
    //! stub. Left on whatever screen Qt assigned it at creation (normally the
    //! primary one), every dimension main.qml computes from the QML Screen
    //! attached property (its own width/height, the style's target sizes) is
    //! wrong on any setup where the keyboard is not shown on the primary
    //! screen, and the panel ends up clipped or overlapping on both edges,
    //! exactly like the panel stub did before QWaylandInputPanelSurface
    //! started calling setScreen() itself.
    void syncKeyboardScreen()
    {
        if (!m_stub || !m_keyboard || !m_stub->screen() || m_keyboard->screen() == m_stub->screen()) {
            return;
        }
        m_keyboard->setScreen(m_stub->screen());
        applyPanelLayout();
    }

    //! Keeps the panel stub mapped exactly while the keyboard window is mapped.
    void syncStubVisibility()
    {
        if (!m_stub) {
            return;
        }
        const bool visible = m_keyboard && m_keyboard->isVisible();
        if (m_stub->isVisible() != visible) {
            qCDebug(PlasmaKeyboard) << "panel stub mapped:" << visible;
            m_stub->setVisible(visible);
        }
    }

    QPointer<QQuickWindow> m_stub;
    QPointer<QWindow> m_keyboard;
    QPointer<LayerShellQt::Window> m_layerShell;
    QRect m_panelRect;
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
            updatePanelVisibility();
        });
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
        if (kwinVisible()) {
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
        const bool keyboardVisible = kwinVisible();

        // The visible keyboard window follows what the compositor reports: KWin
        // still decides when the keyboard is on screen (input mode and focus),
        // it just does it through the invisible panel window.
        if (m_bridge) {
            m_bridge->setKwinVisible(keyboardVisible);
        }
        qCDebug(PlasmaKeyboard) << "keyboard visible" << keyboardVisible << "input method visible" << QGuiApplication::inputMethod()->isVisible();

        // Keep the input method in step with the compositor, but only when the
        // compositor actually changed the panel state. Calling show() on every
        // poll while the panel is visible would undo a hide: right after the
        // user closed the keyboard the compositor is still visible for a
        // moment, and the next poll would bring the panel straight back.
        if (keyboardVisible != m_lastKeyboardVisible) {
            m_lastKeyboardVisible = keyboardVisible;
            QGuiApplication::inputMethod()->setVisible(keyboardVisible);
        }

        // The floating keyboard does not reach the bottom of the screen, so the
        // Plasma panel is left alone while it is on.
        const bool floating = PlasmaKeyboardSettings::self()->floatingKeyboard();
        if (!floating && PlasmaKeyboardSettings::self()->hidePanelWhenKeyboardVisible() && keyboardVisible) {
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
        evaluatePlasmaScript(QStringLiteral("panelIds.forEach(function(id){var p=panelById(id);if(p.hiding!==\"autohide\"){p.hiding=\"autohide\";}});"));
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
    bool m_lastKeyboardVisible = false;
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
    // Let the manager read the effective palette from the QML theme layer
    // (used by exportTheme).
    ThemeManager::instance()->setQmlEngine(&view);
    KLocalization::setupLocalizedContext(&view);

    QObject::connect(&view, &QQmlApplicationEngine::objectCreated, &application, [keyboardWindowBridge](QObject *object) {
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

        qCDebug(PlasmaKeyboard) << "keyboard window configured as a layer-shell overlay";
        keyboardWindowBridge->setLayerShellWindow(layerShell);
        // Visibility follows what KWin reports for the panel window.
        keyboardWindowBridge->setKeyboardWindow(window);
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

    return application.exec();
}

#include "main.moc"
