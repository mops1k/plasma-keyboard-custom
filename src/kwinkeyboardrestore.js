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

var saved = [];
var keyboardWasVisible = false;
var cooldownUntil = 0;

function keyboardVisible() {
    var windows = workspace.windowList();
    for (var i = 0; i < windows.length; ++i) {
        if (windows[i].inputMethod && !windows[i].hidden) {
            return true;
        }
    }
    return false;
}

function restorable(w) {
    return w && w.windowType === 0 && w.maximizeMode === 0 && !w.fullScreen && !w.inputMethod;
}

function rememberGeometry() {
    var snapshot = [];
    workspace.windowList().forEach(function (w) {
        if (!restorable(w)) {
            return;
        }
        var geometry = w.frameGeometry;
        snapshot.push({
            window: w,
            geometry: {
                x: Math.round(geometry.x),
                y: Math.round(geometry.y),
                width: Math.round(geometry.width),
                height: Math.round(geometry.height)
            }
        });
    });
    saved = snapshot;
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

function restoreGeometry() {
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
    saved = [];
}

var poll = new QTimer();
poll.interval = 200;
poll.timeout.connect(function () {
    var visible = keyboardVisible();
    if (keyboardWasVisible && !visible) {
        restoreGeometry();
        // Let the windows settle before the snapshot is taken again: right after
        // the restore they still carry the size the keyboard left them with.
        cooldownUntil = Date.now() + 900;
    }
    keyboardWasVisible = visible;
    if (!visible && Date.now() > cooldownUntil) {
        rememberGeometry();
    }
});
poll.start();
rememberGeometry();
