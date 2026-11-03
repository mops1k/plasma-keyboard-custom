/*
    SPDX-FileCopyrightText: 2024 Aleix Pol i Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2025 Devin Lin <devin@kde.org>
    SPDX-FileCopyrightText: 2025 Kristen McWilliam <kristen@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "focusededitabletracker.h"
#include "sessionlockstate.h"
#include "touchholdwatcher.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QVirtualKeyboardInputEngine>
#include <qqmlintegration.h>

#include <xkbcommon/xkbcommon.h>

#include "inputplugin.h"
#include "uinputkeyboard.h"

#include <memory>

class ClipboardHistory;
class OverlayController;
class QTimer;

/**
 * Global state for the on-screen Ctrl/Alt/Meta keys.
 *
 * The keys in the layouts toggle these values. InputListenerItem consults
 * them when forwarding key events to the compositor, so that a latched
 * modifier is applied to the next key and then cleared.
 */
class KeyboardModifiers : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ctrl READ ctrl WRITE setCtrl NOTIFY ctrlChanged)
    Q_PROPERTY(bool alt READ alt WRITE setAlt NOTIFY altChanged)
    Q_PROPERTY(bool meta READ meta WRITE setMeta NOTIFY metaChanged)
    //! True while a gamepad is available, so key panels can show button hints.
    Q_PROPERTY(bool gamepadAvailable READ gamepadAvailable WRITE setGamepadAvailable NOTIFY gamepadAvailableChanged)

    //! True while the gamepad focus is in the rows above the keyboard: the
    //! keyboard then hides its own navigation highlight.
    Q_PROPERTY(bool extraRowsFocused READ extraRowsFocused WRITE setExtraRowsFocused NOTIFY extraRowsFocusedChanged)

public:
    static KeyboardModifiers *instance();

    bool ctrl() const;
    void setCtrl(bool ctrl);

    bool alt() const;
    void setAlt(bool alt);

    bool meta() const;
    void setMeta(bool meta);

    bool gamepadAvailable() const;
    void setGamepadAvailable(bool available);

    bool extraRowsFocused() const;
    void setExtraRowsFocused(bool focused);

    Q_INVOKABLE void reset();

Q_SIGNALS:
    void ctrlChanged();
    void altChanged();
    void metaChanged();
    void gamepadAvailableChanged();
    void extraRowsFocusedChanged();

private:
    explicit KeyboardModifiers(QObject *parent = nullptr);

    bool m_ctrl = false;
    bool m_alt = false;
    bool m_meta = false;
    bool m_gamepadAvailable = false;
    bool m_extraRowsFocused = false;
};

/**
 * Requests the keyboard to be shown on the next input activation, bypassing
 * the "open on long press" behaviour. Used by the global shortcut, which
 * force-activates the input method and would otherwise be suppressed.
 */
void setInputPanelForceShowOnNextActivation();

class InputListenerItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVirtualKeyboardInputEngine *engine WRITE setEngine)
    Q_PROPERTY(bool keyboardNavigationActive MEMBER m_keyboardNavigationActive)

    /**
     * Controller for overlay popups (diacritics, emoji, text expansion).
     *
     * Exposed to QML for connecting overlay windows.
     */
    Q_PROPERTY(OverlayController *overlayController READ overlayController CONSTANT)

    /**
     * Recent clipboard entries, shown in a row above the keyboard.
     */
    Q_PROPERTY(ClipboardHistory *clipboardHistory READ clipboardHistory CONSTANT)
    Q_PROPERTY(bool auxiliaryUiAllowed READ predictionActive NOTIFY predictionTargetChanged)

    /**
     * Whether the focused field is a password or asks for sensitive data, so
     * the keyboard must not remember what is typed into it.
     */
    Q_PROPERTY(bool sensitiveInput READ sensitiveInput)

    /**
     * The word being typed: the letters before the cursor, together with the
     * hyphens and apostrophes inside them («что-то», «don't»). Empty when the
     * field is not being typed into or the cursor is not after a word.
     */
    Q_PROPERTY(QString predictionPrefix READ predictionPrefix NOTIFY predictionPrefixChanged)
    Q_PROPERTY(bool predictionActive READ predictionActive NOTIFY predictionTargetChanged)

    /**
     * The word before the one being typed: the last word of the text before
     * the current one, with the separators between them left out («привет, как»
     * gives «привет»). Empty when there is no word before the current one. It
     * is what the next-word predictions are looked up with.
     */
    Q_PROPERTY(QString predictionContext READ predictionContext NOTIFY predictionContextChanged)

