/*
    SPDX-FileCopyrightText: 2026 Plasma Keyboard contributors

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <qqmlintegration.h>

class QTimer;

/**
 * Translates gamepad input into on-screen keyboard actions.
 *
 * On systems using InputPlumber, the built-in controller is owned by the
 * InputPlumber daemon, which exposes its events on the system bus through an
 * "dbus" target device (interface org.shadowblip.Input.DBusDevice, signal
 * InputEvent(string, double)). This class subscribes to that stream and emits
 * high level signals that the keyboard UI maps onto Qt Virtual Keyboard's
 * navigation and input.
 *
 * When InputPlumber is not available the handler stays idle.
 */
class GamepadHandler : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    /*! Whether an InputPlumber dbus target device was found. */
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)

    /**
     * Whether the keyboard is on screen, i.e. whether the gamepad should drive
     * it. The compositor stops reporting the keyboard as visible as soon as the
     * text input goes away, but with hideOnInputFocusLoss off the keyboard stays
     * on screen and has to stay controllable: the panel binds this to the
     * keyboard window rather than to what the compositor reports.
     */
    Q_PROPERTY(bool keyboardOnScreen READ keyboardOnScreen WRITE setKeyboardOnScreen NOTIFY keyboardOnScreenChanged)

public:
    explicit GamepadHandler(QObject *parent = nullptr);
    ~GamepadHandler() override;

    bool isAvailable() const;

    /**
     * While active (keyboard shown), gamepad input is intercepted by
     * InputPlumber and no longer reaches the game/Steam, so that the buttons
     * only drive the on-screen keyboard. The previous intercept mode is
     * restored when deactivated.
     */
    Q_INVOKABLE void setActive(bool active);
    bool isActive() const;

    bool keyboardOnScreen() const;
    void setKeyboardOnScreen(bool onScreen);

    /**
     * Whether an alternates overlay may be opened right now, together with the
     * characters it would offer.
     *
     * The panel calls this whenever the highlight moves: the decision has to be
     * made while the button is on its way down, because holding A only starts
     * the delay when the highlighted key has something to choose from.
     */
    Q_INVOKABLE void setAlternatesArmable(bool armable, const QStringList &alternates);

    /**
     * The alternates list is no longer on screen (a character was taken or the
     * list was dismissed), so A goes back to typing the highlighted key.
     */
    Q_INVOKABLE void clearAlternatesOpen();

Q_SIGNALS:
    /*! Move the keyboard focus in the given direction (Qt::Key_Up/Down/Left/Right). */
    void navigate(int key);
    /*! Type the currently highlighted key. */
    void activate();
    /*! Offer the given alternate characters of the key the highlight is on. */
    void showAlternates(const QStringList &alternates);
    /*! Take the character highlighted in the alternates list. */
    void confirmAlternates();
    /*! Delete the character before the cursor. */
    void backspace();
    /*! Insert a space. */
    void space();
    /*! Press Enter/Return. */
    void enter();
    /*! Toggle the shift state. */
    void toggleShift();
    /*! Toggle the symbols (&123) layer. */
    void toggleSymbols();
    /*! Switch to the next keyboard layout. */
    void switchLanguage();
    /*! Go back: leave the voice page, or close the keyboard when it is not up
        (B). */
    void back();
    /*! Hide the keyboard (Start). */
    void hideKeyboard();
    /*! Move the focus between the keyboard and the rows above it (Select). */
    void toggleExtraRows();

    void availableChanged();
    void keyboardOnScreenChanged();

private Q_SLOTS:
    void onInputEvent(const QString &event, double value);
    void onKWinPropertiesChanged(const QString &interfaceName, const QVariantMap &changed, const QStringList &invalidated);
    void refreshFromKWin();

private:
    void handleDirection(int key, bool pressed);
    void handleBackspace(bool pressed);
    void handleAccept(bool pressed);
    void setKwinVisible(bool visible);
    void recomputeActive();
    uint interceptMode() const;
    void setInterceptMode(uint mode);

    bool m_available = false;
    QString m_compositePath;
    bool m_active = false;
    uint m_savedInterceptMode = 0;
    /*! Whether the compositor reports the keyboard as visible. */
    bool m_kwinVisible = false;
    /*! Whether the keyboard window is on screen (see the keyboardOnScreen property). */
    bool m_keyboardOnScreen = false;
    QSet<int> m_pressedDirections;
    QTimer *m_repeatTimer = nullptr;
    QTimer *m_backspaceTimer = nullptr;
    QTimer *m_acceptHoldTimer = nullptr;
    QTimer *m_kwinPollTimer = nullptr;
    int m_repeatKey = 0;
    bool m_backspaceHeld = false;

    /*! Whether the A button is being held right now. */
    bool m_acceptHeld = false;

    /**
     * Whether the A press that is being held has already done its job (it
     * opened the alternates list or took a character out of it), so its
     * release must not type the highlighted key as well.
     */
    bool m_acceptConsumed = false;

    /**
     * Whether the alternates overlay was opened for the A press that is still
     * being held, so the release must not activate the highlighted key.
     */
    bool m_alternatesOpened = false;

    /**
     * Whether an alternates overlay may be opened at all: the panel reports
     * that a key with alternate characters is highlighted and that gamepad
     * alternates are enabled.
     */
    bool m_alternatesArmable = false;

    /*! Alternate characters of the highlighted key. */
    QStringList m_alternates;

    /**
     * Alternates the overlay is offered for, fixed when A went down.
     *
     * The panel keeps polling the highlight while the button is held, so the
     * key under it may change (or blink out for a single poll) before the hold
     * delay is over; the characters have to survive that.
     */
    QStringList m_pressedAlternates;
};
