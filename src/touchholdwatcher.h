/*
    SPDX-FileCopyrightText: 2026 Chechulin Serhii

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <QSet>
#include <QTimer>
#include <memory>
#include <vector>

class QSocketNotifier;

/**
 * Watches touchscreen evdev devices for touch down/up events.
 *
 * Supports multiple connected direct-input touchscreens (e.g. built-in + external).
 * When armed, it emits longPress() once a touch is held down for longer
 * than the configured threshold without moving. An arm is valid for the
 * current touch only; release, movement, or a second contact cancels it.
 * The caller arms it only when the compositor activates a text input context,
 * which is what confines the otherwise-global evdev gesture to text fields.
 */
class TouchHoldWatcher : public QObject
{
    Q_OBJECT

public:
    static constexpr int defaultThresholdMs = 600;

    explicit TouchHoldWatcher(QObject *parent = nullptr);
    ~TouchHoldWatcher() override;

    TouchHoldWatcher(const TouchHoldWatcher &) = delete;
    TouchHoldWatcher &operator=(const TouchHoldWatcher &) = delete;

    /**
     * Start watching for a long touch.
     *
     * @param thresholdMs How long the touch must be held to emit longPress().
     * @return False if no usable touchscreen could be opened, in which case
     * the caller should fall back to its normal behaviour.
     */
    bool arm(int thresholdMs = defaultThresholdMs);

    /** Keep recognising successive holds until disarm() is called. */
    bool watchContinuously(int thresholdMs = defaultThresholdMs);

    /// Stop watching; any pending hold timer is cancelled.
    void disarm();

    bool isArmed() const;

    /// Whether a touch is currently down on any touchscreen.
    bool touchIsDown() const;

    /** Current touch position, normalised to the touchscreen's axis ranges. */
    QPointF normalizedPosition() const;

    /**
     * Consume a stationary touch which ended just before the text input
     * protocol reported the field activation/update.
     */
    bool takeCompletedLongPress(int thresholdMs);

Q_SIGNALS:
    /// A touch was held down for at least the threshold duration.
    void longPress();

private:
    struct Device
    {
        int fd = -1;
        QSocketNotifier *notifier = nullptr;
        QString path;
        int xMinimum = 0;
        int xMaximum = 1;
        int yMinimum = 0;
        int yMaximum = 1;
        int xSlop = 1;
        int ySlop = 1;
        bool touchDown = false;
        bool gestureMoved = false;
        bool currentXValid = false;
        bool currentYValid = false;
        bool haveX = false;
        bool haveY = false;
        int x = 0;
        int y = 0;
        int startX = 0;
        int startY = 0;
        int currentSlot = 0;
        QSet<int> activeSlots;
        QElapsedTimer touchElapsed;
    };

    bool openTouchscreens();
    void readEvents(Device &dev);
    void cancelGesture(Device &dev);
    void updatePosition(Device &dev, int code, int value);

    std::vector<std::unique_ptr<Device>> m_devices;
    Device *m_activeDevice = nullptr;
    Device *m_lastCompletedDevice = nullptr;
    bool m_armed = false;
    QElapsedTimer m_releaseElapsed;
    int m_completedTouchDurationMs = -1;
    bool m_persistent = false;
    int m_thresholdMs = defaultThresholdMs;
    QTimer m_holdTimer;
};