public:
    InputListenerItem();

    void setEngine(QVirtualKeyboardInputEngine *engine);

    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;

    /**
     * Synthesizes a key press/release pair, as if it came from the virtual
     * keyboard. Used by the gamepad handler to emit e.g. Backspace/Space.
     */
    Q_INVOKABLE void sendKeyEvent(int key, const QString &text);

    /**
     * Inserts @p text into the focused field, as if it had been typed.
     * Used by the clipboard row to paste a copied text.
     */
    Q_INVOKABLE void commitText(const QString &text);
    Q_INVOKABLE void pasteClipboardEntry(int row);

    bool predictionActive() const
    {
        return m_sessionLockState.allowsPredictions() && m_input.predictionsAllowed();
    }

    /**
     * The word being typed, as offered to the predictive text input.
     */
    QString predictionPrefix() const;

    /**
     * The word before the word being typed, as offered to the next-word
     * predictions.
     */
    QString predictionContext() const;

    bool sensitiveInput() const;

    /**
     * Whether the cursor stands at the beginning of a sentence: the field is
     * empty or the text before it ends a sentence («. », «! », «? » or a line
     * break). Used to capitalise a word the keyboard inserts as a whole.
     */
    Q_INVOKABLE bool atSentenceStart() const;

    /**
     * Replaces the word being typed with @p word, the way a suggestion is
     * taken. The word carries the case it should be typed in.
     */
    Q_INVOKABLE void applyPrediction(const QString &word);

    /**
     * Types the word of a swipe gesture: replaces the word being typed (if any)
     * with @p word and adds the space after it, and remembers what was inserted
     * so that an alternative can replace it again.
     */
    Q_INVOKABLE void commitSwipeWord(const QString &word);

    /**
     * Replaces the word the last swipe inserted with @p word, keeping the space
     * after it. Does nothing when the text no longer ends with that word (the
     * person typed something else in between).
     */
    Q_INVOKABLE void replaceSwipeWord(const QString &word);

    /**
     * The alternate characters a key offers, in the form they would be typed.
     *
     * @p alternatives is the effectiveAlternativeKeys of the highlighted key
     * (the characters of its alternativeKeys without the key's own text), which
     * is exactly what the long press on the on-screen key shows. Shift is
     * applied the same way Qt Virtual Keyboard applies it in its own popup.
     *
     * @param alternatives Raw alternative characters of the key.
     * @param uppercase Whether the keyboard currently types upper case.
     * @return The characters to offer, empty when the key has none.
     */
    Q_INVOKABLE QStringList alternatesFor(const QVariantList &alternatives, bool uppercase) const;

    /**
     * Get the overlay controller.
     */
    OverlayController *overlayController() const;

    /**
     * Get the model of recent clipboard entries.
     */
    ClipboardHistory *clipboardHistory() const;

Q_SIGNALS:
    void keyNavigationPressed(int key);
    void keyNavigationReleased(int key);
    void predictionPrefixChanged();
    void predictionTargetChanged();
    void predictionContextChanged();

