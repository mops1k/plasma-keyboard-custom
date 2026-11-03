/*
    SPDX-FileCopyrightText: 2026 Chechulin Serhii

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QObject>
#include <QRect>

#include <memory>

class QDBusConnection;
class QDBusMessage;
class QDBusVariant;

/**
 * Remembers the screen rectangle of the focused editable AT-SPI object.
 *
 * The input-method-v1 protocol does not expose the target widget geometry.
 * AT-SPI fills that gap for a field which remains focused after the keyboard
 * has been hidden, without treating an arbitrary hold in the same window as a
 * request to show the keyboard.
 */
class FocusedEditableTracker : public QObject
{
    Q_OBJECT

public:
    explicit FocusedEditableTracker(QObject *parent = nullptr);
    // Use an already connected accessibility bus (also useful for isolated tests).
    explicit FocusedEditableTracker(const QDBusConnection &bus, QObject *parent = nullptr);
    ~FocusedEditableTracker() override;

    bool containsNormalizedTouch(const QPointF &position) const;

private Q_SLOTS:
    void stateChanged(const QString &state, int enabled, int detail, const QDBusVariant &data, const QVariantMap &properties, const QDBusMessage &message);

private:
    void initialize(const QDBusConnection &bus);

    quint64 m_focusGeneration = 0;
    std::unique_ptr<QDBusConnection> m_bus;
    QString m_connectionName;
    QString m_focusedService;
    QString m_focusedPath;
    QRect m_focusedEditableRect;
};
