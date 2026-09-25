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

#include "overlay/longpresstrigger.h"
#include "overlay/overlaycontroller.h"
#include "overlay/prefixquerytrigger.h"
#include "overlay/textexpansiontrigger.h"
#include "prediction/wordatcursor.h"

#include <QLoggingCategory>
#include <QTextFormat>
#include <QTimer>

#include <atomic>
#include <set>

#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <qpa/qwindowsysteminterface.h>

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
    , m_overlayController(new OverlayController(&m_input, this))
    , m_clipboardHistory(new ClipboardHistory(this))
{
    // Grab and listen to physical keyboard input
    m_input.setGrabbing(true);

    // Register overlay triggers
    m_overlayController->registerTrigger(new LongPressTrigger(m_overlayController));
    m_overlayController->registerTrigger(new PrefixQueryTrigger(m_overlayController));
    m_overlayController->registerTrigger(new TextExpansionTrigger(m_overlayController));

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

    connect(&m_input, &InputPlugin::contextChanged, this, [this] {
        const bool hasContext = m_input.hasContext();

        updatePredictionWords();

        // Cancel any pending overlay state when the input context changes (focus loss or target swap)
        if (m_overlayController) {
            m_overlayController->cancelOverlay();
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

            QGuiApplication::inputMethod()->update(Qt::ImQueryAll);
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
            const bool armed = !forceShow && longTapSetting && m_touchHold.arm(PlasmaKeyboardSettings::self()->showOnLongTapThresholdMs());
            qCDebug(PlasmaKeyboard) << "contextChanged hasContext=" << hasContext << "showOnLongTap=" << longTapSetting << "forceShow=" << forceShow
                                    << "armed=" << armed;
            if (!armed) {
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
        if (m_overlayController) {
            m_overlayController->handleSurroundingTextChanged();
        }

        if (m_input.hasContext()) {
            // Re-activate when text input activates, and there is context.
            // Never do it while the keyboard was hidden on purpose: the field
            // keeps sending updates and the panel would pop up again at once.
            if (!m_hiddenByUser && !m_touchHold.isArmed() && !window()->isVisible()) {
                activateKeyboardWindow(window());
                QGuiApplication::inputMethod()->setVisible(true);
            }

            // Update vkbd input method only if the virtual keyboard panel is shown
            if (window()->isExposed()) {
                QGuiApplication::inputMethod()->update(Qt::ImSurroundingText);
            }
        }
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
        qCDebug(PlasmaKeyboard) << "TouchHoldWatcher: long press detected, showing keyboard";
        m_hiddenByUser = false;
        showInputPanel(window());
    });
    connect(&m_input, &InputPlugin::resetRequested, this, [] {
        QGuiApplication::inputMethod()->reset();
    });
    connect(QGuiApplication::inputMethod(), &QInputMethod::visibleChanged, this, [this] {
        const bool visible = QGuiApplication::inputMethod()->isVisible();
        if (!visible) {
            // The keyboard was hidden (by the user, or because the input
            // context went away). Remember it, so that a text field which keeps
            // sending updates cannot bring the panel back on its own.
            m_hiddenByUser = true;
        }
        window()->setVisible(visible);
    });

    connect(&m_input, &InputPlugin::keyPressed, this, [this](QKeyEvent *keyEvent) {
        // qCDebug(PlasmaKeyboard) << "keyPressed. keycode:" << keyEvent->key() << "text:" << keyEvent->text() << "modifiers:" << keyEvent->modifiers();

        // Delegate to overlay controller for diacritics/emoji/text expansion
        if (m_overlayController && m_overlayController->processKeyPress(keyEvent)) {
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

QString InputListenerItem::textBeforeCursor() const
{
    if (!m_input.hasContext()) {
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
    if (word.isEmpty() || !m_input.hasContext()) {
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

    qCDebug(PlasmaKeyboard) << "applyPrediction: replacing" << prefix << "with" << committed;

    // Replace the word that was typed with the suggestion: the replacement is
    // what the clients expect for a completion, and inputMethodEvent() already
    // knows how to send it over the wayland protocol.
    QInputMethodEvent event;
    event.setCommitString(committed, -prefix.size(), prefix.size());
    inputMethodEvent(&event);
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
        Qt::InputMethodHints qtHints;

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

bool InputListenerItem::handleModifiedKey(QKeyEvent *event, bool press)
{
    auto *modifiers = KeyboardModifiers::instance();
    if (!modifiers->ctrl() && !modifiers->alt()) {
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

    // Shift can take part in the combination (e.g. Ctrl+Shift+key).
    const uint32_t depressed = (modifiers->ctrl() ? m_input.controlMask() : 0) | (modifiers->alt() ? m_input.altMask() : 0)
        | ((event->modifiers() & Qt::ShiftModifier) ? m_input.shiftMask() : 0);

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

    // The on-screen Ctrl/Alt keys emit a plain Control/Alt event; use it to
    // toggle the latch (works the same for touch and gamepad activation).
    auto *modifiers = KeyboardModifiers::instance();
    if (event->key() == Qt::Key_Control) {
        modifiers->setCtrl(!modifiers->ctrl());
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Alt) {
        modifiers->setAlt(!modifiers->alt());
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

    if (event->key() == Qt::Key_Control || event->key() == Qt::Key_Alt) {
        // The toggle happened on press; ignore the release.
        event->accept();
        return;
    }

    auto *modifiers = KeyboardModifiers::instance();
    const bool wasLatched = modifiers->ctrl() || modifiers->alt();

    if (handleModifiedKey(event, false)) {
        event->accept();
        return;
    }

    if (wasLatched) {
        // The combination completes on release; clear the latch even if the key
        // could not be mapped, so the modifier never gets stuck.
        modifiers->reset();
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
