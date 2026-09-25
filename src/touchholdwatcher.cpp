/*
    SPDX-FileCopyrightText: 2026 Kristen McWilliam <kristen@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "touchholdwatcher.h"

#include "logging.h"

#include <QDir>
#include <QSocketNotifier>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

namespace
{
// The touchscreen is identified by having both BTN_TOUCH and
// ABS_MT_POSITION_X capabilities, plus INPUT_PROP_DIRECT (see isDirectInput()).
bool hasCapability(int fd, int type, unsigned int code)
{
    unsigned char bits[512];
    memset(bits, 0, sizeof(bits));
    if (ioctl(fd, EVIOCGBIT(type, sizeof(bits)), bits) < 0) {
        return false;
    }
    return bits[code / 8] & (1 << (code % 8));
}

// Touchpads can have BTN_TOUCH and ABS_MT too (e.g. InputPlumber's virtual
// touchpad on handhelds); only a touchscreen is a direct input device.
bool isDirectInput(int fd)
{
    unsigned char props[INPUT_PROP_CNT / 8 + 1];
    memset(props, 0, sizeof(props));
    if (ioctl(fd, EVIOCGPROP(sizeof(props)), props) < 0) {
        return false;
    }
    return props[INPUT_PROP_DIRECT / 8] & (1 << (INPUT_PROP_DIRECT % 8));
}
}

TouchHoldWatcher::TouchHoldWatcher(QObject *parent)
    : QObject(parent)
{
    connect(&m_holdTimer, &QTimer::timeout, this, [this] {
        // Stay armed: the gesture should also re-open the keyboard after
        // the user dismissed it. While the keyboard is visible, show() is
        // a no-op, so repeat fires are harmless.
        if (m_armed && m_touchDown) {
            m_holdTimer.stop();
            Q_EMIT longPress();
        }
    });
}

TouchHoldWatcher::~TouchHoldWatcher()
{
    delete m_notifier;
    if (m_fd >= 0) {
        ::close(m_fd);
    }
}

bool TouchHoldWatcher::openTouchscreen()
{
    const auto devices = QDir(QStringLiteral("/dev/input")).entryList({QStringLiteral("event*")}, QDir::System, QDir::Name);
    for (const QString &name : devices) {
        const QString path = QStringLiteral("/dev/input/") + name;
        const int fd = ::open(path.toUtf8().constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }
        if (hasCapability(fd, EV_ABS, ABS_MT_POSITION_X) && hasCapability(fd, EV_KEY, BTN_TOUCH) && isDirectInput(fd)) {
            m_fd = fd;
            m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
            connect(m_notifier, &QSocketNotifier::activated, this, &TouchHoldWatcher::readEvents);
            return true;
        }
        ::close(fd);
    }

    qCWarning(PlasmaKeyboard) << "TouchHoldWatcher: no readable touchscreen device found";
    return false;
}

bool TouchHoldWatcher::arm(int thresholdMs)
{
    if (m_fd < 0 && !openTouchscreen()) {
        return false;
    }

    m_thresholdMs = thresholdMs;
    m_armed = true;
    qCDebug(PlasmaKeyboard) << "TouchHoldWatcher armed, touchscreen fd=" << m_fd << "touchDown=" << m_touchDown;
    if (m_touchDown) {
        m_holdTimer.start(m_thresholdMs);
    }
    return true;
}

void TouchHoldWatcher::disarm()
{
    m_armed = false;
    m_holdTimer.stop();
}

bool TouchHoldWatcher::isArmed() const
{
    return m_armed;
}

bool TouchHoldWatcher::touchIsDown() const
{
    return m_touchDown;
}

void TouchHoldWatcher::readEvents()
{
    struct input_event events[16];

    while (true) {
        const ssize_t n = ::read(m_fd, events, sizeof(events));
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                break;
            }
            // The device is gone (re-plugged, permissions changed); stop reading.
            delete m_notifier;
            m_notifier = nullptr;
            ::close(m_fd);
            m_fd = -1;
            break;
        }
        if (n == 0) {
            break;
        }

        const auto count = static_cast<size_t>(n) / sizeof(events[0]);
        for (size_t i = 0; i < count; ++i) {
            const input_event &ev = events[i];
            if (ev.type != EV_KEY || ev.code != BTN_TOUCH) {
                continue;
            }
            if (ev.value == 1) {
                m_touchDown = true;
                if (m_armed) {
                    m_holdTimer.start(m_thresholdMs);
                }
            } else if (ev.value == 0) {
                m_touchDown = false;
                m_holdTimer.stop();
            }
        }
    }
}
