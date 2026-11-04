/*
    SPDX-FileCopyrightText: 2026 Plasma Keyboard contributors

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QQueue>
#include <QTimer>

/**
 * A minimal virtual keyboard on /dev/uinput.
 *
 * The Wayland input-method-v1 protocol this application otherwise talks to
 * the compositor over only delivers keys to whichever client currently holds
 * a text input focus -- it was never meant to reach the compositor's own
 * global shortcuts (Meta+L to lock the session, Meta+D to show the desktop,
 * the bare Meta tap that opens the application launcher, Alt+Tab to switch
 * windows; see ~/.config/kglobalshortcutsrc). Those are matched against real
 * input, which a uinput device provides: the kernel (and therefore libinput
 * and KWin) sees it exactly like a hardware keyboard, so a key sent through
 * it can reach global shortcuts that input-method-v1 cannot.
 *
 * Used for every Ctrl/Alt/Meta combination (see InputListenerItem), so it
 * also covers plain application-level shortcuts like Ctrl+C -- that keeps
 * working the same, since the compositor forwards a real key press to
 * whichever client has keyboard focus, same as input-method-v1 did for that
 * case. It is only a fallback to the input-method-v1 path (kept for when
 * /dev/uinput cannot be opened) that is now the exception rather than the
 * rule.
 *
 * Events are queued and written one per timer tick, so a combination never
 * blocks the caller -- this object lives on the GUI thread, where a blocking
 * delay between events would stall the keyboard itself.
 */
class UinputKeyboard : public QObject
{
    Q_OBJECT

public:
    explicit UinputKeyboard(QObject *parent = nullptr);
    ~UinputKeyboard() override;

    UinputKeyboard(const UinputKeyboard &) = delete;
    UinputKeyboard &operator=(const UinputKeyboard &) = delete;

    //! False if /dev/uinput could not be opened or the virtual device could
    //! not be created (e.g. no access, no uinput module); callers must treat
    //! that as "the feature is unavailable" rather than an error.
    bool isValid() const;

    //! Queues a key-down for the given Linux evdev key code (KEY_* from
    //! <linux/input-event-codes.h>), followed by a device sync. No-op if
    //! !isValid(). Returns as soon as the event is queued.
    void pressKey(int keyCode);

    //! Queues a key-up for the given evdev key code and syncs the device.
    //! No-op if !isValid().
    void releaseKey(int keyCode);

private:
    void enqueue(int keyCode, int value);
    void flushNextEvent();
    void writeEvent(int keyCode, int value);

    //! Minimum spacing between two events. A human pressing a combination
    //! always leaves tens of milliseconds of overlap between the keys; see
    //! flushNextEvent() for why the compositor needs that overlap to be
    //! ordered rather than simultaneous.
    static constexpr int interEventDelayMs = 10;

    int m_fd = -1;
    QQueue<std::pair<int, int>> m_pending;
    QTimer m_pumpTimer;
};
