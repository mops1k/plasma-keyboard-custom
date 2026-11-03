// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Components
import QtQuick.Layouts
import org.kde.plasma.keyboard.custom.lib as PlasmaKeyboard

KeyboardLayoutLoader {
    sharedLayouts: ['symbols']
    sourceComponent: InputContext.inputEngine.inputMode === InputEngine.InputMode.Greek ? greekLayout : latinLayout
    Component {
        id: greekLayout
        KeyboardLayout {

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
                    text: "ς"
                }
                Key {
                    id: normalKey
                    text: "ε"
                    alternativeKeys: "εέ"
                }
                Key {
                    text: "ρ"
                }
                Key {
                    text: "τ"
                }
                Key {
                    text: "ψ"
                }
                Key {
                    text: "υ"
                    alternativeKeys: "υύϋΰ"
                }
                Key {
                    text: "θ"
                }
                Key {
                    text: "ι"
                    alternativeKeys: "ιίϊΐ"
                }
                Key {
                    text: "ο"
                    alternativeKeys: "οό"
                }
                Key {
                    text: "π"
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
                    text: "α"
                    alternativeKeys: "αά"
                    weight: normalKeyWidth
                    Layout.fillWidth: false
                }
                Key {
                    text: "σ"
                }
                Key {
                    text: "δ"
                }
                Key {
                    text: "φ"
                }
                Key {
                    text: "γ"
                }
                Key {
                    text: "η"
                    alternativeKeys: "ηή"
                }
                Key {
                    text: "ξ"
                }
                Key {
                    text: "κ"
                }
                Key {
                    text: "λ"
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
                    text: "ζ"
                }
                Key {
                    text: "χ"
                }
                Key {
                    text: "ψ"
                }
                Key {
                    text: "ω"
                    alternativeKeys: "ωώ"
                }
                Key {
                    text: "β"
                }
                Key {
                    text: "ν"
                }
                Key {
                    text: "μ"
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
    }
    Component {
        id: latinLayout
        KeyboardLayout {

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
                    key: Qt.Key_Q
                    text: "q"
                }
                Key {
                    id: normalKey
                    key: Qt.Key_W
                    text: "w"
                }
                Key {
                    key: Qt.Key_E
                    text: "e"
                    alternativeKeys: "êeëèé"
                }
                Key {
                    key: Qt.Key_R
                    text: "r"
                    alternativeKeys: "ŕrř"
                }
                Key {
                    key: Qt.Key_T
                    text: "t"
                    alternativeKeys: "ţtŧť"
                }
                Key {
                    key: Qt.Key_Y
                    text: "y"
                    alternativeKeys: "ÿyýŷ"
                }
                Key {
                    key: Qt.Key_U
                    text: "u"
                    alternativeKeys: "űūũûüuùú"
                }
                Key {
                    key: Qt.Key_I
                    text: "i"
                    alternativeKeys: "îïīĩiìí"
                }
                Key {
                    key: Qt.Key_O
                    text: "o"
                    alternativeKeys: "œøõôöòóo"
                }
                Key {
                    key: Qt.Key_P
                    text: "p"
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
                    key: Qt.Key_A
                    text: "a"
                    alternativeKeys: (InputContext.inputMethodHints & (Qt.ImhEmailCharactersOnly | Qt.ImhUrlCharactersOnly)) ? "a@äåãâàá" : "aäåãâàá"
                    smallTextVisible: (InputContext.inputMethodHints & (Qt.ImhEmailCharactersOnly | Qt.ImhUrlCharactersOnly))
                    weight: normalKeyWidth
                    Layout.fillWidth: false
                }
                Key {
                    key: Qt.Key_S
                    text: "s"
                    alternativeKeys: "šsşś"
                }
                Key {
                    key: Qt.Key_D
                    text: "d"
                    alternativeKeys: "dđď"
                }
                Key {
                    key: Qt.Key_F
                    text: "f"
                }
                Key {
                    key: Qt.Key_G
                    text: "g"
                    alternativeKeys: "ġgģĝğ"
                }
                Key {
                    key: Qt.Key_H
                    text: "h"
                }
                Key {
                    key: Qt.Key_J
                    text: "j"
                }
                Key {
                    key: Qt.Key_K
                    text: "k"
                }
                Key {
                    key: Qt.Key_L
                    text: "l"
                    alternativeKeys: "ĺŀłļľl"
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
                    key: Qt.Key_Z
                    text: "z"
                    alternativeKeys: "zžż"
                }
                Key {
                    key: Qt.Key_X
                    text: "x"
                }
                Key {
                    key: Qt.Key_C
                    text: "c"
                    alternativeKeys: "çcċčć"
                }
                Key {
                    key: Qt.Key_V
                    text: "v"
                }
                Key {
                    key: Qt.Key_B
                    text: "b"
                }
                Key {
                    key: Qt.Key_N
                    text: "n"
                    alternativeKeys: "ņńnň"
                }
                Key {
                    key: Qt.Key_M
                    text: "m"
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
                SpaceKey {
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
    }
}
