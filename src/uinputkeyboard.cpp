/*
    SPDX-FileCopyrightText: 2026 Plasma Keyboard contributors

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "uinputkeyboard.h"
#include "logging.h"

#include <cstring>
#include <fcntl.h>
#include <linux/uinput.h>
#include <unistd.h>
#include <utility>

UinputKeyboard::UinputKeyboard(QObject *parent)
    : QObject(parent)
{
    connect(&m_pumpTimer, &QTimer::timeout, this, &UinputKeyboard::flushNextEvent);

    m_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (m_fd < 0) {
        qCWarning(PlasmaKeyboard) << "Could not open /dev/uinput, Meta shortcuts will not reach the compositor's global shortcuts:" << strerror(errno);
        return;
    }

    bool ok = ioctl(m_fd, UI_SET_EVBIT, EV_KEY) >= 0;
    // Only keys a keyboard can type or combine into shortcuts: Escape through
    // the navigation block. Power, sleep, wake-up and media keys are left out so
    // a bug can never power the machine off through this device.
    for (int key = 1; key <= KEY_COMPOSE && ok; ++key) {
        if (key == KEY_POWER || key == KEY_MUTE || key == KEY_VOLUMEDOWN || key == KEY_VOLUMEUP) {
            continue;
        }
        ok = ioctl(m_fd, UI_SET_KEYBIT, key) >= 0;
    }
    if (!ok) {
        qCWarning(PlasmaKeyboard) << "Could not configure the uinput virtual keyboard:" << strerror(errno);
        close(m_fd);
        m_fd = -1;
        return;
    }

    uinput_setup setup{};
    std::strncpy(setup.name, "plasma-keyboard-custom Meta", UINPUT_MAX_NAME_SIZE - 1);
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x0000;
    setup.id.product = 0x0000;
    setup.id.version = 1;

    if (ioctl(m_fd, UI_DEV_SETUP, &setup) < 0 || ioctl(m_fd, UI_DEV_CREATE) < 0) {
        qCWarning(PlasmaKeyboard) << "Could not create the uinput virtual keyboard, Meta shortcuts will not reach the compositor's global shortcuts:"
                                  << strerror(errno);
        close(m_fd);
        m_fd = -1;
    }
}

UinputKeyboard::~UinputKeyboard()
{
    if (m_fd >= 0) {
        ioctl(m_fd, UI_DEV_DESTROY);
        close(m_fd);
    }
}

bool UinputKeyboard::isValid() const
{
    return m_fd >= 0;
}

void UinputKeyboard::enqueue(int keyCode, int value)
{
    if (m_fd < 0) {
        return;
    }

    m_pending.append(std::make_pair(keyCode, value));
    // The first event of a burst goes out immediately; only the follow-up
    // events are spaced out. Spacing is asynchronous on purpose: this object
    // is owned by the keyboard's GUI thread, and the old blocking 10 ms pause
    // after every event stalled keyPressEvent() for the whole combination.
    if (!m_pumpTimer.isActive()) {
        flushNextEvent();
        if (!m_pending.isEmpty()) {
            m_pumpTimer.start(interEventDelayMs);
        }
    }
}

void UinputKeyboard::flushNextEvent()
{
    if (m_pending.isEmpty()) {
        m_pumpTimer.stop();
        return;
    }
    const auto [keyCode, value] = m_pending.dequeue();
    writeEvent(keyCode, value);
}

void UinputKeyboard::writeEvent(int keyCode, int value)
{
    input_event event{};
    event.type = EV_KEY;
    event.code = static_cast<uint16_t>(keyCode);
    event.value = value;
    if (write(m_fd, &event, sizeof(event)) != sizeof(event)) {
        qCWarning(PlasmaKeyboard) << "Failed to write a uinput key event:" << strerror(errno);
        m_pending.clear();
        m_pumpTimer.stop();
        return;
    }

    input_event sync{};
    sync.type = EV_SYN;
    sync.code = SYN_REPORT;
    sync.value = 0;
    if (write(m_fd, &sync, sizeof(sync)) != sizeof(sync)) {
        qCWarning(PlasmaKeyboard) << "Failed to write a uinput sync event:" << strerror(errno);
        m_pending.clear();
        m_pumpTimer.stop();
    }
}

void UinputKeyboard::pressKey(int keyCode)
{
    enqueue(keyCode, 1);
}

void UinputKeyboard::releaseKey(int keyCode)
{
    enqueue(keyCode, 0);
}
