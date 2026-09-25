/*
    SPDX-FileCopyrightText: 2024 Aleix Pol i Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2025 Devin Lin <devin@kde.org>
    SPDX-FileCopyrightText: 2025 Kristen McWilliam <kristen@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "touchholdwatcher.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QVirtualKeyboardInputEngine>
#include <qqmlintegration.h>

#include <xkbcommon/xkbcommon.h>

#include "inputplugin.h"

class ClipboardHistory;
class OverlayController;
class QTimer;

/**
 * Global state for the on-screen Ctrl/Alt keys.
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

    bool gamepadAvailable() const;
    void setGamepadAvailable(bool available);

    bool extraRowsFocused() const;
    void setExtraRowsFocused(bool focused);

    Q_INVOKABLE void reset();

Q_SIGNALS:
    void ctrlChanged();
    void altChanged();
    void gamepadAvailableChanged();
    void extraRowsFocusedChanged();

private:
    explicit KeyboardModifiers(QObject *parent = nullptr);

    bool m_ctrl = false;
    bool m_alt = false;
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

    /**
     * The word being typed: the letters before the cursor, together with the
     * hyphens and apostrophes inside them («что-то», «don't»). Empty when the
     * field is not being typed into or the cursor is not after a word.
     */
    Q_PROPERTY(QString predictionPrefix READ predictionPrefix NOTIFY predictionPrefixChanged)

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

    /**
     * The word being typed, as offered to the predictive text input.
     */
    QString predictionPrefix() const;

    /**
     * The word before the word being typed, as offered to the next-word
     * predictions.
     */
    QString predictionContext() const;

    /**
     * Replaces the word being typed with @p word, the way a suggestion is
     * taken. The word carries the case it should be typed in.
     */
    Q_INVOKABLE void applyPrediction(const QString &word);

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
    void predictionContextChanged();

private:
    /**
     * Sends the key described by @p event as a real key event with the
     * currently latched Ctrl/Alt modifiers applied.
     *
     * Returns true if the event was handled, in which case the caller must
     * not process it any further.
     */
    bool handleModifiedKey(QKeyEvent *event, bool press);

    //! Re-reads the word being typed and the word before it, and tells QML
    //! when either of them changed.
    void updatePredictionWords();

    //! The text of the field from its beginning up to the cursor.
    QString textBeforeCursor() const;

    InputPlugin m_input;
    OverlayController *m_overlayController = nullptr;
    ClipboardHistory *m_clipboardHistory = nullptr;
    TouchHoldWatcher m_touchHold;
    bool m_keyboardNavigationActive = false;

    //! The word the predictive text input currently suggests for.
    QString m_predictionPrefix;

    //! The word the next-word predictions are currently looked up with.
    QString m_predictionContext;

    //! True after the keyboard was hidden (by the user or by the system) and
    //! until the next input activation. A focused text field keeps sending
    //! updates, and those must not bring the panel back on their own.
    bool m_hiddenByUser = false;

    //! Delays hiding the keyboard after the input context goes away, so a
    //! momentary loss of focus does not hide it.
    QTimer *m_hideDelay = nullptr;
};
