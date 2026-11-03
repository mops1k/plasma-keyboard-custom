/*
    SPDX-FileCopyrightText: 2025 Devin Lin <devin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import QtQuick.Dialogs

import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami
import org.kde.bigscreen as Bigscreen

KCM.SimpleKCM {
    id: keyboardSettingsView

    readonly property var currentTheme: kcm.availableThemes[themeComboBox.currentIndex] || ({})

    title: i18n("On-Screen Keyboard")
    background: null

    leftPadding: Kirigami.Units.smallSpacing
    topPadding: Kirigami.Units.smallSpacing
    rightPadding: Kirigami.Units.smallSpacing
    bottomPadding: Kirigami.Units.smallSpacing

    onActiveFocusChanged: {
        if (activeFocus) {
            changeLanguagesButton.forceActiveFocus()
        }
    }

    ColumnLayout {
        id: column
        KeyNavigation.left: keyboardSettingsView.KeyNavigation.left
        spacing: 0

        Bigscreen.ButtonDelegate {
            id: changeLanguagesButton
            text: i18n("Languages")
            onClicked: localeSelectorSidebar.open()
            KeyNavigation.down: soundOnKeypressButton
        }

        QQC2.Label {
            id: keyPressFeedbackLabel
            text: i18n("Feedback")
            font.pixelSize: 22
            font.weight: Font.Normal
            Layout.topMargin: Kirigami.Units.smallSpacing
            Layout.bottomMargin: Kirigami.Units.smallSpacing
        }

        Bigscreen.SwitchDelegate {
            id: soundOnKeypressButton
            text: i18n("Sound")
            KeyNavigation.up: changeLanguagesButton
            KeyNavigation.down: vibrationOnKeypressButton

            checked: kcm.soundEnabled
            onCheckedChanged: {
                kcm.soundEnabled = checked;
                checked = Qt.binding(() => kcm.soundEnabled);
            }
        }

        Bigscreen.SwitchDelegate {
            id: vibrationOnKeypressButton
            text: i18n("Vibration")
            KeyNavigation.up: soundOnKeypressButton
            KeyNavigation.down: autoCapitalizationButton

            checked: kcm.vibrationEnabled
            onCheckedChanged: {
                kcm.vibrationEnabled = checked;
                checked = Qt.binding(() => kcm.vibrationEnabled);
            }
        }

        QQC2.Label {
            id: generalLabel
            text: i18n("Opening the keyboard")
            font.pixelSize: 22
            font.weight: Font.Normal
            Layout.topMargin: Kirigami.Units.smallSpacing
            Layout.bottomMargin: Kirigami.Units.smallSpacing
        }

        Bigscreen.SwitchDelegate {
            id: showOnLongTapButton
            text: i18n("Open on long press")
            KeyNavigation.up: vibrationOnKeypressButton
            KeyNavigation.down: showOnMouseFocusButton

            checked: kcm.showOnLongTap
            onCheckedChanged: {
                kcm.showOnLongTap = checked;
                checked = Qt.binding(() => kcm.showOnLongTap);
            }
        }

        Bigscreen.SwitchDelegate {
            id: showOnMouseFocusButton
            text: i18n("Open when focused with a mouse")
            KeyNavigation.up: showOnLongTapButton
            KeyNavigation.down: hidePanelWhenKeyboardVisibleButton

            checked: kcm.showOnMouseFocus
            onCheckedChanged: {
                kcm.showOnMouseFocus = checked;
                checked = Qt.binding(() => kcm.showOnMouseFocus);
            }
        }

        Bigscreen.SwitchDelegate {
            id: hidePanelWhenKeyboardVisibleButton
            text: i18n("Hide the panel while the keyboard is open")
            KeyNavigation.up: showOnMouseFocusButton
            KeyNavigation.down: showFunctionKeyRowButton

            checked: kcm.hidePanelWhenKeyboardVisible
            onCheckedChanged: {
                kcm.hidePanelWhenKeyboardVisible = checked;
                checked = Qt.binding(() => kcm.hidePanelWhenKeyboardVisible);
            }
        }

        QQC2.Label {
            id: appearanceLabel
            text: i18n("Appearance")
            font.pixelSize: 22
            font.weight: Font.Normal
            Layout.topMargin: Kirigami.Units.smallSpacing
            Layout.bottomMargin: Kirigami.Units.smallSpacing
        }

        Bigscreen.SwitchDelegate {
            id: showFunctionKeyRowButton
            text: i18n("Function keys")
            KeyNavigation.up: hidePanelWhenKeyboardVisibleButton
            KeyNavigation.down: clipboardEnabledButton

            checked: kcm.showFunctionKeyRow
            onCheckedChanged: {
                kcm.showFunctionKeyRow = checked;
                checked = Qt.binding(() => kcm.showFunctionKeyRow);
            }
        }

        Bigscreen.SwitchDelegate {
            id: clipboardEnabledButton
            text: i18n("Clipboard")
            KeyNavigation.up: showFunctionKeyRowButton
            KeyNavigation.down: themeComboBox

            checked: kcm.clipboardEnabled
            onCheckedChanged: {
                kcm.clipboardEnabled = checked;
                checked = Qt.binding(() => kcm.clipboardEnabled);
            }
        }

        QQC2.ComboBox {
            id: themeComboBox
            Layout.preferredWidth: column.width
            KeyNavigation.up: clipboardEnabledButton
            KeyNavigation.down: importThemeButton

            model: kcm.availableThemes
            textRole: "name"
            valueRole: "id"
            currentIndex: Math.max(0, model.findIndex(theme => theme.id === kcm.theme))

            onActivated: (index) => {
                kcm.theme = model[index].id;
            }
        }

        Bigscreen.ButtonDelegate {
            id: importThemeButton
            text: i18n("Import theme…")
            KeyNavigation.up: themeComboBox
            KeyNavigation.down: exportThemeButton

            onClicked: {
                themeError.text = "";
                importThemeDialog.open();
            }
        }

        Bigscreen.ButtonDelegate {
            id: exportThemeButton
            text: i18n("Export theme…")
            KeyNavigation.up: importThemeButton
            KeyNavigation.down: removeThemeButton

            onClicked: {
                themeError.text = "";
                exportThemeDialog.themeId = themeComboBox.currentValue;
                exportThemeDialog.open();
            }
        }

        Bigscreen.ButtonDelegate {
            id: removeThemeButton
            text: i18n("Remove theme")
            enabled: keyboardSettingsView.currentTheme.source === "user"
            KeyNavigation.up: exportThemeButton
            KeyNavigation.down: keyboardFontComboBox

            onClicked: {
                themeError.text = "";
                removeThemeDialog.open();
            }
        }

        Kirigami.InlineMessage {
            id: themeError
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            visible: text.length > 0
        }

        QQC2.ComboBox {
            id: keyboardFontComboBox
            Layout.preferredWidth: column.width
            KeyNavigation.up: removeThemeButton
            KeyNavigation.down: autoCapitalizationButton

            model: [i18n("Default")].concat(Qt.fontFamilies())
            currentIndex: Math.max(0, model.indexOf(kcm.keyboardFontFamily))

            onActivated: (index) => {
                kcm.keyboardFontFamily = index === 0 ? "" : model[index];
            }
        }

        QQC2.Label {
            id: typingLabel
            text: i18n("Typing")
            font.pixelSize: 22
            font.weight: Font.Normal
            Layout.topMargin: Kirigami.Units.smallSpacing
            Layout.bottomMargin: Kirigami.Units.smallSpacing
        }

        Bigscreen.SwitchDelegate {
            id: autoCapitalizationButton
            text: i18n("Automatic capitalization")
            KeyNavigation.up: keyboardFontComboBox
            KeyNavigation.down: diacriticsButton

            checked: kcm.autoCapitalizationEnabled
            onCheckedChanged: {
                kcm.autoCapitalizationEnabled = checked;
                checked = Qt.binding(() => kcm.autoCapitalizationEnabled);
            }
        }

        Bigscreen.SwitchDelegate {
            id: diacriticsButton
            text: i18n("Alternate characters")
            KeyNavigation.up: autoCapitalizationButton

            checked: kcm.diacriticsPopupEnabled
            onCheckedChanged: {
                kcm.diacriticsPopupEnabled = checked;
                checked = Qt.binding(() => kcm.diacriticsPopupEnabled);
            }
        }

        Bigscreen.SwitchDelegate {
            id: gamepadAlternatesButton
            text: i18n("Gamepad alternate characters")
            KeyNavigation.up: diacriticsButton

            checked: kcm.gamepadAlternatesEnabled
            onCheckedChanged: {
                kcm.gamepadAlternatesEnabled = checked;
                checked = Qt.binding(() => kcm.gamepadAlternatesEnabled);
            }
        }

        Bigscreen.SwitchDelegate {
            id: predictiveTextButton
            text: i18n("Word suggestions")
            KeyNavigation.up: gamepadAlternatesButton

            checked: kcm.predictiveTextEnabled
            onCheckedChanged: {
                kcm.predictiveTextEnabled = checked;
                checked = Qt.binding(() => kcm.predictiveTextEnabled);
            }
        }

        Bigscreen.SwitchDelegate {
            id: swipeTypingButton
            text: i18n("Swipe typing")
            KeyNavigation.up: predictiveTextButton

            checked: kcm.swipeTypingEnabled
            onCheckedChanged: {
                kcm.swipeTypingEnabled = checked;
                checked = Qt.binding(() => kcm.swipeTypingEnabled);
            }
        }

        Bigscreen.SwitchDelegate {
            id: swipeLearningButton
            text: i18n("Remember typed words")
            enabled: swipeTypingButton.checked
            KeyNavigation.up: swipeTypingButton

            checked: kcm.swipeLearningEnabled
            onCheckedChanged: {
                kcm.swipeLearningEnabled = checked;
                checked = Qt.binding(() => kcm.swipeLearningEnabled);
            }
        }

        Bigscreen.SwitchDelegate {
            id: predictiveNextWordButton
            text: i18n("Next word")
            KeyNavigation.up: predictiveTextButton
            enabled: predictiveTextButton.checked

            checked: kcm.predictiveNextWordEnabled
            onCheckedChanged: {
                kcm.predictiveNextWordEnabled = checked;
                checked = Qt.binding(() => kcm.predictiveNextWordEnabled);
            }
        }

        Bigscreen.SwitchDelegate {
            id: predictiveTypoCorrectionButton
            text: i18n("Typo correction")
            KeyNavigation.up: predictiveNextWordButton
            enabled: predictiveTextButton.checked

            checked: kcm.predictiveTypoCorrectionEnabled
            onCheckedChanged: {
                kcm.predictiveTypoCorrectionEnabled = checked;
                checked = Qt.binding(() => kcm.predictiveTypoCorrectionEnabled);
            }
        }

        LocaleSelectorSidebar {
            id: localeSelectorSidebar
            onClosed: changeLanguagesButton.forceActiveFocus()
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
        subtitle: i18n("The theme \"%1\" will be deleted permanently.", keyboardSettingsView.currentTheme.name || "")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel

        onAccepted: {
            const error = kcm.removeUserTheme(keyboardSettingsView.currentTheme.id);
            if (error.length > 0) {
                themeError.text = i18n("Could not remove the theme: %1", error);
            }
        }
    }
}
