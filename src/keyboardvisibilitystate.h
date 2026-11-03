/*
    SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once
#include <optional>

/** Edge-triggered compositor visibility, independent of local Qt hide echoes. */
class KeyboardVisibilityState
{
public:
    void afterUnlock()
    {
        m_waitForHide = true;
    }
    bool compositorVisible() const
    {
        return m_lastVisible;
    }

    std::optional<bool> update(bool visible, bool known, bool locked, bool preparing, bool hideOnFocusLoss)
    {
        if (!visible) {
            m_waitForHide = false;
        }
        const bool effective = visible && known && !preparing && !m_waitForHide;
        if (effective == m_lastVisible) {
            return std::nullopt;
        }
        m_lastVisible = effective;
        // Desktop stickiness is a focus-loss preference, never a veto of the
        // lock-screen's explicit hide button or the pre-lock hide boundary.
        if (effective || !known || locked || m_waitForHide || hideOnFocusLoss) {
            return effective;
        }
        return std::nullopt;
    }

private:
    bool m_lastVisible = false;
    bool m_waitForHide = false;
};
