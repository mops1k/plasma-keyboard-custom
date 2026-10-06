/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

/**
 * Voice input settings: the switch that turns the feature on, the engine and
 * the model it uses, the microphone and the recognition language.
 *
 * The models are downloaded and removed here as well, so the whole feature can
 * be set up without leaving the settings.
 */
SettingsFormPage {
    id: page

    readonly property var engines: [
        {
            "value": "parakeet",
            "text": i18n("Parakeet v3 (default)")
        },
        {
            "value": "whisper",
            "text": i18n("Whisper")
        },
        {
            "value": "gigaam",
            "text": i18n("GigaAM v3 (Russian)")
        },
        {
            "value": "vosk",
            "text": i18n("Vosk")
        }
    ]

    readonly property var languages: [
        {
            "value": "",
            "text": i18n("Automatic")
        },
        {
            "value": "ru",
            "text": i18n("Russian")
        },
        {
            "value": "en",
            "text": i18n("English")
        },
        {
            "value": "de",
            "text": i18n("German")
        },
        {
            "value": "fr",
            "text": i18n("French")
        },
        {
            "value": "es",
            "text": i18n("Spanish")
        },
        {
            "value": "it",
            "text": i18n("Italian")
        },
        {
            "value": "uk",
            "text": i18n("Ukrainian")
        },
        {
            "value": "pt",
            "text": i18n("Portuguese")
        }
    ]

    //! The models of the selected engine, in the order of the catalog.
    readonly property var engineModels: kcm.sttModels.filter(model => model.engine === kcm.sttEngine)

    //! Message of a removal that did not work.
    property string removeErrorText: ""

    SettingsRow {
        label: i18n("Voice input:")
        description: i18n("Recognise speech locally, without sending it anywhere. A microphone key appears next to the space bar while this is on.")

        QQC2.Switch {
            checked: kcm.sttEnabled
            onCheckedChanged: {
                kcm.sttEnabled = checked;
                checked = Qt.binding(() => kcm.sttEnabled);
            }
        }
    }

    SettingsRow {
        label: i18n("Engine:")
        description: i18n("Parakeet v3 recognises the most languages and is the fastest; Whisper is the classic model; GigaAM v3 is trained for Russian; Vosk has the smallest models.")
        controlFillWidth: true

        QQC2.ComboBox {
            id: engineComboBox
            Layout.fillWidth: true
            enabled: kcm.sttEnabled
            textRole: "text"
            valueRole: "value"
            model: page.engines
            currentIndex: {
                for (let i = 0; i < page.engines.length; i++) {
                    if (page.engines[i].value === kcm.sttEngine) {
                        return i;
                    }
                }
                return 0;
            }
            onActivated: kcm.sttEngine = currentValue
        }
    }

    Kirigami.Heading {
        Layout.fillWidth: true
        level: 3
        text: i18n("Models")
        visible: kcm.sttEnabled
    }

    Repeater {
        model: page.engineModels

        delegate: SettingsRow {
            required property var modelData

            label: modelData.name
            description: {
                const parts = [modelData.sizeText];
                if (modelData.language.length > 0) {
                    parts.push(modelData.language);
                }
                if (modelData.installed) {
                    parts.push(i18n("installed"));
                }
                return parts.join(" · ");
            }
            visible: kcm.sttEnabled

            RowLayout {
                spacing: Kirigami.Units.smallSpacing

                QQC2.ProgressBar {
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 6
                    visible: modelData.busy
                    from: 0
                    to: 1
                    value: modelData.progress
                }

                QQC2.Button {
                    text: i18n("Cancel")
                    visible: modelData.busy
                    onClicked: kcm.cancelSttDownload()
                }

                QQC2.Button {
                    text: modelData.installed ? i18n("Reinstall") : i18n("Download")
                    visible: !modelData.busy
                    onClicked: kcm.downloadSttModel(modelData.id)
                }

                QQC2.Button {
                    text: i18n("Remove")
                    visible: modelData.installed && !modelData.busy
                    onClicked: {
                        removeDialog.modelId = modelData.id;
                        removeDialog.modelName = modelData.name;
                        removeDialog.open();
                    }
                }
            }
        }
    }

    QQC2.Label {
        Layout.fillWidth: true
        visible: kcm.sttEnabled && page.engineModels.length === 0
        text: i18n("There are no models for this engine.")
        color: Kirigami.Theme.disabledTextColor
        font: Kirigami.Theme.smallFont
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        visible: kcm.sttDownloadError.length > 0
        type: Kirigami.MessageType.Error
        text: kcm.sttDownloadError
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        visible: page.removeErrorText.length > 0
        type: Kirigami.MessageType.Error
        text: page.removeErrorText
    }

    SettingsRow {
        label: i18n("Microphone:")
        description: i18n("Which microphone the voice input records from.")
        controlFillWidth: true
        visible: kcm.sttEnabled

        QQC2.ComboBox {
            id: microphoneComboBox
            Layout.fillWidth: true
            textRole: "name"
            valueRole: "id"
            model: [{
                    "id": "",
                    "name": i18n("System default")
                }].concat(kcm.sttInputDevices)
            currentIndex: {
                for (let i = 0; i < model.length; i++) {
                    if (model[i].id === kcm.sttInputDevice) {
                        return i;
                    }
                }
                return 0;
            }
            onActivated: kcm.sttInputDevice = currentValue
        }
    }

    SettingsRow {
        label: i18n("Language:")
        description: i18n("Parakeet v3 recognises the language on its own; the other engines use this setting.")
        controlFillWidth: true
        visible: kcm.sttEnabled

        QQC2.ComboBox {
            id: languageModeComboBox
            Layout.fillWidth: true
            textRole: "text"
            valueRole: "value"
            model: [
                {
                    "value": "keyboard",
                    "text": i18n("Follow the keyboard layout")
                },
                {
                    "value": "fixed",
                    "text": i18n("Always use one language")
                }
            ]
            currentIndex: kcm.sttLanguageMode === "fixed" ? 1 : 0
            onActivated: kcm.sttLanguageMode = currentValue
        }
    }

    SettingsRow {
        label: i18n("Recognition language:")
        controlFillWidth: true
        visible: kcm.sttEnabled && kcm.sttLanguageMode === "fixed"

        QQC2.ComboBox {
            Layout.fillWidth: true
            textRole: "text"
            valueRole: "value"
            model: page.languages
            currentIndex: {
                for (let i = 0; i < page.languages.length; i++) {
                    if (page.languages[i].value === kcm.sttLanguage) {
                        return i;
                    }
                }
                return 0;
            }
            onActivated: kcm.sttLanguage = currentValue
        }
    }

    // Removing a model frees a lot of disk space, so it is confirmed first.
    QQC2.Dialog {
        id: removeDialog

        property string modelId: ""
        property string modelName: ""

        anchors.centerIn: parent
        modal: true
        title: i18n("Remove the model")
        standardButtons: QQC2.Dialog.Cancel | QQC2.Dialog.Ok

        contentItem: QQC2.Label {
            text: i18n("Remove “%1” from the disk? The voice input cannot use it until it is downloaded again.", removeDialog.modelName)
            wrapMode: Text.WordWrap
        }

        onAccepted: {
            const error = kcm.removeSttModel(removeDialog.modelId);
            page.removeErrorText = error;
        }
    }
}
