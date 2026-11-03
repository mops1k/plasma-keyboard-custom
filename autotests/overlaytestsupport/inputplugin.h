// SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
#pragma once
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

// Recording facade for the real OverlayController, without a compositor/UI.
class InputPlugin : public QObject
{
    Q_OBJECT
public:
    enum KeyState {
        Released = 0,
        Pressed = 1,
        Repeated = 2
    };
    enum ContentPurpose {
        content_purpose_normal = 0,
        content_purpose_terminal = 12
    };
    ContentPurpose contentPurpose() const
    {
        return content_purpose_normal;
    }
    void key(KeyState state, quint32 code)
    {
        events.append({code, state});
    }
    void commit(const QString &)
    {
    }
    void deleteSurroundingText(int, int)
    {
    }
    QList<QPair<quint32, KeyState>> events;
Q_SIGNALS:
    void contextAboutToChange();
};
