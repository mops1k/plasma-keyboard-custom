// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Components
import QtQuick.Layouts
import org.kde.plasma.keyboard.custom.lib as PlasmaKeyboard

KeyboardLayout {
    sharedLayouts: ['symbols']
    keyWeight: 160
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
            key: 0x0419
            text: "й"
        }
        Key {
            id: normalKey
            key: 0x0426
            text: "ц"
        }
        Key {
            key: 0x0423
            text: "у"
        }
        Key {
            key: 0x041A
            text: "к"
        }
        Key {
            key: 0x0415
            text: "е"
            alternativeKeys: "её"
            smallText: "ё"
            smallTextVisible: true
        }
        Key {
            key: 0x041D
            text: "н"
        }
        Key {
            key: 0x0413
            text: "г"
        }
        Key {
            key: 0x0428
            text: "ш"
        }
        Key {
            key: 0x0429
            text: "щ"
        }
        Key {
            key: 0x0417
            text: "з"
        }
        Key {
            key: 0x0425
            text: "х"
            alternativeKeys: "х[{"
            smallText: "[{"
            smallTextVisible: true
        }
        Key {
            key: 0x042A
            text: "ъ"
            alternativeKeys: "ъ]}"
            smallText: "]}"
            smallTextVisible: true
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
            key: 0x0424
            text: "ф"
        }
        Key {
            key: 0x042B
            text: "ы"
        }
        Key {
            key: 0x0412
            text: "в"
        }
        Key {
            key: 0x0410
            text: "а"
        }
        Key {
            key: 0x041F
            text: "п"
        }
        Key {
            key: 0x0420
            text: "р"
        }
        Key {
            key: 0x041E
            text: "о"
        }
        Key {
            key: 0x041B
            text: "л"
        }
        Key {
            key: 0x0414
            text: "д"
        }
        Key {
            key: 0x0416
            text: "ж"
            alternativeKeys: "ж;:"
            smallText: ";:"
            smallTextVisible: true
        }
        Key {
            key: 0x042D
            text: "э"
            alternativeKeys: "э'\""
            smallText: "'\""
            smallTextVisible: true
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
            key: 0x042F
            text: "я"
        }
        Key {
            key: 0x0427
            text: "ч"
        }
        Key {
            key: 0x0421
            text: "с"
        }
        Key {
            key: 0x041C
            text: "м"
        }
        Key {
            key: 0x0418
            text: "и"
        }
        Key {
            key: 0x0422
            text: "т"
        }
        Key {
            key: 0x042C
            text: "ь"
            alternativeKeys: "ь"
            smallTextVisible: true
        }
        Key {
            key: 0x0411
            text: "б"
            alternativeKeys: "б,<"
            smallText: ",<"
            smallTextVisible: true
        }
        Key {
            key: 0x042E
            text: "ю"
            alternativeKeys: "ю.>"
            smallText: ".>"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_Period
            text: "/"
            alternativeKeys: "!?.,"
            smallText: "!?.,"
            smallTextVisible: true
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
