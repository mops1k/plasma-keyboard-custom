#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
"""Prepare, never install, a surgical patch for the existing TOUCHFIX theme.

Refuse unknown layouts rather than modifying authentication/theme code by guess.
Install LocalTouchFocus.qml alongside the prepared LockScreenUi.qml only after a
coordinated test/backup; this utility does not change mounts or desktop settings.
"""
import argparse
from pathlib import Path

REPLACEMENTS = (
    (
        "    Kirigami.Theme.inherit: false",
        "    // LOCAL_TOUCH_FOCUS: only a press in this view may activate it.\n"
        "    LocalTouchFocus {\n"
        "        screenRoot: lockScreenRoot\n"
        "        targetItem: mainBlock.mainPasswordBox\n"
        "        viewWindow: lockScreenUi.Window.window\n"
        "    }\n\n"
        "    Kirigami.Theme.inherit: false",
    ),
    (
        "        onUiVisibleChanged: {\n"
        "            if (uiVisible) {\n"
        "                Window.window.requestActivate();\n"
        "            }\n",
        "        onUiVisibleChanged: {\n",
    ),
    (
        "        onBlockUIChanged: {\n"
        "            if (blockUI) {\n"
        "                fadeoutTimer.running = false;\n"
        "                uiVisible = true;\n",
        "        onBlockUIChanged: {\n"
        "            if (blockUI) {\n"
        "                fadeoutTimer.running = false;\n"
        "                // Global keyboard/touch state must not reveal another view.\n"
        "                if (Window.window.active) {\n"
        "                    uiVisible = true;\n"
        "                }\n",
    ),
)


def patched(source: str) -> str:
    if "LOCAL_TOUCH_FOCUS" in source:
        raise ValueError("Already patched; refusing to duplicate the handler")
    # Validate every original fragment before changing any of them.
    for old, _ in REPLACEMENTS:
        if source.count(old) != 1:
            raise ValueError("Theme differs from reviewed TOUCHFIX layout; no output written")
    for old, new in REPLACEMENTS:
        source = source.replace(old, new, 1)
    return source


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.source.resolve() == args.output.resolve():
        parser.error("In-place changes are deliberately forbidden")
    try:
        result = patched(args.source.read_text(encoding="utf-8"))
        with args.output.open("x", encoding="utf-8") as target:
            target.write(result)
    except (ValueError, OSError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
