/*
    SPDX-FileCopyrightText: 2025 Devin Lin <devin@kde.org>
    SPDX-FileCopyrightText: 2026 Kristen McWilliam <kristen@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import QtQuick.Dialogs

import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.AbstractKCM {
    id: root

    readonly property var currentTheme: kcm.availableThemes[themeComboBox.currentIndex] || ({})

    header: QQC2.TabBar {
        id: tabBar

        QQC2.TabButton {
            text: i18n("Layouts")
        }

        QQC2.TabButton {
            text: i18nc("@title:tab", "Opening")
        }

        QQC2.TabButton {
            text: i18n("Appearance")
        }

        QQC2.TabButton {
            text: i18n("Typing")
        }

        QQC2.TabButton {
            text: i18n("Voice input")
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: tabBar.currentIndex

        LocaleSelectorListView {
            id: list
        }

        SettingsFormPage {
            SettingsRow {
                label: i18n("Open on long press:")
                description: i18n("Hold a finger on a text field")

                QQC2.Switch {
                    id: showOnLongTapSwitch
                    checked: kcm.showOnLongTap
                    onCheckedChanged: {
                        kcm.showOnLongTap = checked;
                        checked = Qt.binding(() => kcm.showOnLongTap);
                    }
                }
            }

            SettingsRow {
                label: i18n("Long press delay:")
                description: i18n("Time to hold a finger on the screen")
                controlFillWidth: true

                QQC2.SpinBox {
                    id: showOnLongTapThreshold
                    Layout.fillWidth: true
                    from: 100
                    to: 5000
                    stepSize: 100
                    editable: true
                    enabled: showOnLongTapSwitch.checked

                    value: kcm.showOnLongTapThresholdMs
                    onValueModified: {
                        kcm.showOnLongTapThresholdMs = value;
                        value = Qt.binding(() => kcm.showOnLongTapThresholdMs);
                    }

                    textFromValue: function (value) {
                        return i18nc("duration in milliseconds", "%1 ms", value);
                    }
                    valueFromText: function (text) {
                        const number = parseInt(text);
                        return isNaN(number) ? kcm.showOnLongTapThresholdMs : number;
                    }
                }
            }

            SettingsRow {
                label: i18n("Open on focus:")
                description: i18n("When a text field is focused with a mouse")

                QQC2.Switch {
                    id: showOnMouseFocusSwitch
                    checked: kcm.showOnMouseFocus
                    onCheckedChanged: {
                        kcm.showOnMouseFocus = checked;
                        checked = Qt.binding(() => kcm.showOnMouseFocus);
                    }
                }
            }

            SettingsRow {
                label: i18n("Hide the panel while the keyboard is open")
                description: i18n("The keyboard reaches the bottom of the screen")

                QQC2.Switch {
                    id: hidePanelWhenKeyboardVisibleSwitch
                    checked: kcm.hidePanelWhenKeyboardVisible
                    onCheckedChanged: {
                        kcm.hidePanelWhenKeyboardVisible = checked;
                        checked = Qt.binding(() => kcm.hidePanelWhenKeyboardVisible);
                    }
                }
            }

            SettingsRow {
                label: i18n("Hide when the text field loses focus")
                description: i18n("When off, the keyboard is only hidden with its hide key")

                QQC2.Switch {
                    checked: kcm.hideOnInputFocusLoss
                    onCheckedChanged: {
                        kcm.hideOnInputFocusLoss = checked;
                        checked = Qt.binding(() => kcm.hideOnInputFocusLoss);
                    }
                }
            }
        }

        SettingsFormPage {
            SettingsRow {
                label: i18n("Keyboard height:")
                description: i18n("Percentage of the screen height")
                controlFillWidth: true

                trailing: QQC2.Label {
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 3
                    horizontalAlignment: Text.AlignRight
                    text: i18nc("keyboard height in percent", "%1%", Math.round(keyboardHeightSlider.value))
                }

                QQC2.Slider {
                    id: keyboardHeightSlider
                    Layout.fillWidth: true
                    from: 20
                    to: 80
                    stepSize: 1
                    snapMode: QQC2.Slider.SnapAlways
                    value: kcm.keyboardHeightPercent

                    onMoved: {
                        kcm.keyboardHeightPercent = value;
                        value = Qt.binding(() => kcm.keyboardHeightPercent);
                    }

                    Accessible.name: i18n("Keyboard height")
                }
            }

            SettingsRow {
                label: i18n("Floating keyboard width:")
                description: i18n("Percentage of the screen width")
                controlFillWidth: true

                trailing: QQC2.Label {
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 3
                    horizontalAlignment: Text.AlignRight
                    text: i18nc("keyboard width in percent", "%1%", Math.round(floatingKeyboardWidthSlider.value))
                }

                QQC2.Slider {
                    id: floatingKeyboardWidthSlider
                    Layout.fillWidth: true
                    from: 20
                    to: 100
                    stepSize: 5
                    snapMode: QQC2.Slider.SnapAlways
                    value: kcm.floatingKeyboardWidthPercent

                    onMoved: {
                        kcm.floatingKeyboardWidthPercent = value;
                        value = Qt.binding(() => kcm.floatingKeyboardWidthPercent);
                    }

                    Accessible.name: i18n("Floating keyboard width")
                }
            }

            SettingsRow {
                label: i18n("Floating keyboard opacity:")
                description: i18n("Percentage of opacity, 100% is fully opaque")
                controlFillWidth: true

                trailing: QQC2.Label {
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 3
                    horizontalAlignment: Text.AlignRight
                    text: i18nc("keyboard opacity in percent", "%1%", Math.round(floatingKeyboardOpacitySlider.value))
                }

                QQC2.Slider {
                    id: floatingKeyboardOpacitySlider
                    Layout.fillWidth: true
                    from: 20
                    to: 100
                    stepSize: 5
                    snapMode: QQC2.Slider.SnapAlways
                    value: kcm.floatingKeyboardOpacity

                    onMoved: {
                        kcm.floatingKeyboardOpacity = value;
                        value = Qt.binding(() => kcm.floatingKeyboardOpacity);
                    }

                    Accessible.name: i18n("Floating keyboard opacity")
                }
            }

            SettingsRow {
                label: i18n("Keyboard font:")
                description: i18n("Font used for the key labels")
                controlFillWidth: true

                QQC2.Button {
                    id: keyboardFontButton
                    Layout.fillWidth: true

                    text: kcm.keyboardFontFamily.length > 0 ? kcm.keyboardFontFamily : i18n("Default")
                    font.family: kcm.keyboardFontFamily.length > 0 ? kcm.keyboardFontFamily : Kirigami.Theme.defaultFont.family

                    onClicked: {
                        fontChooserDialog.selectedFamily = kcm.keyboardFontFamily;
                        fontChooserDialog.open();
                    }
                }

                QQC2.Button {
                    icon.name: "edit-undo"
                    onClicked: kcm.keyboardFontFamily = ""
                    Accessible.name: i18n("Reset to default")
                    QQC2.ToolTip.text: i18n("Reset to default")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                }
            }

            SettingsRow {
                label: i18n("Theme:")
                description: i18n("Colour scheme of the keyboard")
                controlFillWidth: true

                QQC2.ComboBox {
                    id: themeComboBox
                    Layout.fillWidth: true

                    model: kcm.availableThemes
                    textRole: "name"
                    valueRole: "id"
                    currentIndex: Math.max(0, model.findIndex(theme => theme.id === kcm.theme))

                    onActivated: (index) => {
                        kcm.theme = model[index].id;
                    }
                }
            }

            SettingsRow {
                label: i18n("Theme files:")
                description: i18n("Import, export or remove a theme file")

                QQC2.Button {
                    text: i18n("Import theme…")
                    onClicked: {
                        themeError.text = "";
                        importThemeDialog.open();
                    }
                }

                QQC2.Button {
                    text: i18n("Export theme…")
                    onClicked: {
                        themeError.text = "";
                        exportThemeDialog.themeId = themeComboBox.currentValue;
                        exportThemeDialog.open();
                    }
                }

                QQC2.Button {
                    text: i18n("Remove theme")
                    enabled: root.currentTheme.source === "user"
                    onClicked: {
                        themeError.text = "";
                        removeThemeDialog.open();
                    }
                }
            }

            Kirigami.InlineMessage {
                id: themeError
                Layout.fillWidth: true
                type: Kirigami.MessageType.Error
                visible: text.length > 0
            }

            SettingsRow {
                label: i18n("Function keys:")
                description: i18n("Show an F1–F12 row above the keyboard")

                QQC2.Switch {
                    id: showFunctionKeyRowSwitch
                    checked: kcm.showFunctionKeyRow
                    onCheckedChanged: {
                        kcm.showFunctionKeyRow = checked;
                        checked = Qt.binding(() => kcm.showFunctionKeyRow);
                    }
                }
            }

            SettingsRow {
                label: i18n("Clipboard:")
                description: i18n("Show recent clipboard entries above the keyboard")

                QQC2.Switch {
                    id: clipboardEnabledSwitch
                    checked: kcm.clipboardEnabled
                    onCheckedChanged: {
                        kcm.clipboardEnabled = checked;
                        checked = Qt.binding(() => kcm.clipboardEnabled);
                    }
                }
            }
        }

        SettingsFormPage {
            SettingsRow {
                label: i18n("Automatic capitalization:")
                description: i18n("Capitalize the first letter of a sentence")

                QQC2.Switch {
                    id: autoCapitalizationSwitch
                    checked: kcm.autoCapitalizationEnabled
                    onCheckedChanged: {
                        kcm.autoCapitalizationEnabled = checked;
                        checked = Qt.binding(() => kcm.autoCapitalizationEnabled);
                    }
                }
            }

            SettingsRow {
                label: i18n("Alternate characters:")
                description: i18n("Show a popup when holding a key")

                QQC2.Switch {
                    id: diacriticsSwitch
                    checked: kcm.diacriticsPopupEnabled
                    onCheckedChanged: {
                        kcm.diacriticsPopupEnabled = checked;
                        checked = Qt.binding(() => kcm.diacriticsPopupEnabled);
                    }
                }
            }

            SettingsRow {
                label: i18n("Hold delay:")
                description: i18n("Time to hold a key before the popup appears")
                controlFillWidth: true

                QQC2.SpinBox {
                    id: diacriticsDelaySpinBox
                    Layout.fillWidth: true
                    from: 100
                    to: 1500
                    stepSize: 50
                    enabled: diacriticsSwitch.checked
                    value: kcm.diacriticsHoldThresholdMs

                    textFromValue: function (value) {
                        return i18nc("duration in milliseconds", "%1 ms", value);
                    }
                    valueFromText: function (text) {
                        const number = parseInt(text);
                        return isNaN(number) ? kcm.diacriticsHoldThresholdMs : number;
                    }

                    onValueChanged: {
                        kcm.diacriticsHoldThresholdMs = value;
                        value = Qt.binding(() => kcm.diacriticsHoldThresholdMs);
                    }
                }
            }

            SettingsRow {
                label: i18n("Gamepad:")
                description: i18n("Hold the A button to pick an alternate character")

                QQC2.Switch {
                    id: gamepadAlternatesSwitch
                    checked: kcm.gamepadAlternatesEnabled
                    onCheckedChanged: {
                        kcm.gamepadAlternatesEnabled = checked;
                        checked = Qt.binding(() => kcm.gamepadAlternatesEnabled);
                    }
                }
            }

            SettingsRow {
                label: i18n("Gamepad hold delay:")
                description: i18n("Time to hold the A button before the popup appears")
                controlFillWidth: true

                QQC2.SpinBox {
                    id: gamepadAlternatesDelaySpinBox
                    Layout.fillWidth: true
                    from: 100
                    to: 1500
                    stepSize: 50
                    enabled: gamepadAlternatesSwitch.checked
                    value: kcm.gamepadAlternatesThresholdMs

                    textFromValue: function (value) {
                        return i18nc("duration in milliseconds", "%1 ms", value);
                    }
                    valueFromText: function (text) {
                        const number = parseInt(text);
                        return isNaN(number) ? kcm.gamepadAlternatesThresholdMs : number;
                    }

                    onValueChanged: {
                        kcm.gamepadAlternatesThresholdMs = value;
                        value = Qt.binding(() => kcm.gamepadAlternatesThresholdMs);
                    }
                }
            }

            SettingsRow {
                label: i18n("Word suggestions:")
                description: i18n("Offer words that continue what is being typed")

                QQC2.Switch {
                    id: predictiveTextSwitch
                    checked: kcm.predictiveTextEnabled
                    onCheckedChanged: {
                        kcm.predictiveTextEnabled = checked;
                        checked = Qt.binding(() => kcm.predictiveTextEnabled);
                    }
                }
            }

            SettingsRow {
                label: i18n("Suggestions at once:")
                description: i18n("How many word suggestions are offered at the same time")
                controlFillWidth: true

                QQC2.SpinBox {
                    id: predictiveSuggestionCountSpinBox
                    Layout.fillWidth: true
                    from: 1
                    to: 5
                    enabled: predictiveTextSwitch.checked
                    value: kcm.predictiveSuggestionCount

                    onValueChanged: {
                        kcm.predictiveSuggestionCount = value;
                        value = Qt.binding(() => kcm.predictiveSuggestionCount);
                    }
                }
            }

            SettingsRow {
                label: i18n("Letters before suggesting:")
                description: i18n("How many letters have to be typed before word suggestions appear")
                controlFillWidth: true

                QQC2.SpinBox {
                    id: predictiveMinPrefixLengthSpinBox
                    Layout.fillWidth: true
                    from: 1
                    to: 4
                    enabled: predictiveTextSwitch.checked
                    value: kcm.predictiveMinPrefixLength

                    textFromValue: function (value) {
                        return i18np("%1 letter", "%1 letters", value);
                    }
                    valueFromText: function (text) {
                        const number = parseInt(text);
                        return isNaN(number) ? kcm.predictiveMinPrefixLength : number;
                    }

                    onValueChanged: {
                        kcm.predictiveMinPrefixLength = value;
                        value = Qt.binding(() => kcm.predictiveMinPrefixLength);
                    }
                }
            }

            SettingsRow {
                label: i18n("Next word")
                description: i18n("Offer words that may follow the typed one")

                QQC2.Switch {
                    id: predictiveNextWordSwitch
                    enabled: predictiveTextSwitch.checked
                    checked: kcm.predictiveNextWordEnabled
                    onCheckedChanged: {
                        kcm.predictiveNextWordEnabled = checked;
                        checked = Qt.binding(() => kcm.predictiveNextWordEnabled);
                    }
                }
            }

            SettingsRow {
                label: i18n("Typo correction")
                description: i18n("Offer to correct a mistyped word")

                QQC2.Switch {
                    id: predictiveTypoCorrectionSwitch
                    enabled: predictiveTextSwitch.checked
                    checked: kcm.predictiveTypoCorrectionEnabled
                    onCheckedChanged: {
                        kcm.predictiveTypoCorrectionEnabled = checked;
                        checked = Qt.binding(() => kcm.predictiveTypoCorrectionEnabled);
                    }
                }
            }

            QQC2.Label {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.smallSpacing
                text: i18n("Key press feedback:")
                font.bold: true
            }

            SettingsRow {
                label: i18n("Sound")
                description: i18n("Whether to emit a sound on key press")

                QQC2.Switch {
                    id: soundsEnabledSwitch
                    checked: kcm.soundEnabled
                    onCheckedChanged: {
                        kcm.soundEnabled = checked;
                        checked = Qt.binding(() => kcm.soundEnabled);
                    }
                }
            }

            SettingsRow {
                label: i18n("Vibration")
                description: i18n("Whether to vibrate on key press")

                QQC2.Switch {
                    id: vibrationEnabledSwitch
                    checked: kcm.vibrationEnabled
                    onCheckedChanged: {
                        kcm.vibrationEnabled = checked;
                        checked = Qt.binding(() => kcm.vibrationEnabled);
                    }
                }
            }

            SettingsRow {
                label: i18n("Vibration strength:")
                description: i18n("Percentage of the maximum vibration")
                controlFillWidth: true

                trailing: QQC2.Label {
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 3
                    horizontalAlignment: Text.AlignRight
                    text: i18nc("vibration strength in percent", "%1%", Math.round(vibrationStrengthSlider.value))
                }

                QQC2.Slider {
                    id: vibrationStrengthSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    stepSize: 5
                    snapMode: QQC2.Slider.SnapAlways
                    enabled: vibrationEnabledSwitch.checked
                    value: kcm.vibrationStrength

                    onMoved: {
                        kcm.vibrationStrength = value;
                        value = Qt.binding(() => kcm.vibrationStrength);
                    }

                    Accessible.name: i18n("Vibration strength")
                }
            }

            SettingsRow {
                label: i18n("Navigation:")
                description: i18n("Arrow keys move between the keys")

                QQC2.Switch {
                    id: keyboardNavigationSwitch
                    checked: kcm.keyboardNavigationEnabled
                    onCheckedChanged: {
                        kcm.keyboardNavigationEnabled = checked;
                        checked = Qt.binding(() => kcm.keyboardNavigationEnabled);
                    }
                }
            }
        }

        SttSettingsPage {
            id: sttPage
        }
    }

    FontChooserDialog {
        id: fontChooserDialog

        onAccepted: kcm.keyboardFontFamily = selectedFamily
    }

    FileDialog {
        id: importThemeDialog
        title: i18n("Import theme")
        fileMode: FileDialog.OpenFile
        nameFilters: [i18n("Theme files (*.json)"), i18n("All files (*)")]

        onAccepted: {
            const error = kcm.installTheme(selectedFile);
            if (error.length > 0) {
                themeError.text = i18n("Could not import the theme: %1", error);
            }
        }
    }

    FileDialog {
        id: exportThemeDialog
        property string themeId
        title: i18n("Export theme")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [i18n("Theme files (*.json)"), i18n("All files (*)")]

        onAccepted: {
            const error = kcm.exportTheme(themeId, selectedFile);
            if (error.length > 0) {
                themeError.text = i18n("Could not export the theme: %1", error);
            }
        }
    }

    Kirigami.PromptDialog {
        id: removeThemeDialog
        title: i18n("Remove theme?")
        subtitle: i18n("The theme \"%1\" will be deleted permanently.", root.currentTheme.name || "")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel

        onAccepted: {
            const error = kcm.removeUserTheme(root.currentTheme.id);
            if (error.length > 0) {
                themeError.text = i18n("Could not remove the theme: %1", error);
            }
        }
    }
}
