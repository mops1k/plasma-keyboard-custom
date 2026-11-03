/*
    SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once

#include "inputplugin.h"

namespace InputContentPolicy
{
inline bool sensitive(InputPlugin::ContentHint hint, InputPlugin::ContentPurpose purpose)
{
    return (hint & (InputPlugin::content_hint_hidden_text | InputPlugin::content_hint_sensitive_data)) || purpose == InputPlugin::content_purpose_password;
}

// New contexts are untrusted until the compositor supplies their content type.
// This check must precede reading/extracting text, not just showing candidates.
inline bool predictionsAllowed(bool hasContext, bool contentTypeKnown, InputPlugin::ContentHint hint, InputPlugin::ContentPurpose purpose)
{
    return hasContext && contentTypeKnown && !sensitive(hint, purpose);
}

inline Qt::InputMethodHints privacyHints(InputPlugin::ContentHint hint, InputPlugin::ContentPurpose purpose)
{
    Qt::InputMethodHints result;
    if (sensitive(hint, purpose)) {
        result |= Qt::ImhSensitiveData | Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase;
    }
    if ((hint & InputPlugin::content_hint_hidden_text) || purpose == InputPlugin::content_purpose_password) {
        result |= Qt::ImhHiddenText;
    }
    return result;
}
}
