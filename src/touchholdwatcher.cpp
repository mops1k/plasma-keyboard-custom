/*
    SPDX-FileCopyrightText: 2026 Chechulin Serhii

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "touchholdwatcher.h"

#include "logging.h"

#include <QDir>
#include <QSocketNotifier>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
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

// A hold may wobble slightly, but scrolling and dragging should cancel it.
// Prefer a physical distance when the device reports its resolution; the
// range-based fallback is for touchscreens whose evdev resolution is missing.
int movementSlop(int fd, unsigned int code)
{
    input_absinfo info{};
    if (ioctl(fd, EVIOCGABS(code), &info) < 0) {
        return 1;
    }
    constexpr int slopMillimetres = 8;
    if (info.resolution > 0) {
        return std::max(1, info.resolution * slopMillimetres);
    }
    return std::max(1, (info.maximum - info.minimum) / 100);
}

input_absinfo axisInfo(int fd, unsigned int code)
{
    input_absinfo info{};
    ioctl(fd, EVIOCGABS(code), &info);
    return info;
}
}

TouchHoldWatcher::TouchHoldWatcher(QObject *parent)
    : QObject(parent)
{
    // Start observing before an input context is activated. Opening the devices
    // from arm() is too late for the first touch: the BTN_TOUCH press that
    // focused the text field has already been delivered by then, so the watcher
    // only sees its release and can never recognise that first long press.
    // Failure is harmless; arm() retries in case permissions or devices change.
    openTouchscreens();

    connect(&m_holdTimer, &QTimer::timeout, this, [this] {
        if (m_armed && m_activeDevice && m_activeDevice->touchDown && !m_activeDevice->gestureMoved && m_activeDevice->activeSlots.size() <= 1) {
            // One activation arms exactly one touch. Keeping this armed after
            // success would make a still-focused text field turn every later
            // long press anywhere on the screen into a keyboard request.
            m_holdTimer.stop();
            if (!m_persistent) {
                m_armed = false;
            }
            Q_EMIT longPress();
        }
    });
}

TouchHoldWatcher::~TouchHoldWatcher()
{
    for (auto &dev : m_devices) {
        delete dev->notifier;
        if (dev->fd >= 0) {
            ::close(dev->fd);
        }
    }
}

bool TouchHoldWatcher::openTouchscreens()
{
    const auto devices = QDir(QStringLiteral("/dev/input")).entryList({QStringLiteral("event*")}, QDir::System, QDir::Name);
    for (const QString &name : devices) {
        const QString path = QStringLiteral("/dev/input/") + name;
        const bool alreadyOpen = std::any_of(m_devices.begin(), m_devices.end(), [&](const auto &d) {
            return d->path == path;
        });
        if (alreadyOpen) {
            continue;
        }

        const int fd = ::open(path.toUtf8().constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }
        if (hasCapability(fd, EV_ABS, ABS_MT_POSITION_X) && hasCapability(fd, EV_KEY, BTN_TOUCH) && isDirectInput(fd)) {
            auto dev = std::make_unique<Device>();
            dev->fd = fd;
            dev->path = path;
            const input_absinfo xInfo = axisInfo(fd, ABS_MT_POSITION_X);
            const input_absinfo yInfo = axisInfo(fd, ABS_MT_POSITION_Y);
            dev->xMinimum = xInfo.minimum;
            dev->xMaximum = xInfo.maximum;
            dev->yMinimum = yInfo.minimum;
            dev->yMaximum = yInfo.maximum;
            dev->xSlop = movementSlop(fd, ABS_MT_POSITION_X);
            dev->ySlop = movementSlop(fd, ABS_MT_POSITION_Y);
            dev->notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
            Device *rawDev = dev.get();
            connect(dev->notifier, &QSocketNotifier::activated, this, [this, rawDev] {
                readEvents(*rawDev);
            });
            m_devices.push_back(std::move(dev));
        } else {
            ::close(fd);
        }
    }

    if (m_devices.empty()) {
        qCWarning(PlasmaKeyboard) << "TouchHoldWatcher: no readable touchscreen device found";
        return false;
    }
    return true;
}

bool TouchHoldWatcher::arm(int thresholdMs)
{
    openTouchscreens();
    if (m_devices.empty()) {
        return false;
    }

    // The Wayland activation and evdev fds are separate event sources. Drain
    // evdev here so touchIsDown() is correct even when Wayland happened to be
    // dispatched first for the touch that focused the field.
    for (auto &dev : m_devices) {
        if (dev->fd >= 0) {
            readEvents(*dev);
        }
    }

    m_thresholdMs = thresholdMs;
    m_armed = true;
    m_persistent = false;
    if (m_activeDevice && m_activeDevice->touchDown && !m_activeDevice->gestureMoved && m_activeDevice->activeSlots.size() <= 1) {
        m_holdTimer.start(m_thresholdMs);
    }
    return true;
}

bool TouchHoldWatcher::watchContinuously(int thresholdMs)
{
    if (!arm(thresholdMs)) {
        return false;
    }
    m_persistent = true;
    return true;
}

void TouchHoldWatcher::disarm()
{
    m_armed = false;
    m_persistent = false;
    m_holdTimer.stop();
}

bool TouchHoldWatcher::isArmed() const
{
    return m_armed;
}

bool TouchHoldWatcher::touchIsDown() const
{
    return m_activeDevice != nullptr && m_activeDevice->touchDown;
}

QPointF TouchHoldWatcher::normalizedPosition() const
{
    const Device *dev = m_activeDevice ? m_activeDevice : m_lastCompletedDevice;
    if (!dev) {
        return {};
    }
    const int xRange = dev->xMaximum - dev->xMinimum;
    const int yRange = dev->yMaximum - dev->yMinimum;
    if (xRange <= 0 || yRange <= 0) {
        return {};
    }
    return QPointF(std::clamp(double(dev->x - dev->xMinimum) / xRange, 0.0, 1.0), std::clamp(double(dev->y - dev->yMinimum) / yRange, 0.0, 1.0));
}

bool TouchHoldWatcher::takeCompletedLongPress(int thresholdMs)
{
    // Some clients activate/update text input only after delivering touch-up.
    // Keep the correlation window deliberately shorter than the configured
    // hold, so an unrelated later protocol update cannot consume an old touch
    // from elsewhere. It is bounded because very short/long hold settings
    // should not make event-loop scheduling impossible or retain a gesture for
    // several seconds.
    const int maximumActivationDelayMs = std::clamp(thresholdMs / 2, 50, 250);
    const bool matches = m_completedTouchDurationMs >= thresholdMs && m_releaseElapsed.isValid() && m_releaseElapsed.elapsed() <= maximumActivationDelayMs;
    m_completedTouchDurationMs = -1;
    m_releaseElapsed.invalidate();
    if (matches) {
        disarm();
    }
    return matches;
}

void TouchHoldWatcher::cancelGesture(Device &dev)
{
    dev.gestureMoved = true;
    if (m_activeDevice == &dev) {
        m_holdTimer.stop();
    }
}

void TouchHoldWatcher::updatePosition(Device &dev, int code, int value)
{
    if (code == ABS_MT_POSITION_X) {
        dev.x = value;
        dev.currentXValid = true;
        if (!dev.touchDown) {
            return;
        }
        if (!dev.haveX) {
            dev.startX = value;
            dev.haveX = true;
        }
    } else if (code == ABS_MT_POSITION_Y) {
        dev.y = value;
        dev.currentYValid = true;
        if (!dev.touchDown) {
            return;
        }
        if (!dev.haveY) {
            dev.startY = value;
            dev.haveY = true;
        }
    }

    if ((dev.haveX && std::abs(dev.x - dev.startX) > dev.xSlop) || (dev.haveY && std::abs(dev.y - dev.startY) > dev.ySlop)) {
        cancelGesture(dev);
    }
}

void TouchHoldWatcher::readEvents(Device &dev)
{
    struct input_event events[16];

    while (true) {
        const ssize_t n = ::read(dev.fd, events, sizeof(events));
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                break;
            }
            // The device is gone (re-plugged, permissions changed); stop reading.
            delete dev.notifier;
            dev.notifier = nullptr;
            ::close(dev.fd);
            dev.fd = -1;
            if (m_activeDevice == &dev) {
                m_activeDevice = nullptr;
                m_holdTimer.stop();
            }
            break;
        }
        if (n == 0) {
            break;
        }

        const auto count = static_cast<size_t>(n) / sizeof(events[0]);
        for (size_t i = 0; i < count; ++i) {
            const input_event &ev = events[i];

            if (ev.type == EV_ABS) {
                if (ev.code == ABS_MT_SLOT) {
                    dev.currentSlot = ev.value;
                } else if (ev.code == ABS_MT_TRACKING_ID) {
                    if (ev.value >= 0) {
                        dev.activeSlots.insert(dev.currentSlot);
                        if (dev.activeSlots.size() > 1) {
                            cancelGesture(dev);
                        }
                    } else {
                        dev.activeSlots.remove(dev.currentSlot);
                    }
                } else if (ev.code == ABS_MT_POSITION_X || ev.code == ABS_MT_POSITION_Y) {
                    updatePosition(dev, ev.code, ev.value);
                }
                continue;
            }

            if (ev.type != EV_KEY || ev.code != BTN_TOUCH) {
                continue;
            }

            if (ev.value == 1) {
                dev.touchDown = true;
                dev.gestureMoved = false;
                m_activeDevice = &dev;
                m_completedTouchDurationMs = -1;
                m_releaseElapsed.invalidate();
                dev.touchElapsed.restart();
                dev.haveX = dev.currentXValid;
                dev.haveY = dev.currentYValid;
                dev.startX = dev.x;
                dev.startY = dev.y;
                if (m_armed) {
                    m_holdTimer.start(m_thresholdMs);
                }
            } else if (ev.value == 0) {
                if (!dev.gestureMoved && dev.touchElapsed.isValid()) {
                    m_completedTouchDurationMs = static_cast<int>(dev.touchElapsed.elapsed());
                    m_releaseElapsed.restart();
                    m_lastCompletedDevice = &dev;
                } else {
                    m_completedTouchDurationMs = -1;
                    m_releaseElapsed.invalidate();
                }
                dev.touchElapsed.invalidate();
                dev.touchDown = false;
                dev.activeSlots.clear();
                dev.currentXValid = false;
                dev.currentYValid = false;
                if (m_activeDevice == &dev) {
                    m_activeDevice = nullptr;
                    if (m_persistent) {
                        m_holdTimer.stop();
                    } else {
                        disarm();
                    }
                }
            }
        }
    }
}
