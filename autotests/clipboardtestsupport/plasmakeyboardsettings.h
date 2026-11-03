// SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
#pragma once
#include <QObject>

// Test-only settings; never opens or changes the desktop configuration.
class PlasmaKeyboardSettings : public QObject
{
    Q_OBJECT
public:
    static PlasmaKeyboardSettings *self()
    {
        static PlasmaKeyboardSettings settings;
        return &settings;
    }
    bool clipboardEnabled() const { return m_enabled; }
    void setClipboardEnabled(bool enabled)
    {
        if (enabled != m_enabled) {
            m_enabled = enabled;
            Q_EMIT clipboardEnabledChanged();
        }
    }
Q_SIGNALS:
    void clipboardEnabledChanged();
private:
    bool m_enabled = true;
};
