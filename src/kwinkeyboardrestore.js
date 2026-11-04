// SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
//
// Workaround for the KWin bug where windows shrunk by the virtual keyboard do
// not get their size back when the keyboard goes away (KDE bug 459136): the
// configure event for the shrunken size is still on its way to the window, and
// applying it after KWin has already asked the window to grow back leaves the
// window short.
//
// The script keeps a snapshot of the geometry of the ordinary windows while the
// keyboard is hidden and, once the keyboard is gone, gives that geometry back to
// the windows the keyboard shortened. The size is applied twice (a little larger
// than needed, then exact), so that a configure event carrying the right size
// ends up in the queue.
//
// Maximized and full screen windows are left alone: KWin brings them back itself
// when it recomputes the work area.
//
// The keyboard is followed by its own window, not by what the compositor reports
// for the panel: with hideOnInputFocusLoss off the window stays on screen after
// the text input goes away, while KWin hides the panel and drops the space it
// reserved. The windows are kept short then, and are given their geometry back
// only once the keyboard window is really gone.

var fullSaved = [];
var compactSaved = [];
var wasOnScreen = false;
var cooldownUntil = 0;

//! The compositor hides the panel as soon as the text input goes away, so its
//! visibility only tells that the keyboard was asked for, not that it is on
//! screen.
function stubVisible() {
    var windows = workspace.windowList();
    for (var i = 0; i < windows.length; ++i) {
        if (windows[i].inputMethod && !windows[i].hidden) {
            return true;
        }
    }
    return false;
}

//! The layer-shell window the keyboard draws itself in: an ordinary window in
//! the overlay layer of the compositor, owned by the input method process. It
//! stays mapped for as long as the keyboard is on screen.
function keyboardWindow() {
    var windows = workspace.windowList();
    for (var i = 0; i < windows.length; ++i) {
        var w = windows[i];
        if (w.layer === 9 && w.windowType === 0 && !w.inputMethod && !w.hidden && w.resourceClass === "kwin_wayland") {
            return w;
        }
    }
    return null;
}

function keyboardOnScreen() {
    return stubVisible() || keyboardWindow() !== null;
}

function restorable(w) {
    return w && w.windowType === 0 && w.maximizeMode === 0 && !w.fullScreen && !w.inputMethod;
}

function snapshot() {
    var list = [];
    workspace.windowList().forEach(function (w) {
        if (!restorable(w)) {
            return;
        }
        var geometry = w.frameGeometry;
        list.push({
            window: w,
            geometry: {
                x: Math.round(geometry.x),
                y: Math.round(geometry.y),
                width: Math.round(geometry.width),
                height: Math.round(geometry.height)
            }
        });
    });
    return list;
}

function applyGeometry(w, geometry) {
    w.frameGeometry = { x: geometry.x, y: geometry.y, width: geometry.width, height: geometry.height };
}

function applyGeometryTwice(w, geometry) {
    w.frameGeometry = { x: geometry.x, y: geometry.y, width: geometry.width, height: geometry.height + 4 };
    var settle = new QTimer();
    settle.singleShot = true;
    settle.interval = 80;
    settle.timeout.connect(function () {
        applyGeometry(w, geometry);
    });
    settle.start();
}

//! Gives the saved geometry back to the windows the keyboard shortened. The
//! size is only applied to a window that is still short, so a window the user
//! resized in the meantime is left alone.
function restoreGeometry(saved) {
    var restored = 0;
    saved.forEach(function (entry) {
        try {
            if (!restorable(entry.window)) {
                return;
            }
            var current = entry.window.frameGeometry;
            if (Math.round(current.height) >= entry.geometry.height) {
                return;
            }
            applyGeometryTwice(entry.window, entry.geometry);
            ++restored;
        } catch (error) {
            print("window geometry restore: " + error);
        }
    });
    if (restored > 0) {
        print("window geometry restore: restored " + restored + " of " + saved.length);
    }
}

//! The text input went away, so the compositor gave the windows it had shortened
//! their full size back, while the keyboard window is still on screen: shorten
//! them again.
function holdGeometry() {
    var held = 0;
    compactSaved.forEach(function (entry) {
        try {
            if (!restorable(entry.window)) {
                return;
            }
            var current = entry.window.frameGeometry;
            if (Math.round(current.height) <= entry.geometry.height) {
                return;
            }
            applyGeometry(entry.window, entry.geometry);
            ++held;
        } catch (error) {
            print("window geometry hold: " + error);
        }
    });
    if (held > 0) {
        print("window geometry hold: kept " + held + " window(s) short");
    }
}

var poll = new QTimer();
poll.interval = 200;
poll.timeout.connect(function () {
    var onScreen = keyboardOnScreen();

    if (onScreen && !stubVisible()) {
        // The keyboard is still on screen, but the compositor already dropped the
        // space it reserved for it: keep the windows short.
        holdGeometry();
    } else if (onScreen) {
        // The keyboard is on screen and the compositor knows it: remember the
        // geometry the windows have with the keyboard up.
        compactSaved = snapshot();
    } else {
        if (wasOnScreen) {
            restoreGeometry(fullSaved);
            // Let the windows settle before the snapshot is taken again: right
            // after the restore they still carry the size the keyboard left them
            // with.
            cooldownUntil = Date.now() + 900;
            fullSaved = [];
            compactSaved = [];
        }
        if (Date.now() > cooldownUntil) {
            fullSaved = snapshot();
        }
    }

    wasOnScreen = onScreen;
});
poll.start();

if (!keyboardOnScreen()) {
    fullSaved = snapshot();
}
