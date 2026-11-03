// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Components
import QtQuick.Layouts
import org.kde.plasma.keyboard.custom.lib as PlasmaKeyboard

KeyboardLayout {

    inputMode: InputEngine.InputMode.Latin
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
            text: "\u0914"
            alternativeKeys: "\u0914\u094C\u0967"
        }
        Key {
            id: normalKey
            text: "\u0910"
            alternativeKeys: "\u0910\u0948\u0968"
        }
        Key {
            text: "\u0906"
            alternativeKeys: "\u0906\u093E\u0969"
        }
        Key {
            text: "\u0908"
            alternativeKeys: "\u0908\u0940\u096A"
        }
        Key {
            text: "\u090A"
            alternativeKeys: "\u090A\u0942\u096B"
        }
        Key {
            text: "\u092C"
            alternativeKeys: "\u092C\u092D\u096C"
        }
        Key {
            text: "\u0939"
            alternativeKeys: "\u0939\u096D"
        }
        Key {
            text: "\u0917"
            alternativeKeys: "\u0917\u0918\u096E"
        }
        Key {
            text: "\u0926"
            alternativeKeys: "\u0926\u0927\u096F"
        }
        Key {
            text: "\u091C"
            alternativeKeys: "\u0966\u091E\u091D\u091C"
        }
        Key {
            text: "\u0921"
            alternativeKeys: "\u0921\u0922"
        }
        Key {
            key: Qt.Key_BracketLeft
            text: "["
            alternativeKeys: "[{"
            smallText: "{"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_BracketRight
            text: "]"
            alternativeKeys: "]}"
            smallText: "}"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_Backslash
            text: "\\"
            alternativeKeys: "\\|"
            smallText: "|"
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
            text: "\u0913"
            alternativeKeys: "\u0913\u094B"
        }
        Key {
            text: "\u090F"
            alternativeKeys: "\u090F\u0947"
        }
        Key {
            text: "\u0905"
        }
        Key {
            text: "\u0907"
            alternativeKeys: "\u0907\u093F"
        }
        Key {
            text: "\u0909"
            alternativeKeys: "\u0909\u0941"
        }
        Key {
            text: "\u092A"
            alternativeKeys: "\u092A\u092B"
        }
        Key {
            text: "\u0930"
            alternativeKeys: "\u0930\u090B\u0943"
        }
        Key {
            text: "\u0915"
            alternativeKeys: "\u0915\u0916"
        }
        Key {
            text: "\u0924"
            alternativeKeys: ["\u0924", "\u0925", "\u0924\u094D\u0930"]
        }
        Key {
            text: "\u091A"
            alternativeKeys: "\u091A\u091B"
        }
        Key {
            text: "\u091F"
            alternativeKeys: "\u091F\u0920"
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
            text: "\u0911"
            alternativeKeys: "\u0911\u0949"
        }
        Key {
            text: "\u094D"
            alternativeKeys: "\u094D\u0945\u090D"
        }
        Key {
            text: "\u0902"
            alternativeKeys: "\u093C\u0902\u0903\u0901\u093D"
        }
        Key {
            text: "\u092E"
            alternativeKeys: "\u092E\u0950"
        }
        Key {
            text: "\u0928"
            alternativeKeys: "\u0919\u0928\u0923\u091E"
        }
        Key {
            text: "\u0935"
        }
        Key {
            text: "\u0932"
            alternativeKeys: "\u0932\u0933"
        }
        Key {
            text: "\u0938"
            alternativeKeys: ["\u0936\u094D\u0930", "\u0938", "\u0936", "\u0937"]
        }
        Key {
            text: "\u092F"
            alternativeKeys: "\u0943"
        }
        Key {
            key: Qt.Key_Comma
            weight: normalKeyWidth
            Layout.fillWidth: false
            text: ","
            smallText: "\u2699"
            smallTextVisible: keyboard.isFunctionPopupListAvailable()
            highlighted: true
        }
        Key {
            key: Qt.Key_Period
            weight: normalKeyWidth
            Layout.fillWidth: false
            text: "."
            alternativeKeys: "!.?"
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
