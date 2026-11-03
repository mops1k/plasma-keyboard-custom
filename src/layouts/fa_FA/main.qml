// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Components
import QtQuick.Layouts
import org.kde.plasma.keyboard.custom.lib as PlasmaKeyboard

KeyboardLayout {

    inputMode: InputEngine.InputMode.Arabic
    keyWeight: 160
    smallTextVisible: true
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
            text: "\u0635"
            alternativeKeys: "1\u0635\u0636\u0661"
            smallText: "\u0636\u0661"
        }
        Key {
            id: normalKey
            text: "\u0642"
            alternativeKeys: "2\u0642\u0662"
        }
        Key {
            text: "\u0641"
            alternativeKeys: "3\u0641\u0663"
        }
        Key {
            text: "\u063A"
            alternativeKeys: "4\u063A\u0664"
        }
        Key {
            text: "\u0639"
            alternativeKeys: "5\u0639\u0665"
        }
        Key {
            text: "\u0647"
            alternativeKeys: "6\u0647\u0666"
        }
        Key {
            text: "\u062E"
            alternativeKeys: "7\u062E\u0667"
        }
        Key {
            text: "\u062D"
            alternativeKeys: "8\u062D\u0668"
        }
        Key {
            text: "\u062C"
            alternativeKeys: "9\u062C\u0669"
        }
        Key {
            text: "\u0686"
            alternativeKeys: "0\u0686\u0660"
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
            text: "\u0634"
        }
        Key {
            text: "\u0633"
        }
        Key {
            text: "\u06CC"
        }
        Key {
            text: "\u0628"
        }
        Key {
            text: "\u0644"
        }
        Key {
            text: "\u0627"
            alternativeKeys: "\u0625\u0627\u0623\u0622"
        }
        Key {
            text: "\u062A"
            alternativeKeys: "\u062A\u062B"
        }
        Key {
            text: "\u0646"
        }
        Key {
            text: "\u0645"
        }
        Key {
            text: "\u06A9"
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
            text: "\u200C"
            alternativeKeys: "\u200C\u200D"
            displayText: "\u25C2\u205E\u25B8"
            displayAlternativeKeys: ["\u25B8\u205E\u25C2"]
            highlighted: true
        }
        Key {
            text: "\u064E"
            alternativeKeys: "\u064C\u064E\u064F\u0652\u064B\u064D\u0640\u0651\u0650"
            smallText: "\u25CC\u064F "
        }
        Key {
            text: "\u0637"
            alternativeKeys: "\u0637\u0638"
        }
        Key {
            text: "\u0632"
            alternativeKeys: "\u0632\u0698"
        }
        Key {
            text: "\u0631"
        }
        Key {
            text: "\u0630"
        }
        Key {
            text: "\u062F"
        }
        Key {
            text: "\u0648"
            alternativeKeys: "\u0624\u0648\u0621"
        }
        Key {
            text: "\u06AF"
            alternativeKeys: "\u06AF\u067E"
        }
        Key {
            key: Qt.Key_Comma
            weight: normalKeyWidth
            Layout.fillWidth: false
            text: "\u060C"
            smallText: "\u2699"
            smallTextVisible: keyboard.isFunctionPopupListAvailable()
            highlighted: true
        }
        Key {
            key: Qt.Key_Period
            weight: normalKeyWidth
            Layout.fillWidth: false
            text: "."
            alternativeKeys: "!,\u060C\u061B.\u061F"
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
