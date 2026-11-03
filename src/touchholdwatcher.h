/*
    SPDX-FileCopyrightText: 2026 Kristen McWilliam <kristen@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <QSet>
#include <QTimer>

class QSocketNotifier;

/**
 * Watches the touchscreen evdev device for touch down/up events.
 *
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

    /// Whether a touch is currently down on the touchscreen.
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
    bool openTouchscreen();
    void readEvents();
    void cancelGesture();
    void updatePosition(int code, int value);

    int m_fd = -1;
    QSocketNotifier *m_notifier = nullptr;
    bool m_touchDown = false;
    bool m_armed = false;
    bool m_gestureMoved = false;
    bool m_currentXValid = false;
    bool m_currentYValid = false;
    bool m_haveX = false;
    bool m_haveY = false;
    int m_x = 0;
    int m_y = 0;
    int m_startX = 0;
    int m_startY = 0;
    int m_xSlop = 1;
    int m_ySlop = 1;
    int m_xMinimum = 0;
    int m_xMaximum = 1;
    int m_yMinimum = 0;
    int m_yMaximum = 1;
    int m_currentSlot = 0;
    QSet<int> m_activeSlots;
    QElapsedTimer m_touchElapsed;
    QElapsedTimer m_releaseElapsed;
    int m_completedTouchDurationMs = -1;
    bool m_persistent = false;
    int m_thresholdMs = defaultThresholdMs;
    QTimer m_holdTimer;
};
