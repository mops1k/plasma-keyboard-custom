/*
    SPDX-FileCopyrightText: 2024 Aleix Pol i Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2025 Devin Lin <devin@kde.org>
    SPDX-FileCopyrightText: 2026 Kristen McWilliam <kristen@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "inputlisteneritem.h"
#include "clipboard/clipboardhistory.h"
#include "inputmethod_p.h"
#include "logging.h"
#include "plasmakeyboardsettings.h"
#include "touchholdwatcher.h"

#include "inputcontentpolicy.h"
#include "overlay/longpresstrigger.h"
#include "overlay/overlaycontroller.h"
#include "overlay/prefixquerytrigger.h"
#include "overlay/textexpansiontrigger.h"
#include "prediction/wordatcursor.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QTextFormat>
#include <QTimer>

#include <atomic>
#include <set>

#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <qpa/qwindowsysteminterface.h>

#include <linux/input-event-codes.h>

Q_GLOBAL_STATIC(InputMethod, s_im)

namespace
{
// Timestamp (ms since epoch) until which the next input activation must show
// the keyboard immediately, bypassing the long-press mode.
std::atomic<qint64> s_forceShowUntil{0};

// The keyboard window is a layer-shell surface, which the compositor never
// activates on its own, so Qt has to be told that the window holding the input
// item is the window the input goes to: without that Qt Virtual Keyboard does
// not consider the panel visible and refuses to show it (see
// PlatformInputContext::evaluateInputPanelVisible(), which needs the focus
// object of the focus window). The activation is announced the same way the
// compositor-driven show does it (see activateKeyboardWindow() in main.cpp).
void activateKeyboardWindow(QWindow *window)
{
    if (window) {
        QWindowSystemInterface::handleFocusWindowChanged(window, Qt::ActiveWindowFocusReason);
    }
}

//! Asks Qt Virtual Keyboard for the panel. The activation is announced first:
//! the panel itself appears when the activation reaches the input item, whose
//! onActiveChanged handler asks for the panel again with the input item focused.
void showInputPanel(QWindow *window)
{
    activateKeyboardWindow(window);
    QGuiApplication::inputMethod()->show();
}
} // namespace

void setInputPanelForceShowOnNextActivation()
{
    s_forceShowUntil.store(QDateTime::currentMSecsSinceEpoch() + 5000);

    // KWin does not re-activate an already active input context, so the
    // pending show must be applied right away in that case.
    if (s_im->hasContext()) {
        QGuiApplication::inputMethod()->show();
    }
}

KeyboardModifiers::KeyboardModifiers(QObject *parent)
    : QObject(parent)
{
}

KeyboardModifiers *KeyboardModifiers::instance()
{
    static KeyboardModifiers s_instance;
    return &s_instance;
}

bool KeyboardModifiers::ctrl() const
{
    return m_ctrl;
}

void KeyboardModifiers::setCtrl(bool ctrl)
{
    if (m_ctrl == ctrl) {
        return;
    }
    m_ctrl = ctrl;
    Q_EMIT ctrlChanged();
}

bool KeyboardModifiers::alt() const
{
    return m_alt;
}

void KeyboardModifiers::setAlt(bool alt)
{
    if (m_alt == alt) {
        return;
    }
    m_alt = alt;
    Q_EMIT altChanged();
}

bool KeyboardModifiers::meta() const
{
    return m_meta;
}

void KeyboardModifiers::setMeta(bool meta)
{
    if (m_meta == meta) {
        return;
    }
    m_meta = meta;
    Q_EMIT metaChanged();
}

bool KeyboardModifiers::gamepadAvailable() const
{
    return m_gamepadAvailable;
}

void KeyboardModifiers::setGamepadAvailable(bool available)
{
    if (m_gamepadAvailable == available) {
        return;
    }
    m_gamepadAvailable = available;
    qCDebug(PlasmaKeyboard) << "KeyboardModifiers::gamepadAvailable ->" << available;
    Q_EMIT gamepadAvailableChanged();
}

bool KeyboardModifiers::extraRowsFocused() const
{
    return m_extraRowsFocused;
}

void KeyboardModifiers::setExtraRowsFocused(bool focused)
{
    if (focused == m_extraRowsFocused) {
        return;
    }

    m_extraRowsFocused = focused;
    Q_EMIT extraRowsFocusedChanged();
}

void KeyboardModifiers::reset()
{
    setCtrl(false);
    setAlt(false);
    setMeta(false);
}

Q_GLOBAL_STATIC_WITH_ARGS(const std::set<int>,
                          IGNORED_KEYS,
                          {
                              Qt::Key_Context1 // Triggered by "special keys" button
                          });
// Keys to always capture for keyboard navigation
QList<Qt::Key> initCapture()
{
    return {
        Qt::Key_Left,
        Qt::Key_Right,
        Qt::Key_Up,
        Qt::Key_Down,
    };
}
Q_GLOBAL_STATIC_WITH_ARGS(const QList<Qt::Key>, KEYBOARD_NAVIGATION_CAPTURE_KEYS, (initCapture()));

// Keys to capture when keyboard navigation is active
Q_GLOBAL_STATIC_WITH_ARGS(const QList<Qt::Key>, KEYBOARD_NAVIGATION_ACTIVE_CAPTURE_KEYS, (initCapture() + QList<Qt::Key>{Qt::Key_Return}));

InputListenerItem::InputListenerItem()
    : m_input(&(*s_im))
    , m_sessionLockState(*SessionLockState::instance())
    , m_overlayController(new OverlayController(&m_input, this))
    , m_clipboardHistory(new ClipboardHistory(&m_sessionLockState, this))
    , m_uinput(std::make_unique<UinputKeyboard>(this))
{
    // Grab and listen to physical keyboard input
    m_input.setGrabbing(true);

    // Register overlay triggers
    m_overlayController->registerTrigger(new LongPressTrigger(m_overlayController));
    m_overlayController->registerTrigger(new PrefixQueryTrigger(m_overlayController));
    m_overlayController->registerTrigger(new TextExpansionTrigger(m_overlayController));

    m_modifierHoldTimer = new QTimer(this);
    m_modifierHoldTimer->setSingleShot(true);
    connect(m_modifierHoldTimer, &QTimer::timeout, this, [this] {
        // Still held past the threshold: a genuine physical hold, not a tap.
        // The uinput key is already pressed for real (it went down the
        // moment the tap latched it, in keyPressEvent()); just remember that
        // releasing the on-screen key now must end the hold for real (see
        // keyReleaseEvent()), instead of leaving it latched for whatever is
        // typed next.
        m_modifierHeldForReal = true;
    });

    // Applications drop and re-take the text input for a moment on all kinds of
    // things (a click elsewhere in the window, a popup, a cursor move), so the
    // keyboard is only hidden once the input stayed gone for a while; see the
    // contextChanged handler below, which cancels a pending hide.
    m_hideDelay = new QTimer(this);
    m_hideDelay->setSingleShot(true);
    m_hideDelay->setInterval(300);
    connect(m_hideDelay, &QTimer::timeout, this, [] {
        QGuiApplication::inputMethod()->setVisible(false);
        QGuiApplication::inputMethod()->reset();
    });

    connect(this, &InputListenerItem::predictionTargetChanged, this, [this] {
        m_clipboardHistory->setAccessAllowed(predictionActive());
        if (!predictionActive() && m_overlayController) {
            m_overlayController->cancelOverlay();
        }
    });

    connect(&m_sessionLockState, &SessionLockState::unlocked, this, [this] {
        // The panel belongs to the lock-screen field, not the desktop field
        // which may acquire focus immediately after authentication.
        m_hideDelay->stop();
        m_touchHold.disarm();
        m_longPressNeedsHitTest = false;
        m_hiddenByUser = true;
        QGuiApplication::inputMethod()->hide();
        QGuiApplication::inputMethod()->reset();
    });
    connect(&m_sessionLockState, &SessionLockState::stateChanged, this, [this] {
        Q_EMIT predictionTargetChanged();
        updatePredictionWords();
        // A lock notification is not an input-panel request. In particular,
        // the current context may still belong to an ordinary desktop field.
        if (!m_sessionLockState.isKnown() || m_sessionLockState.isPreparingLock()) {
            m_hideDelay->stop();
            m_touchHold.disarm();
            m_longPressNeedsHitTest = false;
            m_hiddenByUser = true;
            QGuiApplication::inputMethod()->hide();
            QGuiApplication::inputMethod()->reset();
        }
    });

    connect(&m_input, &InputPlugin::contextChanged, this, [this] {
        const bool hasContext = m_input.hasContext();

        // Invalidate asynchronous results even if the new field has the same word.
        Q_EMIT predictionTargetChanged();
        updatePredictionWords();

        // Overlay cleanup runs on contextAboutToChange, before the old
        // context is dropped, so synthetic releases reach the correct client.

        // During lock, KWin's explicit panel visibility is authoritative.
        // An activation/adopt can still carry a stale desktop context.
        if (!m_sessionLockState.allowsPredictions()) {
            m_hideDelay->stop();
            m_touchHold.disarm();
            m_longPressNeedsHitTest = false;
            QGuiApplication::inputMethod()->update(Qt::ImQueryAll);
            return;
        }

        if (hasContext && m_hideDelay->isActive()) {
            m_hideDelay->stop();
            if (window()->isVisible()) {
                // The input came back before the pending hide: the keyboard
                // stays as it was, without waiting for a long press again.
                m_hiddenByUser = false;
                QGuiApplication::inputMethod()->update(Qt::ImQueryAll);
                return;
            }
        }

        if (hasContext) {
            // A new activation is a new request to type: the keyboard may be
            // shown again even if it was hidden before.
            m_hiddenByUser = false;

            // In long-press mode the keyboard stays hidden until a touch is
            // held on the screen long enough; fall back to the immediate
            // show when no touchscreen can be watched.
            // The global shortcut force-activates the input method; show
            // immediately in that case instead of waiting for a long press.
            const bool forceShow = s_forceShowUntil.load() > QDateTime::currentMSecsSinceEpoch();
            if (forceShow) {
                s_forceShowUntil.store(0);
            }

            const bool longTapSetting = PlasmaKeyboardSettings::self()->showOnLongTap();
            // The lock screen cannot expose the regular session's AT-SPI tree,
            // so long-press hit testing can never prove that the touch belongs
            // to its password field. A lock-screen input context is already an
            // explicit request to type and must show immediately.
            const bool lockScreenActive = m_sessionLockState.isKnown() && m_sessionLockState.isLocked();
            const bool waitForLongTouch = !forceShow && !lockScreenActive && longTapSetting;
            const int longTouchThreshold = PlasmaKeyboardSettings::self()->showOnLongTapThresholdMs();
            const bool watchingTouch = waitForLongTouch && m_touchHold.arm(longTouchThreshold);
            // evdev input is global, so only the touch which caused this text
            // input activation may arm the hold timer. A mouse click (or any
            // other non-touch activation) must not bypass long-press mode.
            const bool armed = watchingTouch && m_touchHold.touchIsDown();
            const bool completedLongTouch = watchingTouch && m_touchHold.takeCompletedLongPress(longTouchThreshold);
            if (watchingTouch && !armed && !completedLongTouch) {
                // A mouse activation (or an already completed short tap) has
                // not proved that the next hold belongs to the text field.
                // Keep watching, but require the AT-SPI hit test on every hold.
                m_touchHold.watchContinuously(longTouchThreshold);
            }
            m_longPressNeedsHitTest = watchingTouch && !armed && !completedLongTouch;
            // Suppress both the immediate show and the surrounding-text
            // re-show path for every automatic activation in long-press mode.
            // A successful hold clears this flag in the longPress handler.
            m_hiddenByUser = watchingTouch && !completedLongTouch;
            QGuiApplication::inputMethod()->update(Qt::ImQueryAll);
            qCDebug(PlasmaKeyboard) << "contextChanged hasContext=" << hasContext << "showOnLongTap=" << longTapSetting << "forceShow=" << forceShow
                                    << "lockScreenActive=" << lockScreenActive << "armed=" << armed;
            if (!watchingTouch || completedLongTouch) {
                showInputPanel(window());
            }
        } else if (PlasmaKeyboardSettings::self()->hideOnInputFocusLoss()) {
            m_hideDelay->start();
        }
    });
    connect(&m_input, &InputPlugin::surroundingTextChanged, this, [this] {
        // The word being typed changed with the text around the cursor, so the
        // suggestions have to follow it.
        updatePredictionWords();

        // Notify the overlay controller first so it can cancel the overlay if an
        // external cursor movement is detected (e.g. user tapped elsewhere in the
        // text field while the diacritics overlay was open or the hold timer was
        // still running). The controller uses an internal counter to distinguish
        // its own commit_string echoes from external cursor moves.
        if (predictionActive() && m_overlayController) {
            m_overlayController->handleSurroundingTextChanged();
        }

        if (m_input.hasContext()) {
            // Re-activate when text input activates, and there is context.
            // Never do it while the keyboard was hidden on purpose: the field
            // keeps sending updates and the panel would pop up again at once.
            if (m_sessionLockState.allowsPredictions() && !m_hiddenByUser && !m_touchHold.isArmed() && !window()->isVisible()) {
                activateKeyboardWindow(window());
                QGuiApplication::inputMethod()->setVisible(true);
            }

            // Update vkbd input method only if the virtual keyboard panel is shown
            if (window()->isExposed()) {
                QGuiApplication::inputMethod()->update(Qt::ImSurroundingText);
            }
        }
    });
    connect(&m_input, &InputPlugin::contentTypeChanged, this, [this] {
        // A normal field can become protected without changing input context.
        // Cancel in-flight results before exposing any new word to QML.
        Q_EMIT predictionTargetChanged();
        updatePredictionWords();
        QGuiApplication::inputMethod()->update(Qt::ImHints | Qt::ImSurroundingText);
    });
    connect(&m_input, &InputPlugin::cursorChanged, this, [this] {
        // Moving the cursor moves the word that is being typed.
        updatePredictionWords();
    });

    connect(&m_input, &InputPlugin::deactivate, this, [this] {
        m_touchHold.disarm();
        // The input context is gone: the next activation may show the keyboard
        // again, even if the user had hidden it.
        m_hiddenByUser = false;
        updatePredictionWords();
        if (PlasmaKeyboardSettings::self()->hideOnInputFocusLoss()) {
            m_hideDelay->start();
        }
    });

    connect(&m_touchHold, &TouchHoldWatcher::longPress, this, [this] {
        if (!m_sessionLockState.allowsPredictions()) {
            return;
        }
        if (m_longPressNeedsHitTest && !m_focusedEditable.containsNormalizedTouch(m_touchHold.normalizedPosition())) {
            qCDebug(PlasmaKeyboard) << "TouchHoldWatcher: hold was outside the focused editable widget";
            return;
        }
        qCDebug(PlasmaKeyboard) << "TouchHoldWatcher: long press detected, showing keyboard";
        m_touchHold.disarm();
        m_longPressNeedsHitTest = false;
        m_hiddenByUser = false;
        showInputPanel(window());
    });
    connect(&m_input, &InputPlugin::resetRequested, this, [this] {
        QGuiApplication::inputMethod()->reset();
    });
    connect(QGuiApplication::inputMethod(), &QInputMethod::visibleChanged, this, [this] {
        const bool visible = QGuiApplication::inputMethod()->isVisible();
        if (!visible) {
            // The keyboard was hidden (by the user, or because the input
            // context went away). Remember it, so that a text field which keeps
            // sending updates cannot bring the panel back on its own.
            m_hiddenByUser = true;
            if (m_sessionLockState.allowsPredictions() && m_input.hasContext() && PlasmaKeyboardSettings::self()->showOnLongTap()
                && m_touchHold.watchContinuously(PlasmaKeyboardSettings::self()->showOnLongTapThresholdMs())) {
                m_longPressNeedsHitTest = true;
            }
        } else {
            m_touchHold.disarm();
            m_longPressNeedsHitTest = false;
        }
        window()->setVisible(visible);
    });

    connect(&m_input, &InputPlugin::keyPressed, this, [this](QKeyEvent *keyEvent) {
        // qCDebug(PlasmaKeyboard) << "keyPressed. keycode:" << keyEvent->key() << "text:" << keyEvent->text() << "modifiers:" << keyEvent->modifiers();

        // Delegate to overlay controller for diacritics/emoji/text expansion
        if (predictionActive() && m_overlayController && m_overlayController->processKeyPress(keyEvent)) {
            keyEvent->accept();
            return;
        }
        // The window can have isVisible() = true, but not be shown by the compositor if the input panel is suppressed
        if (!window()->isExposed()) {
            return;
        }

        if (PlasmaKeyboardSettings::self()->keyboardNavigationEnabled()) {
            // Keys to capture for keyboard navigation
            const auto keys = m_keyboardNavigationActive ? *KEYBOARD_NAVIGATION_ACTIVE_CAPTURE_KEYS : *KEYBOARD_NAVIGATION_CAPTURE_KEYS;

            // Forward and accept keyboard navigation events
            for (const auto key : keys) {
                if (keyEvent->key() == key) {
                    keyEvent->accept();
                    Q_EMIT keyNavigationPressed(key);
                    break;
                }
            }
        }
    });
    connect(&m_input, &InputPlugin::keyReleased, this, [this](QKeyEvent *keyEvent) {
        // qCDebug(PlasmaKeyboard) << "keyReleased. keycode:" << keyEvent->key() << "text:" << keyEvent->text();

        // Let the overlay controller handle release events (e.g. diacritics, emoji)
        if (m_overlayController && m_overlayController->processKeyRelease(keyEvent)) {
            keyEvent->accept();
            return;
        }
        // The window can have isVisible() = true, but not be shown by the compositor if the input panel is suppressed
        if (!window()->isExposed()) {
            return;
        }

        if (PlasmaKeyboardSettings::self()->keyboardNavigationEnabled()) {
            // Keys to capture for keyboard navigation
            const auto keys = m_keyboardNavigationActive ? *KEYBOARD_NAVIGATION_ACTIVE_CAPTURE_KEYS : *KEYBOARD_NAVIGATION_CAPTURE_KEYS;

            // Forward and accept keyboard navigation events
            for (const auto key : keys) {
                if (keyEvent->key() == key) {
                    keyEvent->accept();
                    Q_EMIT keyNavigationReleased(key);
                    break;
                }
            }
        }
    });

    // Don't hook into the &InputPlugin::receivedCommit signal and call setVisible(true)
    // -> it can cause a race condition as receivedCommit is emitted when the text field loses focus.
    //
    // If the virtual keyboard (vkbd) is manually closed and then the text field loses focus,
    // setVisible(true/false) calls in quick succession may reopen the vkbd in a broken state.
    //
    // Series of events:
    // - vkbd is manually closed (by user), but text field still focused -> setVisible(false)
    // - User unfocuses the text field
    // - receivedCommit() emitted -> setVisible(true)
    // - contextChanged() (m_input.hasContext() = false) -> setVisible(false)
    // - keyboard gets reopened by KWin because the input context gets from setVisible(true) finishes initializing!
    //
    // This happens because of a delay in InputPanelV1Window initialization, which confuses KWin
    // into creating a new input context even when no text field is focused.
    //
    // TODO: Investigate other places this might happen and how to fix it.

    QGuiApplication::inputMethod()->update(Qt::ImQueryAll);
}

OverlayController *InputListenerItem::overlayController() const
{
    return m_overlayController;
}

ClipboardHistory *InputListenerItem::clipboardHistory() const
{
    return m_clipboardHistory;
}

void InputListenerItem::commitText(const QString &text)
{
    if (text.isEmpty() || !m_input.hasContext()) {
        return;
    }

    m_input.commit(text);
}

void InputListenerItem::pasteClipboardEntry(int row)
{
    if (!predictionActive()) {
        return;
    }
    commitText(m_clipboardHistory->textAt(row));
}

QString InputListenerItem::textBeforeCursor() const
{
    if (!predictionActive() || !PlasmaKeyboardSettings::self()->predictiveTextEnabled()) {
        return {};
    }

    // The cursor position is in bytes, the word is cut in characters.
    const QByteArray surrounding = m_input.surroundingText().toUtf8();
    const int cursorBytes = qBound(0, int(m_input.cursorPos()), surrounding.size());
    return QString::fromUtf8(surrounding.first(cursorBytes));
}

QString InputListenerItem::predictionPrefix() const
{
    return wordBeforeCursor(textBeforeCursor());
}

QString InputListenerItem::predictionContext() const
{
    return previousWordBeforeCursor(textBeforeCursor());
}

bool InputListenerItem::sensitiveInput() const
{
    return m_sensitiveInput;
}

bool InputListenerItem::atSentenceStart() const
{
    const QString before = textBeforeCursor();
    if (before.trimmed().isEmpty()) {
        return true;
    }
    // The space after a sentence end may already be inserted by the keyboard.
    static const QRegularExpression endOfSentence(QStringLiteral("(^|[.!?…]|\\n)[\\s\"'»”’)\\]]*$"));
    return endOfSentence.match(before).hasMatch();
}

void InputListenerItem::updatePredictionWords()
{
    const QString prefix = predictionPrefix();
    if (prefix != m_predictionPrefix) {
        m_predictionPrefix = prefix;
        Q_EMIT predictionPrefixChanged();
    }

    const QString context = predictionContext();
    if (context != m_predictionContext) {
        m_predictionContext = context;
        Q_EMIT predictionContextChanged();
    }
}

void InputListenerItem::applyPrediction(const QString &word)
{
    if (word.isEmpty() || !predictionActive() || !PlasmaKeyboardSettings::self()->predictiveTextEnabled()) {
        return;
    }

    // A picked word is typed as a whole, with the space after it so that the
    // next word can be typed right away, the way the on-screen keyboards do it.
    const QString committed = word + QLatin1Char(' ');

    const QString prefix = predictionPrefix();
    if (prefix.isEmpty()) {
        commitText(committed);
        return;
    }

    // Replace the word that was typed with the suggestion: the replacement is
    // what the clients expect for a completion, and inputMethodEvent() already
    // knows how to send it over the wayland protocol.
    QInputMethodEvent event;
    event.setCommitString(committed, -prefix.size(), prefix.size());
    inputMethodEvent(&event);
}

void InputListenerItem::commitSwipeWord(const QString &word)
{
    if (word.isEmpty() || !m_input.hasContext()) {
        return;
    }

    const QString committed = word + QLatin1Char(' ');
    const QString prefix = predictionPrefix();
    if (prefix.isEmpty()) {
        commitText(committed);
    } else {
        QInputMethodEvent event;
        event.setCommitString(committed, -prefix.size(), prefix.size());
        inputMethodEvent(&event);
    }
    // An alternative offered for this swipe may replace exactly this text.
    m_committedSwipe = committed;
}

void InputListenerItem::replaceSwipeWord(const QString &word)
{
    if (word.isEmpty() || m_committedSwipe.isEmpty() || !m_input.hasContext()) {
        return;
    }
    // Only replace when the inserted word is still right before the cursor: the
    // text may have been edited in the field since the swipe.
    if (!textBeforeCursor().endsWith(m_committedSwipe)) {
        m_committedSwipe.clear();
        return;
    }

    const QString committed = word + QLatin1Char(' ');
    QInputMethodEvent event;
    event.setCommitString(committed, -m_committedSwipe.size(), m_committedSwipe.size());
    inputMethodEvent(&event);
    m_committedSwipe = committed;
}

void InputListenerItem::setEngine(QVirtualKeyboardInputEngine *engine)
{
    // The overlay controller inserts a picked alternate character as a key
    // click through the engine, the way the on-screen alternate-keys popup
    // does; a bare commit_string makes clients end the input session instead.
    if (m_overlayController) {
        m_overlayController->setInputEngine(engine);
    }
}

QVariant InputListenerItem::inputMethodQuery(Qt::InputMethodQuery query) const
{
    if (!m_input.hasContext()) {
        if (query == Qt::ImEnabled) {
            return false;
        }
        return {};
    }

    switch (query) {
    case Qt::ImEnabled:
        return true;
    case Qt::ImSurroundingText:
        return m_input.surroundingText();
    case Qt::ImHints: {
        const auto imHints = m_input.contentHint();
        Qt::InputMethodHints qtHints = InputContentPolicy::privacyHints(imHints, m_input.contentPurpose());

        // Disable Qt VirtualKeyboard's hunspell plugin
        // It deals poorly with current external cursor position change
        // logic, and causes problems when selecting text with a mouse.
        qtHints |= Qt::ImhNoPredictiveText;

        // if (imHints & InputPlugin::content_hint_default) { }
        if (imHints & InputPlugin::content_hint_password) {
            qtHints |= Qt::ImhSensitiveData;
        }
        if ((imHints & InputPlugin::content_hint_auto_capitalization) == 0 || !PlasmaKeyboardSettings::self()->autoCapitalizationEnabled()) {
            qtHints |= Qt::ImhNoAutoUppercase;
        }
        // if (imHints & InputPlugin::content_hint_titlecase) { }
        if (imHints & InputPlugin::content_hint_lowercase) {
            qtHints |= Qt::ImhPreferLowercase;
        }
        if (imHints & InputPlugin::content_hint_uppercase) {
            qtHints |= Qt::ImhPreferUppercase;
        }
        if (imHints & InputPlugin::content_hint_hidden_text) {
            qtHints |= Qt::ImhSensitiveData;
        }
        if (imHints & InputPlugin::content_hint_sensitive_data) {
            qtHints |= Qt::ImhSensitiveData;
        }
        // The personal words of the swipe engine are not learned from a field
        // that asks not to be remembered.
        const bool sensitive =
            (imHints & (InputPlugin::content_hint_password | InputPlugin::content_hint_hidden_text | InputPlugin::content_hint_sensitive_data))
            || m_input.contentPurpose() == InputPlugin::content_purpose_password;
        m_sensitiveInput = sensitive;
        if (imHints & InputPlugin::content_hint_latin) {
            qtHints |= Qt::ImhPreferLatin;
        }
        if (imHints & InputPlugin::content_hint_multiline) {
            qtHints |= Qt::ImhMultiLine;
        }
        const auto imPurpose = m_input.contentPurpose();
        switch (imPurpose) {
        case InputPlugin::content_purpose_normal:
        case InputPlugin::content_purpose_alpha:
        case InputPlugin::content_purpose_name:
            break;
        case InputPlugin::content_purpose_digits:
            qtHints |= Qt::ImhDigitsOnly;
            break;
        case InputPlugin::content_purpose_number:
            qtHints |= Qt::ImhPreferNumbers;
            break;
        case InputPlugin::content_purpose_phone:
            qtHints |= Qt::ImhDialableCharactersOnly;
            break;
        case InputPlugin::content_purpose_url:
            qtHints |= Qt::ImhUrlCharactersOnly;
            break;
        case InputPlugin::content_purpose_email:
            qtHints |= Qt::ImhEmailCharactersOnly;
            break;
        case InputPlugin::content_purpose_password:
            qtHints |= Qt::ImhSensitiveData;
            break;
        case InputPlugin::content_purpose_date:
            qtHints |= Qt::ImhDate;
            break;
        case InputPlugin::content_purpose_time:
            qtHints |= Qt::ImhTime;
            break;
        case InputPlugin::content_purpose_datetime:
            qtHints |= Qt::ImhDate;
            qtHints |= Qt::ImhTime;
            break;
        case InputPlugin::content_purpose_terminal:
            qtHints |= Qt::ImhPreferLatin;
            break;
        }
        return QVariant::fromValue<int>(qtHints);
    }
    case Qt::ImCurrentSelection: {
        // cursorPos and anchorPos are in bytes, we need to convert QString to QByteArray for index operations
        QByteArray surroundingText = m_input.surroundingText().toUtf8();
        int start = qMin(m_input.cursorPos(), m_input.anchorPos());
        int end = qMax(m_input.cursorPos(), m_input.anchorPos());
        return QString::fromUtf8(surroundingText.mid(start, end - start));
    }
    case Qt::ImAnchorPosition: {
        // anchorPos is in bytes, we need to convert QString to QByteArray for index operations
        QByteArray surroundingText = m_input.surroundingText().toUtf8();
        int anchorBytes = qBound(0, int(m_input.anchorPos()), surroundingText.size());
        return QString::fromUtf8(surroundingText.first(anchorBytes)).length();
    }
    case Qt::ImCursorPosition: {
        // cursorPos is in bytes, we need to convert QString to QByteArray for index operations
        QByteArray surroundingText = m_input.surroundingText().toUtf8();
        int cursorBytes = qBound(0, int(m_input.cursorPos()), surroundingText.size());
        return QString::fromUtf8(surroundingText.first(cursorBytes)).length();
    }
    case Qt::ImTextBeforeCursor: {
        // cursorPos is in bytes, we need to convert QString to QByteArray for index operations
        QByteArray surroundingText = m_input.surroundingText().toUtf8();
        int cursorBytes = qBound(0, int(m_input.cursorPos()), surroundingText.size());
        return QString::fromUtf8(surroundingText.first(cursorBytes));
    }
    case Qt::ImTextAfterCursor: {
        // cursorPos is in bytes, we need to convert QString to QByteArray for index operations
        QByteArray surroundingText = m_input.surroundingText().toUtf8();
        return QString::fromUtf8(surroundingText.mid(m_input.cursorPos()));
    }
    case Qt::ImCursorRectangle:
    case Qt::ImFont:
    case Qt::ImMaximumTextLength:
    case Qt::ImPreferredLanguage:
    case Qt::ImPlatformData:
    case Qt::ImAbsolutePosition:
    case Qt::ImEnterKeyType:
    case Qt::ImAnchorRectangle:
    case Qt::ImInputItemClipRectangle:
    case Qt::ImReadOnly:
        // We don't do that
        break;
    default:
        qWarning() << "Unhandled query" << query;
        break;
    }
    return {};
}

void InputListenerItem::armModifierHoldTimer(Qt::Key key)
{
    m_pendingModifierKey = key;
    m_modifierHeldForReal = false;
    m_modifierHoldTimer->start(PlasmaKeyboardSettings::self()->modifierHoldThresholdMs());
}

void InputListenerItem::cancelModifierHoldTimer()
{
    m_modifierHoldTimer->stop();
    m_pendingModifierKey = Qt::Key(0);
    m_modifierHeldForReal = false;
}

void InputListenerItem::pressUinputModifier(Qt::Key key)
{
    if (!m_uinput || !m_uinput->isValid()) {
        return;
    }
    switch (key) {
    case Qt::Key_Control:
        if (!m_uinputCtrlHeld) {
            m_uinput->pressKey(KEY_LEFTCTRL);
            m_uinputCtrlHeld = true;
        }
        break;
    case Qt::Key_Alt:
        if (!m_uinputAltHeld) {
            m_uinput->pressKey(KEY_LEFTALT);
            m_uinputAltHeld = true;
        }
        break;
    case Qt::Key_Meta:
        if (!m_uinputMetaHeld) {
            m_uinput->pressKey(KEY_LEFTMETA);
            m_uinputMetaHeld = true;
        }
        break;
    default:
        break;
    }
}

void InputListenerItem::releaseUinputModifierIfHeld(Qt::Key key)
{
    if (!m_uinput) {
        return;
    }
    switch (key) {
    case Qt::Key_Control:
        if (m_uinputCtrlHeld) {
            m_uinput->releaseKey(KEY_LEFTCTRL);
            m_uinputCtrlHeld = false;
        }
        break;
    case Qt::Key_Alt:
        if (m_uinputAltHeld) {
            m_uinput->releaseKey(KEY_LEFTALT);
            m_uinputAltHeld = false;
        }
        break;
    case Qt::Key_Meta:
        if (m_uinputMetaHeld) {
            m_uinput->releaseKey(KEY_LEFTMETA);
            m_uinputMetaHeld = false;
        }
        break;
    default:
        break;
    }
}

void InputListenerItem::releaseAllHeldUinputModifiers()
{
    releaseUinputModifierIfHeld(Qt::Key_Control);
    releaseUinputModifierIfHeld(Qt::Key_Alt);
    releaseUinputModifierIfHeld(Qt::Key_Meta);
}

bool InputListenerItem::handleModifiedKey(QKeyEvent *event, bool press)
{
    auto *modifiers = KeyboardModifiers::instance();
    if (!modifiers->ctrl() && !modifiers->alt() && !modifiers->meta()) {
        return false;
    }

    const QList<xkb_keysym_t> keysyms = QXkbCommon::toKeysym(event);
    if (keysyms.isEmpty()) {
        return false;
    }

    const uint32_t keycode = m_input.keycodeForKeysym(keysyms.first());
    if (keycode == 0) {
        return false;
    }

    if (m_uinput && m_uinput->isValid()) {
        // Any Ctrl/Alt/Meta combination can be a compositor-global shortcut
        // (Alt+Tab, Meta+L, Meta+D, the application launcher...), which
        // input-method-v1's client-scoped key forwarding cannot reach -- see
        // UinputKeyboard's own documentation. Every latched modifier is
        // already held down for real through that same uinput device (see
        // keyPressEvent()); send the completing key the same way, so the
        // compositor sees a real hardware key combination -- this also still
        // reaches an ordinary application shortcut like Ctrl+C, since the
        // compositor forwards a real key to whichever client has keyboard
        // focus. keycodeForKeysym() already returns an evdev scancode (see
        // its own comment), so it can be sent as-is.
        if (press) {
            m_uinput->pressKey(keycode);
        } else {
            m_uinput->releaseKey(keycode);
            releaseAllHeldUinputModifiers();
            modifiers->reset();
            cancelModifierHoldTimer();
        }
        return true;
    }

    // Fallback for when /dev/uinput could not be opened: the older
    // input-method-v1 path, which only reaches the focused client's own
    // shortcuts (e.g. Ctrl+C), not compositor-global ones.
    //
    // Shift can take part in the combination (e.g. Ctrl+Shift+key).
    const uint32_t depressed = (modifiers->ctrl() ? m_input.controlMask() : 0) | (modifiers->alt() ? m_input.altMask() : 0)
        | (modifiers->meta() ? m_input.metaMask() : 0) | ((event->modifiers() & Qt::ShiftModifier) ? m_input.shiftMask() : 0);

    if (press) {
        // Set the modifier state before pressing the key so the compositor
        // delivers the shortcut (e.g. Ctrl+C) to the client.
        m_input.sendModifiers(depressed, 0, 0, 0);
        m_input.key(InputPlugin::Pressed, keycode);
    } else {
        m_input.key(InputPlugin::Released, keycode);
        m_input.sendModifiers(0, 0, 0, 0);
        // The modifiers only apply to a single key press.
        modifiers->reset();
        // A hold timer may still be running for this modifier (it was pressed
        // just before this combination completed); it must not fire later.
        cancelModifierHoldTimer();
    }
    return true;
}

void InputListenerItem::sendKeyEvent(int key, const QString &text)
{
    QKeyEvent pressEvent(QEvent::KeyPress, key, Qt::NoModifier, text);
    QKeyEvent releaseEvent(QEvent::KeyRelease, key, Qt::NoModifier, text);
    keyPressEvent(&pressEvent);
    keyReleaseEvent(&releaseEvent);
}

QStringList InputListenerItem::alternatesFor(const QVariantList &alternatives, bool uppercase) const
{
    QStringList result;
    result.reserve(alternatives.size());

    for (const QVariant &entry : alternatives) {
        const QString character = entry.toString();
        if (character.isEmpty()) {
            continue;
        }
        // Qt Virtual Keyboard uppercases its own alternate-keys popup from the
        // keyboard state, not from the key text, so a shifted keyboard offers
        // shifted alternates here too.
        result.append(uppercase ? character.toUpper() : character);
    }

    return result;
}

void InputListenerItem::keyPressEvent(QKeyEvent *event)
{
    if (IGNORED_KEYS->find(event->key()) != IGNORED_KEYS->end()) {
        return;
    }

    // Any other key being pressed while a modifier's hold timer is still
    // pending settles what the tap was for: a combination, not a hold. Stop
    // the timer so it cannot fire later and undo the latch out from under a
    // combination that just has not completed yet -- sliding a finger from
    // the modifier straight onto the next key without lifting it in between
    // (a normal, fast typing gesture) never reaches this item's own
    // keyReleaseEvent for the modifier, so that is not a reliable place to
    // rely on alone. The latch itself is left untouched.
    if (event->key() != Qt::Key_Control && event->key() != Qt::Key_Alt && event->key() != Qt::Key_Meta && m_modifierHoldTimer->isActive()) {
        m_modifierHoldTimer->stop();
    }

    // The on-screen Ctrl/Alt/Meta keys emit a plain Control/Alt/Meta event; a
    // tap toggles the latch, which stays engaged like a lock key -- applying
    // to whatever combination follows, however long that takes -- until it is
    // either completed (handleModifiedKey()) or cancelled by tapping the same
    // key again. Right away rather than waiting for a hold, the tap also
    // presses the modifier for real through the uinput device, so it already
    // behaves like a physical modifier held down, for games/apps that read
    // the raw key state and for compositor-global shortcuts alike (Alt+Tab,
    // Meta+L...), which only fire when the compositor sees a real key down
    // for the whole combination; see UinputKeyboard's and
    // handleModifiedKey()'s own documentation. If the key is still physically
    // held when the hold timer fires, that is a genuine hold rather than a
    // tap-then-latch, and keyReleaseEvent() ends it the moment the finger
    // lifts instead of leaving it latched.
    auto *modifiers = KeyboardModifiers::instance();
    if (event->key() == Qt::Key_Control || event->key() == Qt::Key_Alt || event->key() == Qt::Key_Meta) {
        const auto key = static_cast<Qt::Key>(event->key());
        bool nowLatched = false;
        switch (key) {
        case Qt::Key_Control:
            modifiers->setCtrl(!modifiers->ctrl());
            nowLatched = modifiers->ctrl();
            break;
        case Qt::Key_Alt:
            modifiers->setAlt(!modifiers->alt());
            nowLatched = modifiers->alt();
            break;
        case Qt::Key_Meta:
            modifiers->setMeta(!modifiers->meta());
            nowLatched = modifiers->meta();
            break;
        default:
            break;
        }
        if (nowLatched) {
            pressUinputModifier(key);
            armModifierHoldTimer(key);
        } else {
            // A second tap while already latched cancels it.
            releaseUinputModifierIfHeld(key);
            cancelModifierHoldTimer();
        }
        event->accept();
        return;
    }

    if (handleModifiedKey(event, true)) {
        event->accept();
        return;
    }

    const QList<xkb_keysym_t> keys = QXkbCommon::toKeysym(event);
    for (auto key : keys) {
        // Simulate key press only if it's not textual
        if (event->text().isEmpty() || key == XKB_KEY_Return) { // (return is technically "\n")
            m_input.keysym(QDateTime::currentMSecsSinceEpoch(), key, InputPlugin::Pressed, 0);
        }
    }
}

void InputListenerItem::keyReleaseEvent(QKeyEvent *event)
{
    if (IGNORED_KEYS->find(event->key()) != IGNORED_KEYS->end()) {
        return;
    }

    if (event->key() == Qt::Key_Control || event->key() == Qt::Key_Alt || event->key() == Qt::Key_Meta) {
        const auto key = static_cast<Qt::Key>(event->key());
        if (m_modifierHeldForReal && key == m_pendingModifierKey) {
            // A genuine hold (the timer fired while still touching, see
            // armModifierHoldTimer()): releasing the on-screen key now ends
            // it for real, instead of leaving it latched for whatever is
            // typed next.
            auto *modifiers = KeyboardModifiers::instance();
            switch (key) {
            case Qt::Key_Control:
                modifiers->setCtrl(false);
                break;
            case Qt::Key_Alt:
                modifiers->setAlt(false);
                break;
            case Qt::Key_Meta:
                modifiers->setMeta(false);
                break;
            default:
                break;
            }
            releaseUinputModifierIfHeld(key);
            m_pendingModifierKey = Qt::Key(0);
            m_modifierHeldForReal = false;
        } else {
            // A quick tap: the toggle (and the uinput press it made, if it
            // latched) already happened on press -- see keyPressEvent(). The
            // hold timer must not fire later and mark this as a hold when
            // the finger already lifted, so stop it. The latch itself is
            // left alone: it stays engaged, like a lock key, until a
            // combination completes (handleModifiedKey()) or the same key is
            // tapped again to cancel it (keyPressEvent()) -- it does not
            // expire on its own.
            m_modifierHoldTimer->stop();
            m_pendingModifierKey = Qt::Key(0);
            m_modifierHeldForReal = false;
        }
        event->accept();
        return;
    }

    auto *modifiers = KeyboardModifiers::instance();
    const bool wasLatched = modifiers->ctrl() || modifiers->alt() || modifiers->meta();

    if (handleModifiedKey(event, false)) {
        event->accept();
        return;
    }

    if (wasLatched) {
        // The combination completes on release; clear the latch even if the key
        // could not be mapped, so the modifier never gets stuck.
        modifiers->reset();
        cancelModifierHoldTimer();
        // handleModifiedKey() declined (this key could not be mapped): if a
        // modifier was already pressed for real through uinput, release it
        // too, so it never gets stuck down.
        releaseAllHeldUinputModifiers();
    }

    const QList<xkb_keysym_t> keys = QXkbCommon::toKeysym(event);
    for (auto key : keys) {
        if (event->text().isEmpty() || key == XKB_KEY_Return) { // (return is technically "\n")
            // Simulate the keyboard press for non textual keys
            m_input.keysym(QDateTime::currentMSecsSinceEpoch(), key, InputPlugin::Released, 0);
        } else {
            // If we have text coming as a key event, use it to commit the string
            m_input.commit(event->text());
        }
    }
}

void InputListenerItem::inputMethodEvent(QInputMethodEvent *event)
{
    // Don't forward events from Qt VKBD to KWin if the keyboard window isn't open
    // This avoids the possibility of Qt VKBD causing text changes from receiving
    // events (ex. cursor movement) done by its input engine (for features like text prediction).
    if (!window()->isExposed()) {
        return;
    }

    QString commit = event->commitString();
    QString preedit = event->preeditString();
    bool needsReplacement = event->replacementStart() != 0 || event->replacementLength() != 0;

    // Delete characters that are supposed to be replaced
    if (needsReplacement) {
        // The positions we send to need to be in bytes (to support UTF8)
        QString surroundingText = m_input.surroundingText();
        QByteArray surroundingUtf8 = surroundingText.toUtf8();
        int boundedCursorBytes = qBound(0, int(m_input.cursorPos()), surroundingUtf8.size());
        int cursorChars = QString::fromUtf8(surroundingUtf8.first(boundedCursorBytes)).size();
        int startChars = qBound(0, cursorChars + event->replacementStart(), surroundingText.size());
        int endChars = qBound(startChars, startChars + event->replacementLength(), surroundingText.size());

        int startBytes = surroundingText.first(startChars).toUtf8().size();
        int endBytes = surroundingText.first(endChars).toUtf8().size();

        if (endBytes > startBytes) {
            m_input.deleteSurroundingText(startBytes - boundedCursorBytes, endBytes - startBytes);
        }
    }

    // Commit string if there is something to commit, or we did a replacement
    if (needsReplacement || !commit.isEmpty()) {
        m_input.commit(commit);
    }

    // Set attributes for style (ex. needed for CJK)
    for (const auto &x : event->attributes()) {
        if (x.type == QInputMethodEvent::TextFormat) {
            int startBytes = preedit.first(qBound(0, x.start, preedit.size())).toUtf8().size();
            int endBytes = preedit.first(qBound(0, x.start + x.length, preedit.size())).toUtf8().size();
            m_input.setPreEditStyle(startBytes, endBytes - startBytes, x.value.value<QTextFormat>().type());
        }
    }

    // Send cursor position (must be before preedit string)
    int preEditCursorPos = preedit.toUtf8().size();
    for (const auto &attribute : event->attributes()) {
        if (attribute.type != QInputMethodEvent::Cursor) {
            continue;
        }

        // Set special attribute preedit cursor position (ex. needed for CJK)
        if (!attribute.value.isValid() || !attribute.value.toBool()) {
            preEditCursorPos = -1;
        } else {
            preEditCursorPos = preedit.first(qBound(0, attribute.start, preedit.size())).toUtf8().size();
        }
        break;
    }
    m_input.setPreEditCursor(preEditCursorPos);

    // Send currently being edited string
    m_input.setPreEditString(preedit);
}

#include "moc_inputlisteneritem.cpp"
