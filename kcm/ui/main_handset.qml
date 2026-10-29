/*
    SPDX-FileCopyrightText: 2025 Devin Lin <devin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import QtQuick.Dialogs

import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.kirigamiaddons.formcard 1.0 as FormCard

KCM.SimpleKCM {
    id: root

    readonly property var currentTheme: kcm.availableThemes[themeComboBox.currentIndex] || ({})

    leftPadding: 0
    rightPadding: 0
    topPadding: Kirigami.Units.gridUnit
    bottomPadding: Kirigami.Units.gridUnit

    ColumnLayout {
        spacing: 0
        width: parent.width

        FormCard.FormCard {
            FormCard.FormTextFieldDelegate {
                label: i18n("Test the virtual keyboard…")
            }
        }

        FormCard.FormCard {
            Layout.topMargin: Kirigami.Units.largeSpacing

            FormCard.FormButtonDelegate {
                id: languageList
                text: i18n("Languages")
                icon.name: 'languages'
                onClicked: kcm.push(localePage)

                Kirigami.ScrollablePage {
                    id: localePage
                    title: i18n("Keyboard Languages")

                    LocaleSelectorListView {}
                }
            }
        }

        FormCard.FormHeader {
            title: i18nc("@title:group", "Opening the keyboard")
        }

        FormCard.FormCard {
            FormCard.FormSwitchDelegate {
                id: showOnLongTap
                text: i18n("Open on long press")
                description: i18n("Hold a finger on a text field to open the keyboard")

                checked: kcm.showOnLongTap
                onCheckedChanged: {
                    kcm.showOnLongTap = checked;
                    checked = Qt.binding(() => kcm.showOnLongTap)
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSpinBoxDelegate {
                id: showOnLongTapThreshold
                label: i18n("Long press delay")
                description: i18n("Time to hold a finger on the screen")
                from: 100
                to: 5000
                stepSize: 100
                enabled: showOnLongTap.checked
                value: kcm.showOnLongTapThresholdMs
                onValueChanged: kcm.showOnLongTapThresholdMs = value

                textFromValue: function (value) {
                    return i18nc("duration in milliseconds", "%1 ms", value);
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSwitchDelegate {
                id: showOnMouseFocus
                text: i18n("Open when focused with a mouse")
                description: i18n("Otherwise it only opens on touch input or via the shortcut")

                checked: kcm.showOnMouseFocus
                onCheckedChanged: {
                    kcm.showOnMouseFocus = checked;
                    checked = Qt.binding(() => kcm.showOnMouseFocus)
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSwitchDelegate {
                id: hidePanelWhenKeyboardVisible
                text: i18n("Hide the panel while the keyboard is open")
                description: i18n("The keyboard reaches the bottom of the screen")

                checked: kcm.hidePanelWhenKeyboardVisible
                onCheckedChanged: {
                    kcm.hidePanelWhenKeyboardVisible = checked;
                    checked = Qt.binding(() => kcm.hidePanelWhenKeyboardVisible)
                }
            }
        }

        FormCard.FormHeader {
            title: i18nc("@title:group", "Appearance")
        }

        FormCard.FormCard {
            FormCard.FormSpinBoxDelegate {
                label: i18n("Keyboard height (%)")
                description: i18n("Percentage of the screen height")
                from: 20
                to: 80
                stepSize: 2
                value: kcm.keyboardHeightPercent
                onValueChanged: kcm.keyboardHeightPercent = value
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSpinBoxDelegate {
                label: i18n("Floating keyboard width (%)")
                description: i18n("Percentage of the screen width")
                from: 20
                to: 100
                stepSize: 5
                value: kcm.floatingKeyboardWidthPercent
                onValueChanged: kcm.floatingKeyboardWidthPercent = value
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSpinBoxDelegate {
                label: i18n("Floating keyboard opacity (%)")
                description: i18n("Percentage of opacity, 100% is fully opaque")
                from: 20
                to: 100
                stepSize: 5
                value: kcm.floatingKeyboardOpacity
                onValueChanged: kcm.floatingKeyboardOpacity = value
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormComboBoxDelegate {
                id: keyboardFontComboBox
                text: i18n("Keyboard font")

                model: [i18n("Default")].concat(Qt.fontFamilies())
                currentIndex: Math.max(0, model.indexOf(kcm.keyboardFontFamily))

                onActivated: (index) => {
                    kcm.keyboardFontFamily = index === 0 ? "" : model[index];
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormComboBoxDelegate {
                id: themeComboBox
                text: i18n("Theme")

                model: kcm.availableThemes
                textRole: "name"
                valueRole: "id"
                currentIndex: Math.max(0, model.findIndex(theme => theme.id === kcm.theme))

                onActivated: (index) => {
                    kcm.theme = model[index].id;
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormButtonDelegate {
                id: importThemeButton
                text: i18n("Import theme…")
                icon.name: "document-import"
                onClicked: {
                    themeError.text = "";
                    importThemeDialog.open();
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormButtonDelegate {
                id: exportThemeButton
                text: i18n("Export theme…")
                icon.name: "document-export"
                onClicked: {
                    themeError.text = "";
                    exportThemeDialog.themeId = themeComboBox.currentValue;
                    exportThemeDialog.open();
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormButtonDelegate {
                id: removeThemeButton
                text: i18n("Remove theme")
                icon.name: "edit-delete"
                enabled: root.currentTheme.source === "user"
                onClicked: {
                    themeError.text = "";
                    removeThemeDialog.open();
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSwitchDelegate {
                id: showFunctionKeyRow
                text: i18n("Function keys")
                description: i18n("Show an F1–F12 row above the keyboard")

                checked: kcm.showFunctionKeyRow
                onCheckedChanged: {
                    kcm.showFunctionKeyRow = checked;
                    checked = Qt.binding(() => kcm.showFunctionKeyRow)
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSwitchDelegate {
                id: clipboardEnabled
                text: i18n("Clipboard")
                description: i18n("Show recent clipboard entries above the keyboard")

                checked: kcm.clipboardEnabled
                onCheckedChanged: {
                    kcm.clipboardEnabled = checked;
                    checked = Qt.binding(() => kcm.clipboardEnabled)
                }
            }
        }

        Kirigami.InlineMessage {
            id: themeError
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
            type: Kirigami.MessageType.Error
            visible: text.length > 0
        }

        FormCard.FormHeader {
            title: i18nc("@title:group", "Typing")
        }

        FormCard.FormCard {
            FormCard.FormSwitchDelegate {
                id: autoCapitalizationEnabled
                text: i18n("Automatic capitalization")
                description: i18n("Capitalize the first letter of a sentence")

                checked: kcm.autoCapitalizationEnabled
                onCheckedChanged: {
                    kcm.autoCapitalizationEnabled = checked;
                    checked = Qt.binding(() => kcm.autoCapitalizationEnabled)
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSwitchDelegate {
                id: diacriticsCheckbox
                text: i18n("Alternate characters")
                description: i18n("Show a popup when holding a key")

                checked: kcm.diacriticsPopupEnabled
                onCheckedChanged: {
                    kcm.diacriticsPopupEnabled = checked;
                    checked = Qt.binding(() => kcm.diacriticsPopupEnabled)
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSpinBoxDelegate {
                label: i18n("Hold delay")
                description: i18n("Time to hold a key before the popup appears")
                from: 100
                to: 1500
                stepSize: 50
                enabled: diacriticsCheckbox.checked
                value: kcm.diacriticsHoldThresholdMs
                onValueChanged: kcm.diacriticsHoldThresholdMs = value

                textFromValue: function (value) {
                    return i18nc("duration in milliseconds", "%1 ms", value);
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSwitchDelegate {
                id: gamepadAlternatesCheckbox
                text: i18n("Gamepad alternate characters")
                description: i18n("Hold the A button on a gamepad to pick an alternate character of the highlighted key")

                checked: kcm.gamepadAlternatesEnabled
                onCheckedChanged: {
                    kcm.gamepadAlternatesEnabled = checked;
                    checked = Qt.binding(() => kcm.gamepadAlternatesEnabled)
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSpinBoxDelegate {
                label: i18n("Gamepad hold delay")
                description: i18n("Time to hold the A button before the popup appears")
                from: 100
                to: 1500
                stepSize: 50
                enabled: gamepadAlternatesCheckbox.checked
                value: kcm.gamepadAlternatesThresholdMs
                onValueChanged: kcm.gamepadAlternatesThresholdMs = value

                textFromValue: function (value) {
                    return i18nc("duration in milliseconds", "%1 ms", value);
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSwitchDelegate {
                id: predictiveTextCheckbox
                text: i18n("Word suggestions")
                description: i18n("Offer words that continue what is being typed")

                checked: kcm.predictiveTextEnabled
                onCheckedChanged: {
                    kcm.predictiveTextEnabled = checked;
                    checked = Qt.binding(() => kcm.predictiveTextEnabled)
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSwitchDelegate {
                id: swipeTypingCheckbox
                text: i18n("Swipe typing")
                description: i18n("Type a word by dragging across its letters")

                checked: kcm.swipeTypingEnabled
                onCheckedChanged: {
                    kcm.swipeTypingEnabled = checked;
                    checked = Qt.binding(() => kcm.swipeTypingEnabled)
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSpinBoxDelegate {
                label: i18n("Suggestions at once")
                description: i18n("How many word suggestions are offered at the same time")
                from: 1
                to: 5
                enabled: predictiveTextCheckbox.checked
                value: kcm.predictiveSuggestionCount
                onValueChanged: kcm.predictiveSuggestionCount = value
            }

            FormCard.FormSpinBoxDelegate {
                label: i18n("Letters before suggesting")
                description: i18n("How many letters have to be typed before word suggestions appear")
                from: 1
                to: 4
                enabled: predictiveTextCheckbox.checked
                value: kcm.predictiveMinPrefixLength
                onValueChanged: kcm.predictiveMinPrefixLength = value
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSwitchDelegate {
                id: predictiveNextWordCheckbox
                text: i18n("Next word")
                description: i18n("Offer words that may follow the typed one")

                enabled: predictiveTextCheckbox.checked
                checked: kcm.predictiveNextWordEnabled
                onCheckedChanged: {
                    kcm.predictiveNextWordEnabled = checked;
                    checked = Qt.binding(() => kcm.predictiveNextWordEnabled)
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSwitchDelegate {
                id: predictiveTypoCorrectionCheckbox
                text: i18n("Typo correction")
                description: i18n("Offer to correct a mistyped word")

                enabled: predictiveTextCheckbox.checked
                checked: kcm.predictiveTypoCorrectionEnabled
                onCheckedChanged: {
                    kcm.predictiveTypoCorrectionEnabled = checked;
                    checked = Qt.binding(() => kcm.predictiveTypoCorrectionEnabled)
                }
            }
        }

        FormCard.FormHeader {
            title: i18nc("@title:group", "Feedback")
        }

        FormCard.FormCard {
            FormCard.FormSwitchDelegate {
                id: soundsEnabled
                text: i18n("Sound")
                description: i18n("Whether to emit a sound on key press")

                checked: kcm.soundEnabled
                onCheckedChanged: {
                    kcm.soundEnabled = checked;
                    checked = Qt.binding(() => kcm.soundEnabled)
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSwitchDelegate {
                id: vibrationEnabled
                text: i18n("Vibration")
                description: i18n("Whether to vibrate on key press")

                checked: kcm.vibrationEnabled
                onCheckedChanged: {
                    kcm.vibrationEnabled = checked;
                    checked = Qt.binding(() => kcm.vibrationEnabled)
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSpinBoxDelegate {
                label: i18n("Vibration strength")
                description: i18n("Percentage of the maximum vibration")
                from: 0
                to: 100
                stepSize: 5
                enabled: vibrationEnabled.checked
                value: kcm.vibrationStrength
                onValueChanged: kcm.vibrationStrength = value

                textFromValue: function (value) {
                    return i18nc("vibration strength in percent", "%1%", value);
                }
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSwitchDelegate {
                id: keyboardNavigationEnabled
                text: i18n("Keyboard navigation")
                description: i18n("Whether to use the arrow keys to navigate the keyboard")

                checked: kcm.keyboardNavigationEnabled
                onCheckedChanged: {
                    kcm.keyboardNavigationEnabled = checked;
                    checked = Qt.binding(() => kcm.keyboardNavigationEnabled)
                }
            }
        }
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
