// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Components
import QtQuick.Layouts
import org.kde.plasma.keyboard.custom.lib as PlasmaKeyboard

KeyboardLayout {
    id: pcLayout
    // Caps Lock affects letters, not the punctuation level of PC keys.
    readonly property bool symbolsShifted: InputContext.shiftActive && !InputContext.capsLockActive

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
            text: pcLayout.symbolsShifted ? "~" : "`"
            alternativeKeys: "`~"
            smallText: pcLayout.symbolsShifted ? "`" : "~"
            smallTextVisible: true
        }
        Key {
            text: pcLayout.symbolsShifted ? "!" : "1"
            alternativeKeys: "1!"
            smallText: pcLayout.symbolsShifted ? "1" : "!"
            smallTextVisible: true
        }
        Key {
            text: pcLayout.symbolsShifted ? "\"" : "2"
            alternativeKeys: "2\"@"
            smallText: pcLayout.symbolsShifted ? "2" : "\""
            smallTextVisible: true
        }
        Key {
            text: pcLayout.symbolsShifted ? "№" : "3"
            alternativeKeys: "3№#"
            smallText: pcLayout.symbolsShifted ? "3" : "№"
            smallTextVisible: true
        }
        Key {
            text: pcLayout.symbolsShifted ? ";" : "4"
            alternativeKeys: "4;$"
            smallText: pcLayout.symbolsShifted ? "4" : ";"
            smallTextVisible: true
        }
        Key {
            text: pcLayout.symbolsShifted ? "%" : "5"
            alternativeKeys: "5%"
            smallText: pcLayout.symbolsShifted ? "5" : "%"
            smallTextVisible: true
        }
        Key {
            text: pcLayout.symbolsShifted ? ":" : "6"
            alternativeKeys: "6:^"
            smallText: pcLayout.symbolsShifted ? "6" : ":"
            smallTextVisible: true
        }
        Key {
            text: pcLayout.symbolsShifted ? "?" : "7"
            alternativeKeys: "7?&"
            smallText: pcLayout.symbolsShifted ? "7" : "?"
            smallTextVisible: true
        }
        Key {
            text: pcLayout.symbolsShifted ? "*" : "8"
            alternativeKeys: "8*"
            smallText: pcLayout.symbolsShifted ? "8" : "*"
            smallTextVisible: true
        }
        Key {
            text: pcLayout.symbolsShifted ? "(" : "9"
            alternativeKeys: "9("
            smallText: pcLayout.symbolsShifted ? "9" : "("
            smallTextVisible: true
        }
        Key {
            text: pcLayout.symbolsShifted ? ")" : "0"
            alternativeKeys: "0)"
            smallText: pcLayout.symbolsShifted ? "0" : ")"
            smallTextVisible: true
        }
        Key {
            text: pcLayout.symbolsShifted ? "_" : "-"
            alternativeKeys: "-_"
            smallText: pcLayout.symbolsShifted ? "-" : "_"
            smallTextVisible: true
        }
        Key {
            text: pcLayout.symbolsShifted ? "+" : "="
            alternativeKeys: "=+"
            smallText: pcLayout.symbolsShifted ? "=" : "+"
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
            id: normalKey
            text: "й"
        }
        Key {
            text: "ц"
        }
        Key {
            text: "у"
        }
        Key {
            text: "к"
        }
        Key {
            text: "е"
        }
        Key {
            text: "н"
        }
        Key {
            text: "г"
        }
        Key {
            text: "ш"
        }
        Key {
            text: "щ"
        }
        Key {
            text: "з"
        }
        Key {
            text: "х"
        }
        Key {
            text: "ї"
        }
        Key {
            text: "ґ"
            alternativeKeys: "\\|/"
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
            text: "ф"
        }
        Key {
            text: "і"
        }
        Key {
            text: "в"
        }
        Key {
            text: "а"
        }
        Key {
            text: "п"
        }
        Key {
            text: "р"
        }
        Key {
            text: "о"
        }
        Key {
            text: "л"
        }
        Key {
            text: "д"
        }
        Key {
            text: "ж"
        }
        Key {
            text: "є"
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
            text: "я"
            alternativeKeys: "$¢я₴€¥£"
            smallTextVisible: true
        }
        Key {
            text: "ч"
        }
        Key {
            text: "с"
        }
        Key {
            text: "м"
        }
        Key {
            text: "и"
        }
        Key {
            text: "т"
        }
        Key {
            text: "ь"
        }
        Key {
            text: "б"
        }
        Key {
            text: "ю"
        }
        Key {
            text: pcLayout.symbolsShifted ? "," : "."
            alternativeKeys: ".,!?"
            smallText: pcLayout.symbolsShifted ? "." : ","
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
