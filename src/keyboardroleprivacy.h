/*
    SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once
#include <optional>

enum class KeyboardRoleAction {
    KeepUnprivileged,
    KeepAuthorized,
    AuthorizeRestricted,
    Recreate
};

inline KeyboardRoleAction keyboardRoleAction(std::optional<bool> previousLocked, bool known, bool locked, bool authorized, bool attempted = false)
{
    const bool restricted = known && locked;
    if (previousLocked && *previousLocked != restricted) {
        return KeyboardRoleAction::Recreate;
    }
    if (!restricted) {
        return KeyboardRoleAction::KeepUnprivileged;
    }
    // A failed attempt must not be repeated on a potentially mapped surface.
    // Stay unprivileged until the normal hide/destroy path creates a new role.
    if (attempted && !authorized) {
        return KeyboardRoleAction::KeepUnprivileged;
    }
    return authorized ? KeyboardRoleAction::KeepAuthorized : KeyboardRoleAction::AuthorizeRestricted;
}
