# The swipe model and its credits

The neural swipe model used for English is the MIT-licensed model of the
[Swipe Keyboard](https://github.com/itsdestin/swipe-keyboard) project:

- `swipe-model.bin` — 2 115 260 bytes, SHA-256
  `941272a7017229a73e07d1c0bd766757e739174db3a42fb622c26256b8c46110`,
  release `models-2026-10-23`.
- Copyright: 2026 Destin Moss, https://youcoded.ai. Licence: MIT (see
  `LICENSES/MIT.txt`).
- Trained on real swipes from FUTO's `swipe.futo.org` dataset (MIT, Copyright
  (c) 2025 FUTO — the text is in `FUTO-SWIPE-DATA-LICENSE.txt` next to this
  file) plus synthetic swipes. It contains no third-party model weights.
- Not used for Russian: the model was trained on English, Spanish, French and
  German, so Russian swipes are decoded by the engine's geometric decoder over
  the Russian word list.

If the file is replaced, update its SHA-256 here.

<!--
  SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
-->
