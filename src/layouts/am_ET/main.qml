/*
    SPDX-FileCopyrightText: 2026 Abenezer Wesenseged <wseged@proton.me>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Components
import QtQuick.Layouts
import org.kde.plasma.keyboard.custom.lib as PlasmaKeyboard

KeyboardLayout {

    id: keyboardLayout
    keyWeight: 160

    function createInputMethod() {
        return Qt.createComponent("AmharicInputMethod.qml").createObject(parent);
    }

    readonly property real normalKeyWidth: normalKey.width
    readonly property real functionKeyWidth: mapFromItem(normalKey, normalKey.width / 2, 0).x

    
    KeyboardRow {
        Key {
            key: Qt.Key_Escape
            displayText: "Esc"
            noModifier: true
            functionKey: true
            highlighted: true
        }
        Key {
            text: "`"
            alternativeKeys: "`~"
            smallText: "~"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_1
            text: "1"
            alternativeKeys: "1!"
            smallText: "!"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_2
            text: "2"
            alternativeKeys: "2@"
            smallText: "@"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_3
            text: "3"
            alternativeKeys: "3#"
            smallText: "#"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_4
            text: "4"
            alternativeKeys: "4$"
            smallText: "$"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_5
            text: "5"
            alternativeKeys: "5%"
            smallText: "%"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_6
            text: "6"
            alternativeKeys: "6^"
            smallText: "^"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_7
            text: "7"
            alternativeKeys: "7&"
            smallText: "&"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_8
            text: "8"
            alternativeKeys: "8*"
            smallText: "*"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_9
            text: "9"
            alternativeKeys: "9("
            smallText: "("
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_0
            text: "0"
            alternativeKeys: "0)"
            smallText: ")"
            smallTextVisible: true
        }
        Key {
            text: "-"
            alternativeKeys: "-_"
            smallText: "_"
            smallTextVisible: true
        }
        Key {
            text: "+"
            alternativeKeys: "+="
            smallText: "="
            smallTextVisible: true
        }
        BackspaceKey {
        }
    }
    KeyboardRow {
        Key {
            key: Qt.Key_Tab
            displayText: "Tab"
            noModifier: true
            functionKey: true
            highlighted: true
        }
        Key {
            text: "\u1200"
        }
        Key {
            id: normalKey
            text: "\u1208"
        }
        Key {
            text: "\u1210"
        }
        Key {
            text: "\u1218"
        }
        Key {
            text: "\u1220"
        }
        Key {
            text: "\u1228"
        }
        Key {
            text: "\u1230"
        }
        Key {
            text: "\u1238"
        }
        Key {
            text: "\u1240"
            alternativeKeys: "\u1248\u124A\u124B\u124C\u124D"
            smallText: "\u1248"
            smallTextVisible: true
        }
        Key {
            text: "\u1260"
        }
        Key {
            text: "\u1268"
        }
        Key {
            text: "\u1270"
        }
        Key {
            text: "\\"
            alternativeKeys: "\\|/"
            smallText: "|/"
            smallTextVisible: true
        }
    }
    KeyboardRow {
        Key {
            key: Qt.Key_Delete
            displayText: "Del"
            noModifier: true
            functionKey: true
            highlighted: true
            Layout.preferredWidth: normalKeyWidth
            Layout.fillWidth: false
        }
        Key {
            text: "\u1278"
            weight: normalKeyWidth
            Layout.fillWidth: false
        }
        Key {
            text: "\u1280"
            alternativeKeys: "\u1288\u128A\u128B\u128C\u128D"
            smallText: "\u1288"
            smallTextVisible: true
        }
        Key {
            text: "\u1290"
        }
        Key {
            text: "\u1298"
        }
        Key {
            text: "\u12A0"
        }
        Key {
            text: "\u12A8"
            alternativeKeys: "\u12B0\u12B2\u12B3\u12B4\u12B5"
            smallText: "\u12B0"
            smallTextVisible: true
        }
        Key {
            text: "\u12B8"
            alternativeKeys: "\u12C0\u12C2\u12C3\u12C4\u12C5"
            smallText: "\u12C0"
            smallTextVisible: true
        }
        Key {
            text: "\u12C8"
        }
        Key {
            text: "\u12D0"
        }
        Key {
            text: "\u12D8"
        }
        Key {
            text: "\u12E0"
            weight: normalKeyWidth
            Layout.fillWidth: false
        }
        EnterKey {
            weight: normalKeyWidth * 2
            Layout.fillWidth: false
        }
    }
    KeyboardRow {
        ShiftKey {
            weight: normalKeyWidth
            Layout.fillWidth: false
        }
        Key {
            text: "\u12E8"
        }
        Key {
            text: "\u12F0"
        }
        Key {
            text: "\u1300"
        }
        Key {
            text: "\u1308"
            alternativeKeys: "\u1310\u1312\u1313\u1314\u1315"
            smallText: "\u1310"
            smallTextVisible: true
        }
        Key {
            text: "\u1320"
        }
        Key {
            text: "\u1328"
        }
        Key {
            text: "\u1330"
        }
        Key {
            text: "\u1338"
        }
        Key {
            text: "\u1340"
        }
        Key {
            text: "\u1348"
        }
        Key {
            text: "\u1350"
        }
        Key {
            weight: normalKeyWidth
            Layout.fillWidth: false
            text: "\u1363"
            smallText: "\u2699"
            smallTextVisible: keyboard.isFunctionPopupListAvailable()
            highlighted: true
        }
        Key {
            weight: normalKeyWidth
            Layout.fillWidth: false
            text: "\u1362"
            alternativeKeys: "!.?"
            smallText: "!?"
            smallTextVisible: true
            highlighted: true
        }
        FillerKey {
            weight: normalKeyWidth
            Layout.fillWidth: false
        }
        Key {
            key: Qt.Key_Up
            weight: normalKeyWidth
            Layout.fillWidth: false
            displayText: "\u2191"
            repeat: true
            noModifier: true
            functionKey: true
            highlighted: true
        }
        PlasmaKeyboard.HideKey {
            weight: normalKeyWidth
            Layout.fillWidth: false
        }
    }
    KeyboardRow {
        PlasmaKeyboard.ModifierKey {
            modifier: "ctrl"
            displayText: "Ctrl"
            weight: normalKeyWidth
            Layout.fillWidth: false
        }
        SymbolModeKey {
            weight: normalKeyWidth
            Layout.fillWidth: false
        }
        PlasmaKeyboard.ModifierKey {
            modifier: "meta"
            displayText: "Win"
            weight: normalKeyWidth
            Layout.fillWidth: false
        }
        PlasmaKeyboard.LanguageKey {
            weight: normalKeyWidth
            Layout.fillWidth: false
        }
        PlasmaKeyboard.FloatingKey {
            weight: normalKeyWidth
            Layout.fillWidth: false
        }
        SpaceKey {
        }
        PlasmaKeyboard.MicrophoneKey {
            weight: normalKeyWidth
            Layout.fillWidth: false
        }
        PlasmaKeyboard.ModifierKey {
            modifier: "alt"
            displayText: "Alt"
            weight: normalKeyWidth
            Layout.fillWidth: false
        }
        Key {
            key: Qt.Key_Left
            weight: normalKeyWidth
            Layout.fillWidth: false
            displayText: "\u2190"
            repeat: true
            noModifier: true
            functionKey: true
            highlighted: true
        }
        Key {
            key: Qt.Key_Down
            weight: normalKeyWidth
            Layout.fillWidth: false
            displayText: "\u2193"
            repeat: true
            noModifier: true
            functionKey: true
            highlighted: true
        }
        Key {
            key: Qt.Key_Right
            weight: normalKeyWidth
            Layout.fillWidth: false
            displayText: "\u2192"
            repeat: true
            noModifier: true
            functionKey: true
            highlighted: true
        }
    }

}
