// SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
import QtQuick
import QtQuick.Window

// Observe the actual password control, not an ancestor whose child may consume
// the event before an ancestor handler sees it.
// Do not derive native-window activation from global keyboard-visible state.
PointHandler {
    required property Item screenRoot
    required property Item targetItem
    onTargetItemChanged: if (targetItem) parent = targetItem
    Component.onCompleted: if (targetItem) parent = targetItem
    required property Window viewWindow
    objectName: "localTouchFocus"
    signal activationRequested()
    acceptedDevices: PointerDevice.TouchScreen
    onActiveChanged: {
        if (active && viewWindow) {
            screenRoot.uiVisible = true;
            activationRequested();
            viewWindow.requestActivate();
        }
    }
}
