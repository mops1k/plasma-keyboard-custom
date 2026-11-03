#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
SCRIPT = Path(__file__).resolve().parents[1] / "tools/lockscreen/prepare-focus-patch.py"
spec = importlib.util.spec_from_file_location("focuspatch", SCRIPT)
focuspatch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(focuspatch)


class FocusPatchTest(unittest.TestCase):
    def source(self):
        # Exact reviewed fragments; surrounding authentication fixes survive.
        return "// TOUCHFIX keep me\n" + "\n".join(old for old, _ in focuspatch.REPLACEMENTS) + "\nauthenticator.startAuthenticating();\n"

    def test_surgical(self):
        patched = focuspatch.patched(self.source())
        self.assertIn("LOCAL_TOUCH_FOCUS", patched)
        self.assertIn("LocalTouchFocus {", patched)
        self.assertIn("if (Window.window.active)", patched)
        self.assertNotIn("Window.window.requestActivate();", patched)
        self.assertIn("// TOUCHFIX keep me", patched)
        self.assertEqual(patched.count("authenticator.startAuthenticating();"), 1)
        with self.assertRaises(ValueError):
            focuspatch.patched(patched)

    def test_refuses_different_or_ambiguous_layout(self):
        with self.assertRaises(ValueError):
            focuspatch.patched("not the reviewed layout")
        with self.assertRaises(ValueError):
            focuspatch.patched(self.source() + focuspatch.REPLACEMENTS[0][0])

    def test_never_overwrites_source_or_output(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "source.qml"
            output = Path(directory) / "prepared.qml"
            source.write_text(self.source(), encoding="utf-8")
            original = source.read_bytes()
            first = subprocess.run([sys.executable, str(SCRIPT), str(source), str(output)], capture_output=True)
            self.assertEqual(first.returncode, 0, first.stderr)
            self.assertEqual(source.read_bytes(), original)
            prepared = output.read_bytes()
            again = subprocess.run([sys.executable, str(SCRIPT), str(source), str(output)], capture_output=True)
            self.assertNotEqual(again.returncode, 0)
            self.assertEqual(output.read_bytes(), prepared)
            inplace = subprocess.run([sys.executable, str(SCRIPT), str(source), str(source)], capture_output=True)
            self.assertNotEqual(inplace.returncode, 0)
            self.assertEqual(source.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
