/*
    SPDX-FileCopyrightText: 2024 Aleix Pol i Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2026 Kristen McWilliam <kristen@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Settings

import org.kde.plasma.keyboard.custom
import org.kde.plasma.keyboard.custom.lib as PlasmaKeyboard

import org.kde.kirigami as Kirigami

InputPanelWindow {
    id: root
    height: Screen.height
    width: Screen.width
    color: 'transparent'

    // The style sizes the panel from the screen size, but as a QtObject its own
    // Screen attached property does not follow this window (see style.qml).
    // This window is always as large as the screen the keyboard is on.
    Binding {
        target: inputPanel.keyboard ? inputPanel.keyboard.style : null
        property: "screenWidth"
        value: root.width
        when: inputPanel.keyboard && inputPanel.keyboard.style && inputPanel.keyboard.style.screenWidth !== undefined
    }
    Binding {
        target: inputPanel.keyboard ? inputPanel.keyboard.style : null
        property: "screenHeight"
        value: root.height
        when: inputPanel.keyboard && inputPanel.keyboard.style && inputPanel.keyboard.style.screenWidth !== undefined
    }

    onVisibleChanged: {
        if (visible) {
            // The window is unmapped while the keyboard is hidden, which also
            // drops the input focus inside it. Qt Virtual Keyboard delivers the
            // typed text to the input item, so it has to be focused again or
            // the keys would be tapped without anything appearing in the field.
            thing.forceActiveFocus();
        } else {
            // Reset keyboard navigation when hidden
            // Note: keyboard property is internal Qt API
            if (inputPanel.keyboard.navigationModeActive) {
                inputPanel.keyboard.navigationModeActive = false;
            }

            // Do not keep a focus in the rows above the keyboard either.
            extraRowFocus = 0;
            extraColumn = 0;

            // The gamepad highlight of the voice page is gone with the window.
            voiceNavigationActive = false;

            // The word being typed is gone with the field it was typed into.
            predictions = [];
            suggestionsRow.clipboardChosen = false;

            // Close language dialog
            languageDialog.close();
        }
    }

    // Qt Virtual Keyboard keeps the panel (and the keys) disabled unless the
    // window that holds the input item is the active window: the keyboard window
    // is activated when it is shown, and the input item has to be focused again
    // at that point, otherwise the active focus stays on the window root.
    onActiveChanged: {
        if (active) {
            thing.forceActiveFocus();
            // Qt Virtual Keyboard decides whether the panel is visible when the
            // input panel is asked for: the window only became active now, so ask
            // again with the input item focused.
            Qt.inputMethod.show();
        }
    }

    // Gamepad: which of the rows above the keyboard has the focus
    // (0 = the keyboard itself, 1 = the clipboard row, 2 = the F1-F12 row) and
    // which item of it is selected. Those rows are plain items, so the
    // navigation of Qt Virtual Keyboard cannot reach them on its own.
    property int extraRowFocus: 0
    property int extraColumn: 0

    //! What the row above the keyboard offers for what is being typed, most
    //! frequent first. Every entry is a record with the word itself, where it
    //! came from ("complete" — the word being typed, continued; "correct" — the
    //! word being typed, with the typo it may have; "next" — the word that may
    //! follow the previous one) and how much of it would be added to the text.
    property var predictions: []

    //! Whether the voice input mode is on: the keys are covered by one big
    //! microphone button. Only reachable while the voice input is enabled.
    property bool voiceMode: false

    //! Whether the gamepad has taken over the voice page: the highlight is on
    //! one of its controls, and the directions move it instead of the (hidden)
    //! keys of Qt Virtual Keyboard.
    property bool voiceNavigationActive: false

    //! Opens the voice input mode from the microphone key of a layout.
    function openVoiceMode() {
        if (PlasmaKeyboardSettings.sttEnabled) {
            root.voiceMode = true;
            root.voiceNavigationActive = false;
            voicePanel.focusIndex = voicePanel.microphoneIndex;
        }
    }

    //! Leaves the voice input mode and drops a recording in progress.
    function closeVoiceMode() {
        PlasmaKeyboard.Stt.cancel();
        root.voiceMode = false;
        root.voiceNavigationActive = false;
    }

    //! The word lists the completions and the corrections are looked up in.
    PredictiveDictionary {
        id: predictor
    }

    //! The bigram lists the next-word predictions are looked up in.
    WordPredictor {
        id: nextWordPredictor
    }

    //! @p words as the entries of the row, told where they came from.
    function predictionItems(words, kind) {
        const typedLength = kind === "complete" ? thing.predictionPrefix.length : 0;
        return words.map(word => ({
            "text": word,
            "kind": kind,
            "completion": kind === "complete" ? Math.max(0, word.length - typedLength) : 0
        }));
    }

    //! Re-reads what the row above the keyboard offers. What is being typed
    //! comes first: the words that continue it, or the word it may have been
    //! meant to be when nothing continues it. With no word being typed, the
    //! words that may follow the previous one are offered. The language is the
    //! one the text field asked for.
    function updatePredictions() {
        if (!PlasmaKeyboardSettings.predictiveTextEnabled) {
            predictions = [];
            return;
        }

        const locale = inputPanel.InputContext.locale;
        const prefix = thing.predictionPrefix;
        if (prefix.length >= PlasmaKeyboardSettings.predictiveMinPrefixLength) {
            const completions = predictor.complete(prefix, PlasmaKeyboardSettings.predictiveSuggestionCount, locale);
            if (completions.length > 0) {
                predictions = predictionItems(completions, "complete");
                return;
            }
            // Nothing continues what is typed, so the word may have a typo in
            // it. One or two letters are too little to tell a typo from the
            // beginning of a word, so only longer words are corrected.
            if (PlasmaKeyboardSettings.predictiveTypoCorrectionEnabled && prefix.length >= 2) {
                predictions = predictionItems(predictor.correct(prefix, PlasmaKeyboardSettings.predictiveSuggestionCount, locale), "correct");
                return;
            }
            predictions = [];
            return;
        }

        if (prefix.length === 0 && PlasmaKeyboardSettings.predictiveNextWordEnabled) {
            predictions = predictionItems(nextWordPredictor.predict(thing.predictionContext, PlasmaKeyboardSettings.predictiveSuggestionCount, locale), "next");
            return;
        }

        predictions = [];
    }

    Connections {
        target: thing
        function onPredictionPrefixChanged() {
            root.updatePredictions();
        }
        function onPredictionContextChanged() {
            root.updatePredictions();
        }
    }

    Connections {
        target: PlasmaKeyboardSettings
        function onPredictiveTextEnabledChanged() {
            root.updatePredictions();
        }
        function onPredictiveMinPrefixLengthChanged() {
            root.updatePredictions();
        }
        function onPredictiveSuggestionCountChanged() {
            root.updatePredictions();
        }
        function onPredictiveNextWordEnabledChanged() {
            root.updatePredictions();
        }
        function onPredictiveTypoCorrectionEnabledChanged() {
            root.updatePredictions();
        }
    }

    Connections {
        target: PlasmaKeyboardSettings
        function onSttEnabledChanged() {
            // Turning the voice input off must not leave the mode on screen.
            if (!PlasmaKeyboardSettings.sttEnabled) {
                root.closeVoiceMode();
            }
        }
    }

    //! The recognised phrase is inserted into the field the keyboard types into.
    //! The voice mode stays open: leaving it is up to the user, who does it with
    //! the arrow in the corner.
    Connections {
        target: PlasmaKeyboard.Stt
        function onTextRecognized(text) {
            if (text.length > 0) {
                thing.commitText(text);
            }
        }
    }

    Connections {
        target: inputPanel.InputContext
        function onLocaleChanged() {
            root.updatePredictions();
        }
    }

    //! Rows above the keyboard, listed from the keyboard upwards. Rebuilt
    //! whenever a row is shown or hidden, so the whole navigation follows this
    //! one list and disabled or empty rows simply drop out of it.
    readonly property var navRows: {
        const rows = [];
        if (functionKeyRow.visible) {
            rows.push("fkeys");
        }
        if (suggestionsRow.visible) {
            rows.push("suggestions");
        }
        return rows;
    }

    //! Number of selectable items in the given row.
    function extraRowItemCount(zone) {
        if (zone <= 0 || zone > navRows.length) {
            return 0;
        }
        // The row above the keyboard holds the suggestions (or the clipboard
        // entries) and the keys that switch between them and clear the history.
        return navRows[zone - 1] === "suggestions" ? suggestionsRow.itemCount() : 12;
    }

    //! Zone number of the named row (0 when that row is not shown).
    function zoneOf(name) {
        return navRows.indexOf(name) + 1;
    }

    //! Row above the given one (0 = none above the keyboard).
    function rowAbove(zone) {
        const index = zone === 0 ? 0 : zone;
        return index < navRows.length ? index + 1 : 0;
    }

    //! Row below the given one (0 = the keyboard itself).
    function rowBelow(zone) {
        return zone > 1 ? zone - 1 : 0;
    }

    //! Row the focus wraps to when it leaves the keyboard downwards.
    function topExtraRow() {
        return navRows.length;
    }

    //! Item of the given row that lies under the given horizontal fraction.
    function columnForZone(zone, ratio) {
        const count = extraRowItemCount(zone);
        if (count <= 0) {
            return 0;
        }
        return Math.max(0, Math.min(count - 1, Math.floor(ratio * count)));
    }

    //! Moves the keyboard focus sideways to the column under the given fraction.
    function focusKeyboardColumn(ratio) {

        const keyboard = inputPanel.keyboard;
        const highlight = keyboard.navigationHighlight;
        const targetX = ratio * keyboard.width;
        for (let i = 0; i < 12; ++i) {
            const item = highlight ? highlight.highlightItem : null;
            if (!item || item === keyboard) {
                break;
            }
            const centreX = keyboard.mapFromItem(item, item.width / 2, 0).x;
            if (Math.abs(centreX - targetX) < item.width / 2) {
                break;
            }
            const direction = centreX < targetX ? Qt.Key_Right : Qt.Key_Left;
            inputPanel.InputContext.priv.navigationKeyPressed(direction, false);
            inputPanel.InputContext.priv.navigationKeyReleased(direction, false);
        }
    }

    //! Moves the keyboard focus to the first or last row of keys, keeping the
    //! horizontal position under the given fraction.
    function focusKeyboardEdge(ratio, bottom) {

        const keyboard = inputPanel.keyboard;
        const highlight = keyboard.navigationHighlight;
        const rowHeight = keyboard.style ? keyboard.style.targetKeyboardHeight / 5 : keyboard.height / 5;

        // Nothing highlighted yet: one press starts the navigation and moves in
        // the wanted direction.
        if (!highlight || highlight.highlightItem === keyboard) {
            const key = bottom ? Qt.Key_Down : Qt.Key_Up;
            inputPanel.InputContext.priv.navigationKeyPressed(key, false);
            inputPanel.InputContext.priv.navigationKeyReleased(key, false);
        }

        for (let i = 0; i < 6; ++i) {
            const item = highlight ? highlight.highlightItem : null;
            if (!item || item === keyboard) {
                break;
            }
            const centreY = keyboard.mapFromItem(item, 0, item.height / 2).y;
            const atEdge = bottom ? centreY > keyboard.height - rowHeight : centreY < rowHeight;
            if (atEdge) {
                break;
            }
            const key = bottom ? Qt.Key_Down : Qt.Key_Up;
            inputPanel.InputContext.priv.navigationKeyPressed(key, false);
            inputPanel.InputContext.priv.navigationKeyReleased(key, false);
        }

        focusKeyboardColumn(ratio);
    }

    //! Activates the selected item of the focused row.
    function activateExtraRowItem() {
        if (extraRowFocus === zoneOf("suggestions")) {
            suggestionsRow.activate(extraColumn);
            return;
        }
        if (extraRowFocus === zoneOf("fkeys")) {
            thing.sendKeyEvent(Qt.Key_F1 + extraColumn, "");
        }
    }

    //! The key that currently has the navigation highlight, null when the
    //! keyboard has no highlight to act on.
    function highlightedKey() {
        const keyboard = inputPanel.keyboard;
        const highlight = keyboard.navigationHighlight;
        const item = highlight ? highlight.highlightItem : null;
        return item && item !== keyboard ? item : null;
    }

    //! The alternate characters the highlighted key offers, in the form they
    //! would be typed. Empty when the key has none.
    //!
    //! The characters come from the layout (the alternativeKeys of the key),
    //! which is what Qt Virtual Keyboard itself shows on a long press there.
    function highlightedKeyAlternates() {
        const item = highlightedKey();
        if (!item || !item.effectiveAlternativeKeys) {
            return [];
        }
        return thing.alternatesFor(item.effectiveAlternativeKeys, inputPanel.InputContext.uppercase);
    }

    // The gamepad opens the alternate characters of the key that is under the
    // highlight, so it has to know whether there are any before the button is
    // held: the delay only starts when something can be offered.
    function updateGamepadAlternates() {
        // Holding A on the voice page must not offer the alternate characters
        // of a key that is hidden behind it: there A activates the highlighted
        // control instead.
        if (root.voiceMode) {
            gamepad.setAlternatesArmable(false, []);
            return;
        }
        if (!PlasmaKeyboardSettings.gamepadAlternatesEnabled) {
            gamepad.setAlternatesArmable(false, []);
            return;
        }
        gamepad.setAlternatesArmable(root.extraRowFocus === 0, root.highlightedKeyAlternates());
        root.updateAlternatesAnchor();
    }

    //! Put the alternates list over the key it belongs to, the way Qt Virtual
    //! Keyboard puts its own alternate-keys popup there.
    function updateAlternatesAnchor() {
        const item = root.highlightedKey();
        if (!item) {
            return;
        }
        const centre = item.mapToItem(panelWrapper, item.width / 2, 0);
        alternatesList.anchorX = centre.x;
        alternatesList.anchorY = centre.y;
    }

    // The highlighted key is not a normal property of the panel: the highlight
    // moves inside Qt Virtual Keyboard, so it is watched per frame instead.
    Timer {
        interval: 100
        running: root.visible
        repeat: true
        onTriggered: root.updateGamepadAlternates()
    }

    // While the focus is in the row right above the keyboard, keep the keyboard
    // cursor under the selected item, so leaving downwards lands on the key
    // that is below it.
    onExtraColumnChanged: {
        if (extraRowFocus === 1) {
            focusKeyboardEdge((extraColumn + 0.5) / extraRowItemCount(1), false);
        }
    }

    InputListenerItem {
        id: thing
        focus: true
        engine: inputPanel.InputContext.inputEngine

        keyboardNavigationActive: inputPanel.keyboard.navigationModeActive

        onKeyNavigationPressed: (key) => {
            // HACK: invoke the Qt VirtualKeyboard keyboard navigation feature ourselves
            // See https://github.com/qt/qtvirtualkeyboard/blob/6d810ac41df96f1ad984f56e17f16860bec2abbf/src/virtualkeyboard/qvirtualkeyboardinputcontext_p.h#L110
            inputPanel.InputContext.priv.navigationKeyPressed(key, false);
        }
        onKeyNavigationReleased: (key) => {
            // HACK: invoke the Qt VirtualKeyboard keyboard navigation feature ourselves
            inputPanel.InputContext.priv.navigationKeyReleased(key, false);
        }
    }

    // Gamepad support (via InputPlumber's dbus target on the system bus).
    GamepadHandler {
        id: gamepad

        //! The alternates overlay that the gamepad itself opened and drives.
        //! Touch and physical keyboards open the same overlay through the
        //! input method instead, and that one keeps its own selection.
        readonly property bool drivingAlternates: thing.overlayController.overlayVisible && thing.overlayController.alternatesOnly

        onNavigate: (key) => {
            // The voice page has no keys of Qt Virtual Keyboard to move in: its
            // own controls take the highlight, which also turns the highlight on
            // for the first press.
            if (root.voiceMode) {
                root.voiceNavigationActive = true;
                voicePanel.navigate(key);
                return;
            }

            if (drivingAlternates) {
                thing.overlayController.navigateAlternates(key);
                return;
            }

            if (root.extraRowFocus !== 0) {
                const count = root.extraRowItemCount(root.extraRowFocus);
                if (key === Qt.Key_Left) {
                    root.extraColumn = (root.extraColumn + count - 1) % count;
                    return;
                }
                if (key === Qt.Key_Right) {
                    root.extraColumn = (root.extraColumn + 1) % count;
                    return;
                }

                // Up and down move along the rows above the keyboard. Going
                // down past the lowest row returns to the keyboard, going up
                // past the highest one stays there: the row above is the top of
                // the list, so a held direction cannot fall out of it.
                if (key === Qt.Key_Up) {
                    const above = root.rowAbove(root.extraRowFocus);
                    const ratio = (root.extraColumn + 0.5) / count;
                    if (above !== 0) {
                        root.extraRowFocus = above;
                        root.extraColumn = root.columnForZone(above, ratio);
                    } else {
                        // Top of the circle: continue on the bottom row.
                        root.extraRowFocus = 0;
                        root.extraColumn = 0;
                        root.focusKeyboardEdge(ratio, true);
                    }
                    return;
                }

                const below = root.rowBelow(root.extraRowFocus);
                const ratio = (root.extraColumn + 0.5) / count;
                if (below !== 0) {
                    root.extraRowFocus = below;
                    root.extraColumn = root.columnForZone(below, ratio);
                    // The row right above the keyboard: put its focus on the
                    // first row of keys now, while the highlight is hidden, so
                    // leaving downwards needs no movement at all.
                    if (below === 1) {
                        root.focusKeyboardEdge(ratio, false);
                    }
                } else {
                    // Back to the keyboard, on the key below the one that was
                    // selected in the row.
                    root.extraRowFocus = 0;
                    root.extraColumn = 0;
                    root.focusKeyboardEdge(ratio, false);
                }
                return;
            }

            // Qt Virtual Keyboard wraps sideways across rows: on the first and
            // last key of a row the focus should stay in that row instead.
            // Whether a key is at the edge is not guessed from the geometry
            // (rows have different margins and keys different widths): the press
            // is performed, and if it left the row it is undone with the
            // opposite direction and the focus walks to the other end of the
            // same row.
            if (key === Qt.Key_Left || key === Qt.Key_Right) {
                const keyboard = inputPanel.keyboard;
                const highlight = keyboard.navigationHighlight;
                const item = highlight ? highlight.highlightItem : null;
                if (keyboard.navigationModeActive && item && item !== keyboard) {
                    const rowHeight = keyboard.style ? keyboard.style.targetKeyboardHeight / 5 : keyboard.height / 5;
                    const rowY = keyboard.mapFromItem(item, 0, item.height / 2).y;
                    const inSameRow = (what) => what && what !== keyboard && Math.abs(keyboard.mapFromItem(what, 0, what.height / 2).y - rowY) < rowHeight * 0.6;
                    const press = (what) => {
                        inputPanel.InputContext.priv.navigationKeyPressed(what, false);
                        inputPanel.InputContext.priv.navigationKeyReleased(what, false);
                    };
                    const opposite = key === Qt.Key_Left ? Qt.Key_Right : Qt.Key_Left;

                    press(key);
                    if (!inSameRow(highlight.highlightItem)) {
                        // Wrapped onto the neighbouring row: go back and walk to
                        // the other end of this row.
                        press(opposite);
                        for (let i = 0; i < 24; ++i) {
                            press(opposite);
                            if (!inSameRow(highlight.highlightItem)) {
                                press(key);
                                break;
                            }
                        }
                    }
                    return;
                }
            }

            // The rows above the keyboard take part in the navigation: up from
            // the top row of the keyboard enters them, down from the bottom row
            // continues into them after a full circle.
            if (key === Qt.Key_Up || key === Qt.Key_Down) {
                const keyboard = inputPanel.keyboard;
                const highlight = keyboard.navigationHighlight;
                const item = highlight ? highlight.highlightItem : null;
                if (keyboard.navigationModeActive && item && item !== keyboard) {
                    const rowHeight = keyboard.style ? keyboard.style.targetKeyboardHeight / 5 : keyboard.height / 5;
                    const centreY = keyboard.mapFromItem(item, 0, item.height / 2).y;
                    const isTopRow = centreY < rowHeight;
                    const isBottomRow = centreY > keyboard.height - rowHeight;
                    const zone = key === Qt.Key_Up ? (isTopRow ? root.rowAbove(0) : 0) : (isBottomRow ? root.topExtraRow() : 0);
                    if (zone !== 0) {
                        // Keep the position: the focus lands on the item above
                        // (or below) the key that was selected.
                        const ratio = keyboard.mapFromItem(item, item.width / 2, 0).x / keyboard.width;
                        root.extraRowFocus = zone;
                        // The clipboard row always starts at its first entry.
                        root.extraColumn = zone === root.zoneOf("suggestions") ? 0 : root.columnForZone(zone, ratio);
                        return;
                    }

                }
            }

            inputPanel.InputContext.priv.navigationKeyPressed(key, false);
            inputPanel.InputContext.priv.navigationKeyReleased(key, false);
        }
        onActivate: {
            // A takes the control the voice page has highlighted.
            if (root.voiceMode) {
                voicePanel.activate();
                return;
            }

            if (root.extraRowFocus !== 0) {
                root.activateExtraRowItem();
                return;
            }

            inputPanel.InputContext.priv.navigationKeyPressed(Qt.Key_Return, false);
            inputPanel.InputContext.priv.navigationKeyReleased(Qt.Key_Return, false);
        }
        onShowAlternates: alternates => {
            // A key with a single alternate has nothing to choose from: taking
            // it straight away saves a list of one that only needs confirming.
            if (alternates.length === 1) {
                thing.overlayController.commitAlternate(alternates[0]);
                gamepad.clearAlternatesOpen();
                return;
            }
            // Nothing is typed for this: the characters come from the layout of
            // the highlighted key, so no character has to be deleted afterwards.
            thing.overlayController.openAlternates(alternates);
        }
        onConfirmAlternates: {
            // The list takes the highlighted character; when there is nothing
            // selected the list closes instead.
            thing.overlayController.navigateAlternates(Qt.Key_Return);
        }
        onToggleExtraRows: {
            // The rows above the keyboard are covered by the voice page.
            if (root.voiceMode) {
                return;
            }

            const zone = root.rowAbove(root.extraRowFocus);
            if (root.extraRowFocus === 0) {
                const keyboard = inputPanel.keyboard;
                const highlight = keyboard.navigationHighlight;
                const item = highlight ? highlight.highlightItem : null;
                const ratio = item && item !== keyboard ? keyboard.mapFromItem(item, item.width / 2, 0).x / keyboard.width : 0;
                root.extraColumn = zone === root.zoneOf("suggestions") ? 0 : root.columnForZone(zone, ratio);
            } else {
                root.extraColumn = root.columnForZone(zone, (root.extraColumn + 0.5) / root.extraRowItemCount(root.extraRowFocus));
            }
            root.extraRowFocus = zone;
        }
        onBackspace: thing.sendKeyEvent(Qt.Key_Backspace, "")
        onSpace: thing.sendKeyEvent(Qt.Key_Space, " ")
        onEnter: thing.sendKeyEvent(Qt.Key_Return, "\n")
        onToggleShift: {
            // Shift has nothing to shift on the voice page.
            if (!root.voiceMode) {
                inputPanel.InputContext.priv.shiftHandler.toggleShift();
            }
        }
        onToggleSymbols: {
            // The symbols layer is hidden behind the voice page as well.
            if (!root.voiceMode) {
                inputPanel.keyboard.symbolMode = !inputPanel.keyboard.symbolMode;
            }
        }
        onSwitchLanguage: inputPanel.keyboard.changeInputLanguage(false)
        onBack: {
            // B dismisses the alternates list without picking anything; on the
            // voice page it goes back to the ordinary keyboard, and only there
            // does a second press close the keyboard.
            if (drivingAlternates) {
                thing.overlayController.navigateAlternates(Qt.Key_Escape);
                return;
            }
            if (root.voiceMode) {
                root.closeVoiceMode();
                return;
            }
            Qt.inputMethod.hide();
        }
        onHideKeyboard: {
            // Start hides the keyboard from anywhere, the voice page included.
            Qt.inputMethod.hide();
        }
    }

    // Play the key click at full volume: the bundled sound is mastered quiet.
    Component.onCompleted: {
        VirtualKeyboardSettings.keySoundVolume = 100;
        root.updatePredictions();

        // The window can already be active by the time the QML is loaded, in
        // which case onActiveChanged will not fire for it.
        if (active) {
            thing.forceActiveFocus();
        }

        // The navigation highlight of Qt Virtual Keyboard is animated, so while
        // the focus is moved into place it appears to travel through the keys it
        // passes. Without the animation it always shows the key that really has
        // the focus.
        if (inputPanel.keyboard.navigationHighlight) {
            inputPanel.keyboard.navigationHighlight.moveDuration = 0;
            inputPanel.keyboard.navigationHighlight.resizeDuration = 0;
        }
    }

    // Let the key panels know a gamepad is available, so they can show
    // the button glyphs directly on the relevant keys.
    Binding {
        target: PlasmaKeyboard.Modifiers
        property: "gamepadAvailable"
        value: gamepad.available
    }

    // While the gamepad is in the rows above the keyboard, the keyboard hides
    // its own navigation highlight (the item is kept, only the highlight goes).
    Binding {
        target: PlasmaKeyboard.Modifiers
        property: "extraRowsFocused"
        value: root.extraRowFocus !== 0
    }

    // Qt Virtual Keyboard's HideInputPanel only hides its internal panel; hide
    // our window as well so the keyboard actually disappears.
    Connections {
        target: inputPanel.InputContext.priv
        function onHideInputPanel() {
            Qt.inputMethod.hide();
        }
    }

    // Unified overlay system for diacritics, emoji, text expansion, etc.
    OverlayWindow {
        id: overlayWindow
        controller: thing.overlayController
        onCandidateSelected: (index) => thing.overlayController.commitCandidate(index)
    }

    // Only the panel is interactive while the keyboard is on screen; while it is
    // hidden the window keeps a one pixel region so that clicks reach the windows
    // below it.
    interactiveRegion: Qt.rect(panelWrapper.x, panelWrapper.y, panelWrapper.width, panelWrapper.height)

    Kirigami.ShadowedRectangle {
        id: panelWrapper

        LanguagePopup {
            id: languageDialog
            style: inputPanel.keyboard.style
            keyboardPanel: inputPanel

            onShowSettings: root.showSettings()
        }

        // Whether the panel takes the full width of the screen. The floating
        // panel is always detached from the edges, so it never fills the width.
        readonly property bool floating: PlasmaKeyboardSettings.floatingKeyboard
        readonly property bool isFullScreenWidth: PlasmaKeyboardSettings.panelFillScreenWidth && !floating

        //! Position of the floating panel, in window coordinates. A negative
        //! value means it was never moved: the panel then starts at the bottom
        //! centre, lifted off the edge by the panel padding.
        //!
        //! The vertical position is kept as the bottom edge of the panel: when a
        //! row of suggestions appears and the panel becomes taller, it grows
        //! upwards and the bottom edge stays where the user put it.
        property real floatingX: -1
        property real floatingBottom: -1

        //! Set once the panel has its final size: a later size change (a row of
        //! suggestions appearing, the panel switching between the docked and the
        //! floating width) must not move a panel the user has placed.
        property bool floatingPlaced: false

        readonly property real minX: padding
        readonly property real maxX: Math.max(minX, root.width - width - padding)

        //! Keeps the floating panel inside the screen.
        function clampFloatingX(value) {
            return Math.max(minX, Math.min(maxX, value));
        }
        function clampFloatingBottom(value) {
            return Math.max(padding + height, Math.min(root.height - padding, value));
        }

        //! Puts the floating panel where it belongs: at the position remembered
        //! from a previous run, or at the bottom centre. Only done once the panel
        //! really has its size: before that the panel keeps the placeholder size
        //! (100) it is given so that the input region is never empty, and a
        //! position computed from it would be wrong.
        function placeFloatingPanel() {
            if (floatingPlaced || inputPanel.width <= 0 || width <= inputPanel.width || height <= 0) {
                return;
            }
            floatingPlaced = true;
            if (PlasmaKeyboardSettings.floatingKeyboardX >= 0 && PlasmaKeyboardSettings.floatingKeyboardY >= 0) {
                floatingX = PlasmaKeyboardSettings.floatingKeyboardX * root.width;
                floatingBottom = PlasmaKeyboardSettings.floatingKeyboardY * root.height;
            } else {
                floatingX = (root.width - width) / 2;
                floatingBottom = root.height - padding;
            }
            floatingX = clampFloatingX(floatingX);
            floatingBottom = clampFloatingBottom(floatingBottom);
        }

        //! Remembers where the panel was left, as a fraction of the screen, so
        //! that it comes back to the same place after a restart and a different
        //! resolution does not move it off the screen.
        function saveFloatingPosition() {
            if (!floating || root.width <= 0 || root.height <= 0) {
                return;
            }
            PlasmaKeyboardSettings.floatingKeyboardX = floatingX / root.width;
            PlasmaKeyboardSettings.floatingKeyboardY = floatingBottom / root.height;
            PlasmaKeyboardSettings.save();
        }

        // KWin keeps driving the keyboard through the invisible input-panel
        // window and uses its input region as the keyboard rectangle (that is
        // what it reserves on screen and moves the focused window away from),
        // so the region has to follow the visible panel.
        function updatePanelStub() {
            PlasmaKeyboard.KeyboardWindow.setPanelRect(Qt.rect(x, y, width, height));
        }
        onXChanged: updatePanelStub()
        onYChanged: updatePanelStub()
        onWidthChanged: {
            placeFloatingPanel();
            updatePanelStub();
        }
        onHeightChanged: {
            placeFloatingPanel();
            updatePanelStub();
        }
        // The space reserved for the panel and the window anchors depend on the
        // mode: the compositor reserves it through the layer-shell exclusive zone.
        onFloatingChanged: {
            updatePanelStub();
            PlasmaKeyboard.KeyboardWindow.updatePanelLayout();
        }

        Component.onCompleted: updatePanelStub()

        // The floating panel can be made translucent in the settings; the docked
        // panel always stays opaque. The voice mode keeps that translucency and
        // hides the keys instead, so the panel does not turn opaque when the
        // voice mode is entered.
        readonly property real floatingOpacity: floating ? PlasmaKeyboardSettings.floatingKeyboardOpacity / 100 : 1
        opacity: floatingOpacity

        color: PlasmaKeyboard.Theme.current.backgroundType === "gradient" ? "transparent" : PlasmaKeyboard.BreezeConstants.keyboardBackgroundColor

        // Themed gradient background, used when the theme asks for one. The
        // per-corner radii follow the panel's rounded corners.
        Rectangle {
            anchors.fill: parent
            visible: PlasmaKeyboard.Theme.current.backgroundType === "gradient"
            topLeftRadius: panelWrapper.corners.topLeftRadius
            topRightRadius: panelWrapper.corners.topRightRadius
            bottomLeftRadius: panelWrapper.corners.bottomLeftRadius
            bottomRightRadius: panelWrapper.corners.bottomRightRadius
            gradient: Gradient {
                orientation: PlasmaKeyboard.BreezeConstants.backgroundOrientation
                GradientStop { position: 0.0; color: PlasmaKeyboard.Theme.current.backgroundStart }
                GradientStop { position: 1.0; color: PlasmaKeyboard.Theme.current.backgroundEnd }
            }
        }

        // Provide shadow and radius when the keyboard is detached from edges
        corners {
            // Only the floating panel is detached from every edge.
            bottomLeftRadius: Kirigami.Units.cornerRadius
            bottomRightRadius: Kirigami.Units.cornerRadius
            topLeftRadius: isFullScreenWidth ? 0 : Kirigami.Units.cornerRadius
            topRightRadius: isFullScreenWidth ? 0 : Kirigami.Units.cornerRadius
        }
        shadow {
            size: isFullScreenWidth ? 0 : 16
            color: Qt.rgba(0, 0, 0, 0.3)
        }

        // The docked panel is centred at the bottom; the floating one keeps the
        // position it was placed or dragged to. The horizontal position is kept
        // as it is and the vertical one is anchored to the bottom edge, so a
        // size change (a row of suggestions appearing, Shift changing the layout)
        // makes the panel grow upwards instead of moving it under the finger.
        x: floating ? (floatingX < 0 ? (root.width - width) / 2 : floatingX) : (root.width / 2) - (width / 2)
        y: floating ? (floatingBottom < 0 ? root.height - height - padding : floatingBottom - height) : root.height - height

        // A resolution change can leave the panel outside the screen.
        Connections {
            target: root
            function onWidthChanged() {
                if (panelWrapper.floating && panelWrapper.floatingX >= 0) {
                    panelWrapper.floatingX = panelWrapper.clampFloatingX(panelWrapper.floatingX);
                }
            }
            function onHeightChanged() {
                if (panelWrapper.floating && panelWrapper.floatingBottom >= 0) {
                    panelWrapper.floatingBottom = panelWrapper.clampFloatingBottom(panelWrapper.floatingBottom);
                }
            }
        }

        // Dragging the free background of the panel moves it. The keys are
        // handled by Qt Virtual Keyboard itself, so the handler only sees the
        // parts of the panel that are not a key.
        DragHandler {
            id: panelDrag

            target: null
            enabled: panelWrapper.floating

            //! How far the pointer has to move before the panel follows: a plain
            //! tap must not move (or remember) anything.
            readonly property real moveThreshold: 12

            property real startX: 0
            property real startBottom: 0
            property bool moved: false

            onActiveChanged: {
                if (active) {
                    startX = panelWrapper.x;
                    startBottom = panelWrapper.y + panelWrapper.height;
                    moved = false;
                } else if (moved) {
                    panelWrapper.saveFloatingPosition();
                }
            }
            onTranslationChanged: {
                if (!active) {
                    return;
                }
                if (!moved && Math.hypot(translation.x, translation.y) < moveThreshold) {
                    return;
                }
                moved = true;
                panelWrapper.floatingX = panelWrapper.clampFloatingX(startX + translation.x);
                panelWrapper.floatingBottom = panelWrapper.clampFloatingBottom(startBottom + translation.y);
            }
        }

        // Padding for background corners and panel drag area
        readonly property real padding: isFullScreenWidth ? 0 : Kirigami.Units.largeSpacing

        // The row above the keyboard: the words that continue what is being
        // typed while a word is being typed, the recent clipboard entries
        // otherwise. Both are a single row of chips, so they share the place on
        // the screen; the keys in the corner switch between them when both have
        // something to offer, and clear the clipboard history in that mode.
        Item {
            id: suggestionsRow

            readonly property var kbdStyle: inputPanel.keyboard.style
            // Three quarters of a normal keyboard row: the row is the F-key row
            // (half a row) grown by half of that height, so that the suggestions
            // are comfortable to read and to hit.
            readonly property real normalRowHeight: kbdStyle ? kbdStyle.targetKeyboardHeight / 5 : Kirigami.Units.gridUnit * 2
            readonly property real rowHeight: normalRowHeight * 0.75
            readonly property real fontScale: rowHeight / normalRowHeight
            readonly property real sideMargin: PlasmaKeyboard.BreezeConstants.keyBackgroundMargin / 2
            // How many entries fit on screen at once: all entries have the same
            // width, longer texts are cut off.
            readonly property int visibleChips: 3
            // The suggestions are all shown at once; the clipboard entries
            // scroll three at a time, so that a long history does not shrink
            // every entry to an unreadable width.
            readonly property int chipColumns: showingSuggestions ? Math.max(visibleChips, rowItems.length) : visibleChips
            readonly property real chipWidth: width / chipColumns

            //! Whether there is something to suggest for the word being typed.
            readonly property bool hasSuggestions: root.predictions.length > 0

            //! Whether there are clipboard entries to offer.
            readonly property bool hasClipboard: PlasmaKeyboardSettings.clipboardEnabled && thing.clipboardHistory.count > 0

            //! Set when the clipboard was asked for while suggestions are
            //! available; a new word gives way to the suggestions again.
            property bool clipboardChosen: false

            readonly property bool showingSuggestions: hasSuggestions && !clipboardChosen

            //! The chips of the row: the suggestions (with the part that would
            //! be added to the word) or the clipboard entries.
            readonly property var rowItems: {
                if (showingSuggestions) {
                    // What the engine offered is taken as it is: only a
                    // completion has a part to add, a correction and a
                    // prediction replace or insert the whole word.
                    return root.predictions.map(item => ({
                                "text": item.text,
                                "completion": item.completion,
                                "suggestion": true
                            }));
                }
                const items = [];
                for (let i = 0; i < thing.clipboardHistory.count; ++i) {
                    items.push({
                        "text": thing.clipboardHistory.textAt(i),
                        "completion": 0,
                        "suggestion": false
                    });
                }
                return items;
            }

            //! The key in the corner that switches to the other mode, if any.
            readonly property bool hasSwitchButton: showingSuggestions ? hasClipboard : hasSuggestions

            //! The key that clears the clipboard history (clipboard mode only).
            readonly property bool hasClearButton: !showingSuggestions

            //! Number of selectable cells of the row, for the gamepad.
            function itemCount() {
                return rowItems.length + (hasSwitchButton ? 1 : 0) + (hasClearButton ? 1 : 0);
            }

            //! Takes the cell the gamepad selected.
            function activate(column) {
                if (column < rowItems.length) {
                    if (showingSuggestions) {
                        thing.applyPrediction(rowItems[column].text);
                    } else {
                        thing.commitText(rowItems[column].text);
                    }
                    return;
                }
                let index = column - rowItems.length;
                if (hasSwitchButton) {
                    if (index === 0) {
                        clipboardChosen = !clipboardChosen;
                        return;
                    }
                    --index;
                }
                if (hasClearButton) {
                    thing.clipboardHistory.clear();
                }
            }

            visible: showingSuggestions || hasClipboard
            onVisibleChanged: {
                if (!visible && root.extraRowFocus === root.zoneOf("suggestions")) {
                    root.extraRowFocus = 0;
                }
            }

            // A new word means new suggestions, so the clipboard, if it was
            // asked for, gives way to them again.
            Connections {
                target: thing
                function onPredictionPrefixChanged() {
                    suggestionsRow.clipboardChosen = false;
                }
            }

            anchors {
                top: parent.top
                topMargin: parent.padding
                horizontalCenter: parent.horizontalCenter
            }

            width: inputPanel.width > 0 ? inputPanel.width - sideMargin * 2 : 100
            height: visible ? rowHeight : 0

            //! Scrolls the entry selected with the gamepad into view.
            function showColumn(column) {
                const target = column * chipWidth + chipWidth / 2 - entries.width / 2;
                entries.contentX = Math.max(0, Math.min(entries.contentWidth - entries.width, target));
            }

            Connections {
                target: root
                function onExtraColumnChanged() {
                    if (root.extraRowFocus === root.zoneOf("suggestions")) {
                        suggestionsRow.showColumn(root.extraColumn);
                    }
                }
                function onExtraRowFocusChanged() {
                    if (root.extraRowFocus === root.zoneOf("suggestions")) {
                        suggestionsRow.showColumn(root.extraColumn);
                    }
                }
            }

            // The chips, flicked from the left edge. The keys in the corner are
            // not part of this area, so they never scroll away.
            Flickable {
                id: entries

                anchors.left: parent.left
                anchors.top: parent.top
                width: parent.width - cornerKeys.width
                height: parent.height

                contentWidth: chips.width
                contentHeight: height
                // The gaps between the chips are made by the key margins of the
                // cells, exactly like between the keyboard keys.
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                Row {
                    id: chips
                    height: entries.height
                    // The suggestions start at the left edge, the way the word
                    // suggestions sit on the keyboards that have them. The
                    // clipboard entries are centered while they fit: fewer
                    // entries than the row holds should not hug the edge.
                    x: suggestionsRow.showingSuggestions ? 0 : Math.max(0, (entries.width - width) / 2)

                    Repeater {
                        model: suggestionsRow.rowItems

                        delegate: Item {
                            id: chip
                            required property int index
                            required property var modelData

                            //! Selected with the gamepad.
                            readonly property bool focused: root.extraRowFocus === root.zoneOf("suggestions") && root.extraColumn === index

                            //! Whether the part of the suggestion that would be
                            //! added to the word being typed is shown.
                            readonly property bool completes: modelData.suggestion && modelData.completion > 0

                            //! A suggestion is as wide as its text, so that a
                            //! short word does not take a third of the screen; the
                            //! clipboard entries keep one width, three to a row.
                            //! The text keeps equal room on both sides of the chip,
                            //! also when the chip is wider than its text.
                            readonly property real chipPadding: PlasmaKeyboard.BreezeConstants.keyBackgroundMargin * 2
                            readonly property real chipTextWidth: chipLabel.contentWidth + 2 * chipPadding
                            width: modelData.suggestion ? Math.min(suggestionsRow.width, Math.max(suggestionsRow.rowHeight * 2, chipTextWidth)) : suggestionsRow.chipWidth
                            height: suggestionsRow.rowHeight

                            Kirigami.ShadowedRectangle {
                                id: chipBackground
                                // Same margins as a keyboard key, so the row looks like
                                // part of the keyboard.
                                anchors.fill: parent
                                anchors.margins: PlasmaKeyboard.BreezeConstants.keyBackgroundMargin
                                radius: PlasmaKeyboard.BreezeConstants.buttonRadius

                                readonly property var outline: PlasmaKeyboard.Theme.current.keyOutlineFor("suggestions")
                                readonly property real shadowStrength: PlasmaKeyboard.Theme.current.keyShadowFor("suggestions")

                                color: PlasmaKeyboard.Theme.current.keyColorFor("suggestions", chipHandler.pressed ? "pressed" : "normal")

                                border.width: outline.width
                                border.color: outline.color
                                shadow.size: 3 * shadowStrength
                                shadow.yOffset: 1 * shadowStrength
                                shadow.color: Qt.rgba(0, 0, 0, 0.2 * shadowStrength)

                                // Selected with the gamepad: highlighted the way
                                // Qt Virtual Keyboard highlights its own keys.
                                Rectangle {
                                    anchors.fill: parent
                                    radius: PlasmaKeyboard.BreezeConstants.buttonRadius
                                    visible: chip.focused
                                    color: PlasmaKeyboard.BreezeConstants.navigationHighlightColor
                                    border.width: 2
                                    border.color: PlasmaKeyboard.BreezeConstants.navigationHighlightBorderColor
                                }

                                Text {
                                    id: chipLabel
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.verticalCenter: parent.verticalCenter
                                    // The suggestion takes the width of its text
                                    // (the chip is measured from it); a clipboard
                                    // entry takes the width of the chip and is cut
                                    // off when it is longer.
                                    width: chip.modelData.suggestion ? implicitWidth : Math.max(0, chip.width - chip.chipPadding * 2)
                                    // The completed part of a suggestion is
                                    // underlined, the way Qt Virtual Keyboard
                                    // underlines it in its own candidate bar. An
                                    // entry can be multi-line: show it as one line.
                                    text: chip.completes ? chip.modelData.text.slice(0, -chip.modelData.completion) + "<u>" + chip.modelData.text.slice(-chip.modelData.completion) + "</u>" : chip.modelData.text.replace(/\s+/g, " ")
                                    textFormat: chip.completes ? Text.RichText : Text.PlainText
                                    elide: chip.completes ? Text.ElideNone : Text.ElideRight
                                    maximumLineCount: 1
                                    color: PlasmaKeyboard.BreezeConstants.keyTextColor
                                    font {
                                        family: PlasmaKeyboard.BreezeConstants.fontFamily
                                        pixelSize: suggestionsRow.kbdStyle ? 60 * (suggestionsRow.kbdStyle.targetKeyboardHeight / suggestionsRow.kbdStyle.keyboardDesignHeight) * suggestionsRow.fontScale : Kirigami.Units.gridUnit
                                    }
                                }
                            }

                            MouseArea {
                                id: chipHandler
                                anchors.fill: parent
                                onClicked: suggestionsRow.activate(chip.index)
                            }
                        }
                    }
                }
            }

            // The keys in the right corner: the one that switches between the
            // suggestions and the clipboard entries, and the one that clears
            // the history. Both are regular keys (square, key background) next
            // to the lighter entries.
            Row {
                id: cornerKeys

                //! Column of the switching key in the row, -1 when it is hidden.
                readonly property int switchColumn: suggestionsRow.hasSwitchButton ? suggestionsRow.rowItems.length : -1

                //! Column of the key that clears the history, -1 when it is hidden.
                readonly property int clearColumn: suggestionsRow.hasClearButton ? suggestionsRow.rowItems.length + (suggestionsRow.hasSwitchButton ? 1 : 0) : -1

                anchors.right: parent.right
                anchors.top: parent.top
                height: suggestionsRow.rowHeight

                // Switching to the clipboard (and back to the suggestions).
                Item {
                    id: switchButton

                    visible: suggestionsRow.hasSwitchButton
                    width: suggestionsRow.rowHeight
                    height: suggestionsRow.rowHeight

                    readonly property bool focused: root.extraRowFocus === root.zoneOf("suggestions") && root.extraColumn === cornerKeys.switchColumn

                    Kirigami.ShadowedRectangle {
                        anchors.fill: parent
                        anchors.margins: PlasmaKeyboard.BreezeConstants.keyBackgroundMargin
                        radius: PlasmaKeyboard.BreezeConstants.buttonRadius

                        readonly property var outline: PlasmaKeyboard.Theme.current.keyOutlineFor("normal")
                        readonly property real shadowStrength: PlasmaKeyboard.Theme.current.keyShadowFor("normal")

                        color: PlasmaKeyboard.Theme.current.keyColorFor("normal", switchHandler.pressed ? "pressed" : "normal")

                        border.width: outline.width
                        border.color: outline.color
                        shadow.size: 3 * shadowStrength
                        shadow.yOffset: 1 * shadowStrength
                        shadow.color: Qt.rgba(0, 0, 0, 0.2 * shadowStrength)

                        Rectangle {
                            anchors.fill: parent
                            radius: PlasmaKeyboard.BreezeConstants.buttonRadius
                            visible: switchButton.focused
                            color: PlasmaKeyboard.BreezeConstants.navigationHighlightColor
                            border.width: 2
                            border.color: PlasmaKeyboard.BreezeConstants.navigationHighlightBorderColor
                        }

                        Kirigami.Icon {
                            anchors.centerIn: parent
                            width: Math.round(suggestionsRow.rowHeight * 0.6)
                            height: width
                            // The clipboard while the suggestions are shown, the
                            // suggestions while the clipboard is shown. The
                            // symbolic variants are the ones Kirigami.Icon can
                            // recolour with keyTextColor; the plain names are
                            // taken from the icon theme as they are and vanish on
                            // a light background.
                            source: PlasmaKeyboard.BreezeConstants.icon(suggestionsRow.showingSuggestions ? "edit-paste-symbolic" : "tools-check-spelling-symbolic")
                            color: PlasmaKeyboard.BreezeConstants.keyTextColor
                        }
                    }

                    MouseArea {
                        id: switchHandler
                        anchors.fill: parent
                        onClicked: suggestionsRow.clipboardChosen = !suggestionsRow.clipboardChosen
                    }
                }

                // Clearing the whole clipboard history.
                Item {
                    id: clearButton

                    visible: suggestionsRow.hasClearButton
                    width: suggestionsRow.rowHeight
                    height: suggestionsRow.rowHeight

                    //! Selected with the gamepad (the last column of the row).
                    readonly property bool focused: root.extraRowFocus === root.zoneOf("suggestions") && root.extraColumn === cornerKeys.clearColumn

                    Kirigami.ShadowedRectangle {
                        anchors.fill: parent
                        anchors.margins: PlasmaKeyboard.BreezeConstants.keyBackgroundMargin
                        radius: PlasmaKeyboard.BreezeConstants.buttonRadius

                        readonly property var outline: PlasmaKeyboard.Theme.current.keyOutlineFor("normal")
                        readonly property real shadowStrength: PlasmaKeyboard.Theme.current.keyShadowFor("normal")

                        color: PlasmaKeyboard.Theme.current.keyColorFor("normal", clearHandler.pressed ? "pressed" : "normal")

                        border.width: outline.width
                        border.color: outline.color
                        shadow.size: 3 * shadowStrength
                        shadow.yOffset: 1 * shadowStrength
                        shadow.color: Qt.rgba(0, 0, 0, 0.2 * shadowStrength)

                        Rectangle {
                            anchors.fill: parent
                            radius: PlasmaKeyboard.BreezeConstants.buttonRadius
                            visible: clearButton.focused
                            color: PlasmaKeyboard.BreezeConstants.navigationHighlightColor
                            border.width: 2
                            border.color: PlasmaKeyboard.BreezeConstants.navigationHighlightBorderColor
                        }

                        Kirigami.Icon {
                            anchors.centerIn: parent
                            width: Math.round(suggestionsRow.rowHeight * 0.6)
                            height: width
                            source: PlasmaKeyboard.BreezeConstants.icon("edit-clear-all-symbolic")
                            color: PlasmaKeyboard.BreezeConstants.keyTextColor
                        }
                    }

                    MouseArea {
                        id: clearHandler
                        anchors.fill: parent
                        onClicked: thing.clipboardHistory.clear()
                    }
                }
            }
        }

        // Optional F1-F12 row above the keyboard. The panel grows by the row
        // height when it is shown, the keyboard itself keeps its size.
        Row {
            id: functionKeyRow
            visible: PlasmaKeyboardSettings.showFunctionKeyRow
            // Match the keyboard geometry: five rows fill the keyboard height,
            // and the visible key background is inset by keyBackgroundMargin.
            readonly property var kbdStyle: inputPanel.keyboard.style
            // Half the height of a normal keyboard row.
            readonly property real normalRowHeight: kbdStyle ? kbdStyle.targetKeyboardHeight / 5 : Kirigami.Units.gridUnit * 2
            readonly property real keyHeight: normalRowHeight / 2
            // Key labels scale with the row height.
            readonly property real fontScale: keyHeight / normalRowHeight
            // Align the row with the keyboard keys below it: the keyboard
            // itself keeps a cell margin of about half a key margin.
            readonly property real sideMargin: PlasmaKeyboard.BreezeConstants.keyBackgroundMargin / 2
            anchors {
                top: suggestionsRow.visible ? suggestionsRow.bottom : parent.top
                topMargin: parent.padding
                horizontalCenter: parent.horizontalCenter
            }
            width: inputPanel.width > 0 ? inputPanel.width - sideMargin * 2 : 100
            height: visible ? keyHeight : 0

            Repeater {
                model: 12
                delegate: Item {
                    required property int index
                    width: functionKeyRow.width / 12
                    height: functionKeyRow.keyHeight

                    Kirigami.ShadowedRectangle {
                        anchors.fill: parent
                        anchors.margins: PlasmaKeyboard.BreezeConstants.keyBackgroundMargin
                        radius: PlasmaKeyboard.BreezeConstants.buttonRadius
                        readonly property bool focused: root.extraRowFocus === root.zoneOf("fkeys") && root.extraColumn === index
                        readonly property var outline: PlasmaKeyboard.Theme.current.keyOutlineFor("function")
                        readonly property real shadowStrength: PlasmaKeyboard.Theme.current.keyShadowFor("function")

                        color: PlasmaKeyboard.Theme.current.hasCategoryColors("function")
                            ? PlasmaKeyboard.Theme.current.keyColorFor("function", pressHandler.pressed ? "pressed" : "normal")
                            : (pressHandler.pressed ? PlasmaKeyboard.BreezeConstants.primaryDarkColor : PlasmaKeyboard.BreezeConstants.normalKeyBackgroundColor)

                        border.width: outline.width
                        border.color: outline.color
                        shadow.size: 3 * shadowStrength
                        shadow.yOffset: 1 * shadowStrength
                        shadow.color: Qt.rgba(0, 0, 0, 0.2 * shadowStrength)

                        Rectangle {
                            anchors.fill: parent
                            radius: PlasmaKeyboard.BreezeConstants.buttonRadius
                            visible: parent.focused
                            color: PlasmaKeyboard.BreezeConstants.navigationHighlightColor
                            border.width: 2
                            border.color: PlasmaKeyboard.BreezeConstants.navigationHighlightBorderColor
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "F" + (index + 1)
                            color: PlasmaKeyboard.Theme.current.keyTextColorFor("function")
                            font {
                                family: PlasmaKeyboard.BreezeConstants.fontFamily
                                weight: Font.Bold
                                // Keyboard label size, scaled with the row height.
                                pixelSize: functionKeyRow.kbdStyle ? 60 * (functionKeyRow.kbdStyle.targetKeyboardHeight / functionKeyRow.kbdStyle.keyboardDesignHeight) * functionKeyRow.fontScale : Kirigami.Units.gridUnit
                            }
                        }
                    }

                    MouseArea {
                        id: pressHandler
                        anchors.fill: parent
                        onClicked: thing.sendKeyEvent(Qt.Key_F1 + index, "")
                    }
                }
            }
        }

        // Never let width & height to be 0, otherwise it can cause problems for setting interactiveRegion
        width: inputPanel.width > 0 ? (inputPanel.width + padding * 2) : 100
        height: inputPanel.height > 0
            ? (inputPanel.height + padding * 2 + (functionKeyRow.visible ? functionKeyRow.height : 0) + (suggestionsRow.visible ? suggestionsRow.height : 0))
            : 100

        InputPanel {
            id: inputPanel
            anchors {
                top: functionKeyRow.visible ? functionKeyRow.bottom : (suggestionsRow.visible ? suggestionsRow.bottom : parent.top)
                // Keep the vertical rhythm of the keyboard rows when the F-key
                // row is shown.
                topMargin: functionKeyRow.visible ? -functionKeyRow.sideMargin : parent.padding
                left: parent.left
                leftMargin: parent.padding
            }

            // height is calculated by InputPanel
            // The floating keyboard is a compact panel whose width is a share of
            // the screen width (configurable). The limit lives here rather than in
            // the style so that it also holds for a style that does not know
            // about the floating mode.
            // The voice mode hides the keys this way rather than with visible:
            // the panel keeps its size, and a translucent floating panel keeps
            // its translucency.
            opacity: root.voiceMode ? 0 : 1
            enabled: !root.voiceMode
            width: {
                if (!inputPanel.keyboard.style) {
                    return 0;
                }
                const full = inputPanel.keyboard.style.aspectRatio * inputPanel.keyboard.style.targetKeyboardHeight;
                if (!PlasmaKeyboardSettings.floatingKeyboard) {
                    return full;
                }
                const share = root.width * PlasmaKeyboardSettings.floatingKeyboardWidthPercent / 100;
                return Math.min(full, share - panelWrapper.padding * 2);
            }

            focusPolicy: Qt.NoFocus
            externalLanguageSwitchEnabled: true
            onExternalLanguageSwitch: (localeList, currentIndex) => {
                languageDialog.show(inputPanel.keyboard.activeKey, localeList, currentIndex)
            }

            function updateLocales() {
                let locales = PlasmaKeyboardSettings.enabledLocales;
                if (locales.length === 0) {
                    // If there are no enabled locales, set it to the current locale
                    // NOTE: If Qt.locale().name is not valid, then all keyboard layouts will be shown.
                    let locale = Qt.locale().name;
                    if (locale === "C") {
                        locale = "en_US";
                    }
                    locales = [locale];
                }
                VirtualKeyboardSettings.activeLocales = locales;

                // Qt checks VirtualKeyboardSettings.locale before the locale the
                // system (or the focused application) asks for and before
                // activeLocales[0], so setting it here is what makes the chosen
                // layout open by default. A locale that is not in activeLocales
                // is ignored by Qt, so clear the choice when it is not available.
                const defaultLocale = PlasmaKeyboardSettings.defaultLocale;
                VirtualKeyboardSettings.locale = locales.includes(defaultLocale) ? defaultLocale : "";
            }

            Connections {
                target: VirtualKeyboardSettings
                function onAvailableLocalesChanged() {
                    inputPanel.updateLocales();
                }
            }

            Connections {
                target: PlasmaKeyboardSettings
                function onEnabledLocalesChanged() {
                    inputPanel.updateLocales();
                }
                function onDefaultLocaleChanged() {
                    inputPanel.updateLocales();
                }
                function onThemeChanged() {
                    PlasmaKeyboard.Theme.setThemeId(PlasmaKeyboardSettings.theme);
                }
            }

            Component.onCompleted: {
                VirtualKeyboardSettings.styleName = "PlasmaBreezeCustom";
                PlasmaKeyboard.Theme.setThemeId(PlasmaKeyboardSettings.theme);
                // Enable Qt Virtual Keyboard's arrow-key navigation so the
                // gamepad can move the highlight and activate keys.
                VirtualKeyboardSettings.arrowKeyNavigationEnabled = true;
                inputPanel.updateLocales();
            }
        }

        // Voice input mode: covers the keys with one big microphone button. It
        // keeps the size of the keyboard, so the panel does not move when the
        // mode is entered or left.
        VoicePanel {
            id: voicePanel
            anchors.fill: inputPanel
            z: 100
            visible: root.voiceMode
            recording: PlasmaKeyboard.Stt.recording
            busy: PlasmaKeyboard.Stt.busy
            level: PlasmaKeyboard.Stt.level
            message: PlasmaKeyboard.Stt.lastError
            highlighted: gamepad.available && root.voiceNavigationActive
            // The space bar of the voice mode shows the active layout, the same
            // way the space bar of the ordinary keyboard does.
            languageName: {
                const name = Qt.locale(inputPanel.InputContext.locale).nativeLanguageName;
                return name.length > 0 ? name.charAt(0).toUpperCase() + name.slice(1) : name;
            }

            onToggleRequested: {
                if (PlasmaKeyboard.Stt.recording) {
                    PlasmaKeyboard.Stt.stopRecording();
                } else {
                    PlasmaKeyboard.Stt.startRecording(inputPanel.InputContext.locale);
                }
            }

            onCloseRequested: root.closeVoiceMode()
            onHideRequested: Qt.inputMethod.hide()

            // The keys kept under the microphone while dictating: the layout
            // switch, the space bar, enter and backspace.
            onLanguageRequested: inputPanel.keyboard.changeInputLanguage(false)
            onSpaceRequested: thing.commitText(" ")
            onEnterRequested: thing.sendKeyEvent(Qt.Key_Return, "\n")
            onBackspaceRequested: thing.sendKeyEvent(Qt.Key_Backspace, "")
        }

        // The alternate characters the gamepad offers are drawn here, inside
        // the keyboard window. A window of its own would take the input focus
        // away from the field being typed into, and the character picked from
        // the list would then have nowhere to be inserted: the input engine
        // sends it as a key click to the focused window.
        AlternatesList {
            id: alternatesList

            property var cachedOptions: []

            function reload() {
                const model = thing.overlayController.candidateModel;
                const items = [];
                for (let i = 0; i < model.rowCount(); i++) {
                    items.push(model.insertTextAt(i));
                }
                cachedOptions = items;
            }

            Connections {
                target: thing.overlayController.candidateModel
                function onModelReset() {
                    alternatesList.reload();
                }
            }

            // The list is gone, so A goes back to typing the highlighted key.
            Connections {
                target: thing.overlayController
                function onOverlayVisibleChanged() {
                    if (!thing.overlayController.overlayVisible) {
                        gamepad.clearAlternatesOpen();
                    }
                }
            }

            Component.onCompleted: reload()

            visible: thing.overlayController.overlayVisible && thing.overlayController.alternatesOnly
            style: inputPanel.keyboard.style
            options: cachedOptions
            externalSelectedIndex: thing.overlayController.alternateSelection

            onCharacterSelected: index => thing.overlayController.commitCandidate(index)
        }
    }

    // Steam-like button prompt on the key that is currently focused.
    Item {
        id: gamepadKeyPrompt
        readonly property Item activeKey: inputPanel.keyboard ? inputPanel.keyboard.activeKey : null
        visible: gamepad.available && inputPanel.keyboard.navigationModeActive && activeKey !== null
        width: 24
        height: 24
        x: activeKey ? activeKey.mapToItem(null, activeKey.width - width - 3, activeKey.height - height - 3).x : 0
        y: activeKey ? activeKey.mapToItem(null, activeKey.width - width - 3, activeKey.height - height - 3).y : 0

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "#2e7d32"
            border.color: "white"
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "A"
                color: "white"
                font.bold: true
                font.pixelSize: 15
            }
        }
    }
}
