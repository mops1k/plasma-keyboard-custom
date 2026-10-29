<!--
  SPDX-FileCopyrightText: 2026 Destin Moss <https://youcoded.ai>

  SPDX-License-Identifier: MIT
-->

# Swipe engine (vendored)

The MIT-licensed swipe recognition engine of the
[Swipe Keyboard](https://github.com/itsdestin/swipe-keyboard) project, vendored
as an in-tree static library so that the keyboard builds offline and the
distribution packages carry a fixed, reviewable copy.

- Upstream commit: `f48099755cd1ebeb3f3a9e40f680a683ccc95b2b` (2026-10-24).
- Copied: `engine/src/*.cpp` and `engine/include/swipekb/*.hpp`. The upstream
  `CMakeLists.txt`, its tests and its plugin layer are not copied;
  `CMakeLists.txt` here only builds the sources as `swipekb_engine`.
- Upstream author: Destin Moss, https://youcoded.ai. Licence: MIT (see
  `LICENSES/MIT.txt` in the repository root).
- One local change: in `src/swipe_layout.cpp` the code-point table that maps a
  letter to its key covers the whole Basic Multilingual Plane instead of
  `0x180` (Latin-1 + Latin Extended-A). Without it every non-Latin letter
  (Russian, Greek, Hebrew, Arabic) maps to "no key" and the vocabulary for
  such a layout is built empty. The rest of the sources keep their SPDX
  headers and are otherwise unchanged.

## What it does

`swipekb::LanguageEngine` is the entry point: it takes a finger path, the
letter-key rectangles and the previous words, and returns ranked dictionary
words. A word must be in the dictionary to be returned. With no swipe model it
decodes by key geometry alone, which is what the Russian language uses; the
English language also loads the optional 2.1 MB MIT neural model.

The dictionary format is AOSP `.combined`; `tools/generate-swipe-dictionaries.py`
builds it from the predictive word lists (`src/prediction/data/*.bin`) at build
time.

## Updating

1. Copy the new `engine/src` and `engine/include/swipekb` over these files.
2. Update the commit hash above.
3. Run the keyboard's tests and `reuse lint`.
