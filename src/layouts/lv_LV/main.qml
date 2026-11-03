// Modified Latvian layout: PC-style rows with modifiers, like fallback/ru_RU.
import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Components
import QtQuick.Layouts
import org.kde.plasma.keyboard.custom.lib as PlasmaKeyboard

KeyboardLayout {
    inputMode: InputEngine.InputMode.Latin
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
            key: Qt.Key_QuoteLeft
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
            key: Qt.Key_Minus
            text: "-"
            alternativeKeys: "-_"
            smallText: "_"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_Equal
            text: "="
            alternativeKeys: "=+"
            smallText: "+"
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
            key: Qt.Key_Q
            text: "q"
        }
        Key {
            key: Qt.Key_W
            text: "w"
        }
        Key {
            key: Qt.Key_E
            text: "e"
            alternativeKeys: "eē"
            smallText: "ē"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_R
            text: "r"
            alternativeKeys: "rŗ"
            smallText: "ŗ"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_T
            text: "t"
        }
        Key {
            key: Qt.Key_Y
            text: "y"
        }
        Key {
            key: Qt.Key_U
            text: "u"
            alternativeKeys: "uū"
            smallText: "ū"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_I
            text: "i"
            alternativeKeys: "iī"
            smallText: "ī"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_O
            text: "o"
            alternativeKeys: "oō"
            smallText: "ō"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_P
            text: "p"
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
            key: Qt.Key_A
            text: "a"
            alternativeKeys: "aā"
            smallText: "ā"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_S
            text: "s"
            alternativeKeys: "sš"
            smallText: "š"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_D
            text: "d"
        }
        Key {
            key: Qt.Key_F
            text: "f"
        }
        Key {
            key: Qt.Key_G
            text: "g"
            alternativeKeys: "gģ"
            smallText: "ģ"
            smallTextVisible: true
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
            alternativeKeys: "kķ"
            smallText: "ķ"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_L
            text: "l"
            alternativeKeys: "lļ"
            smallText: "ļ"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_Semicolon
            text: ";"
            alternativeKeys: ":"
            smallText: ":"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_Apostrophe
            text: "'"
            alternativeKeys: "'\""
            smallText: "\""
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
            key: Qt.Key_Z
            text: "z"
            alternativeKeys: "zž"
            smallText: "ž"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_X
            text: "x"
        }
        Key {
            key: Qt.Key_C
            text: "c"
            alternativeKeys: "cč"
            smallText: "č"
            smallTextVisible: true
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
            alternativeKeys: "nņ"
            smallText: "ņ"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_M
            text: "m"
        }
        Key {
            key: Qt.Key_Comma
            text: ","
            alternativeKeys: "<"
            smallText: "<"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_Period
            text: "."
            alternativeKeys: ">"
            smallText: ">"
            smallTextVisible: true
        }
        Key {
            key: Qt.Key_Slash
            text: "/"
            alternativeKeys: "?"
            smallText: "?"
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
