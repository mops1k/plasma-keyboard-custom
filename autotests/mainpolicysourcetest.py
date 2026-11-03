#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
"""Source-level guards for the layer-shell policy in src/main.cpp."""
import pathlib
import re
import sys

source = (pathlib.Path(__file__).resolve().parent.parent / "src" / "main.cpp").read_text()

# The output follows the compositor's active output; pinning an explicit Qt
# screen on the layer role would keep the keyboard on the wrong display.
assert not re.search(r"m_layerShell->setScreen\(", source), "main.cpp must not pin the layer-shell screen"

# The reserved space may only be given up for a *known* locked session.
zone = re.search(r"setExclusiveZone\(([^;]*)\);", source[source.index("panelHeight"):])
assert zone, "docked exclusive zone not found"
assert "isKnown()" in source[source.index("const bool sessionLocked"):source.index("const bool sessionLocked") + 200], \
    "exclusive zone must not drop to 0 while the session state is unknown"
assert zone.group(1).strip() == "sessionLocked ? 0 : panelHeight", zone.group(1)
print("ok")
sys.exit(0)