private:
    /**
     * Sends the key described by @p event as a real key event with the
     * currently latched Ctrl/Alt/Meta modifiers applied.
     *
     * Returns true if the event was handled, in which case the caller must
     * not process it any further.
     */
    bool handleModifiedKey(QKeyEvent *event, bool press);

    //! Starts the hold-threshold timer for a just-latched Ctrl/Alt/Meta key.
    //! The tap already latched the modifier (it stays engaged, like a lock
    //! key, until a combination completes or the same key is tapped again to
    //! cancel it -- see keyPressEvent()/keyReleaseEvent()) and pressed it for
    //! real through uinput, so it already behaves like a physical modifier
    //! held down for games and applications that read the raw key state. If
    //! the key is still (physically) held when the timer fires, that is a
    //! genuine hold rather than a tap: keyReleaseEvent() then ends it (and
    //! the latch) the moment the finger lifts, instead of leaving it engaged
    //! for whatever is typed next.
    void armModifierHoldTimer(Qt::Key key);

    //! Stops the hold timer and clears the state armModifierHoldTimer() set,
    //! so it cannot fire later for a modifier that a combination (or a reset
    //! elsewhere) already consumed.
    void cancelModifierHoldTimer();

    //! Re-reads the word being typed and the word before it, and tells QML
    //! when either of them changed.
    void updatePredictionWords();

    //! The text of the field from its beginning up to the cursor.
    QString textBeforeCursor() const;

    InputPlugin m_input;
    SessionLockState &m_sessionLockState;
    OverlayController *m_overlayController = nullptr;
    ClipboardHistory *m_clipboardHistory = nullptr;
    TouchHoldWatcher m_touchHold;
    FocusedEditableTracker m_focusedEditable;
    bool m_longPressNeedsHitTest = false;
    bool m_keyboardNavigationActive = false;

    //! The word the predictive text input currently suggests for.
    QString m_predictionPrefix;

    //! The word the next-word predictions are currently looked up with.
    QString m_predictionContext;

    //! The text (word and space) the last swipe inserted, so that one of its
    //! alternatives can replace it.
    QString m_committedSwipe;

    //! Whether the focused field is a password or asks for sensitive data.
    mutable bool m_sensitiveInput = false;

    //! True after the keyboard was hidden (by the user or by the system) and
    //! until the next input activation. A focused text field keeps sending
    //! updates, and those must not bring the panel back on their own.
    bool m_hiddenByUser = false;

    //! Delays hiding the keyboard after the input context goes away, so a
    //! momentary loss of focus does not hide it.
    QTimer *m_hideDelay = nullptr;

    //! Fires modifierHoldThresholdMs after a Ctrl/Alt/Meta key is latched; see
    //! armModifierHoldTimer().
    QTimer *m_modifierHoldTimer = nullptr;
    //! Which modifier key the hold timer above is currently running for, or 0.
    Qt::Key m_pendingModifierKey = Qt::Key(0);
    //! True once the hold timer has fired for m_pendingModifierKey: it is
    //! being genuinely held down (not just latched from a tap), so releasing
    //! it must end the hold for real instead of leaving it latched.
    bool m_modifierHeldForReal = false;

    //! Virtual keyboard used for Ctrl/Alt/Meta combinations; see
    //! UinputKeyboard's own documentation for why. Null (and every call
    //! guarded) is a supported state: it just falls back to the older
    //! input-method-v1 path in handleModifiedKey(), same as before this
    //! existed.
    std::unique_ptr<UinputKeyboard> m_uinput;
    //! Whether each modifier's uinput key is currently pressed down.
    bool m_uinputCtrlHeld = false;
    bool m_uinputAltHeld = false;
    bool m_uinputMetaHeld = false;

    //! Presses @p key's uinput key (Control_L/Alt_L/Super_L) if it is not
    //! already held. Safe to call unconditionally; a no-op for any other key
    //! or without a usable m_uinput.
    void pressUinputModifier(Qt::Key key);

    //! Releases @p key's uinput key if pressUinputModifier() left it held.
    //! Safe to call unconditionally.
    void releaseUinputModifierIfHeld(Qt::Key key);

    //! releaseUinputModifierIfHeld() for Control, Alt and Meta together, so a
    //! completed (or abandoned) combination can never leave one stuck down.
    void releaseAllHeldUinputModifiers();
};
