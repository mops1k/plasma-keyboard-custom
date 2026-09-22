/* SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>
   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */

(function () {
    "use strict";

    var I18N = {
        ru: {
            "meta.title": "Plasma Keyboard (custom) — виртуальная клавиатура для handheld",
            "meta.description": "Форк KDE plasma-keyboard с навигацией геймпадом, плавающим режимом, темами, подсказками слов и рядом буфера обмена для KDE Plasma 6 на Wayland.",
            "a11y.skip": "К содержимому",
            "a11y.close": "Закрыть",
            "nav.features": "Возможности",
            "nav.gallery": "Галерея",
            "nav.themes": "Темы",
            "nav.gamepad": "Геймпад",
            "nav.settings": "Настройки",
            "nav.install": "Установка",
            "hero.badge": "Форк KDE plasma-keyboard · 6.7.90 · KDE Plasma 6 / Wayland",
            "hero.title": "Виртуальная клавиатура, которой удобно управлять геймпадом",
            "hero.lead": "Plasma Keyboard (custom) — форк KDE plasma-keyboard для handheld-консолей: навигация геймпадом, плавающий режим, десять тем, подсказки слов, ряд буфера обмена и свои раскладки в стиле ПК. Ставится рядом с официальным пакетом и не заменяет его.",
            "hero.install": "Установить",
            "hero.source": "Исходники",
            "hero.releases": "Релизы",
            "hero.stat1v": "10",
            "hero.stat1": "встроенных тем",
            "hero.stat2v": "2,2 млн",
            "hero.stat2": "словоформ для подсказок",
            "hero.stat3": "показ клавиатуры по умолчанию",
            "hero.shotCaption": "Глифы кнопок геймпада нарисованы прямо на назначенных клавишах",
            "alt.keyboardEn": "Английская раскладка с глифами кнопок геймпада на клавишах",
            "alt.keyboardRu": "Русская раскладка в стиле ПК",
            "alt.fkeys": "Ряд F1–F12 над клавиатурой",
            "alt.clipboard": "Ряд буфера обмена с тремя чипами",
            "alt.floating": "Плавающая клавиатура с прозрачностью 50 %",
            "alt.catppuccin": "Тема Catppuccin Mocha",
            "alt.googleDark": "Тема Google Dark",
            "alt.ios": "Тема iOS",
            "alt.material": "Тема Material",
            "alt.kcm": "Страница настроек, вкладка «Раскладки»",
            "features.title": "Что здесь есть",
            "features.sub": "Всё, что добавлено к upstream, и всё это работает на сенсорном экране, мышью и геймпадом.",
            "feat.pad.t": "Навигация геймпадом",
            "feat.pad.d": "D-pad и стик ходят по клавишам и рядам над клавиатурой, A выбирает, B закрывает, X — backspace, Y — пробел, LT/RT — Shift и Enter, LB/RB — символы и раскладка.",
            "feat.hold.t": "Удержание A — доп. символы",
            "feat.hold.d": "Долгое нажатие A открывает альтернативные символы подсвеченной клавиши — те же, что по долгому тапу (е → ё, 1 → !). Список появляется над клавишей и следует выбранной теме; задержка настраивается.",
            "feat.float.t": "Плавающая клавиатура",
            "feat.float.d": "Клавиша со стрелками переключает клавиатуру между нижней панелью и плавающим окном: его можно тащить за фон, ширина 20–100 %, прозрачность 20–100 %, позиция запоминается.",
            "feat.themes.t": "Десять встроенных тем",
            "feat.themes.d": "system, светлая и тёмная, iOS, Material 3, Google (Gboard) в двух вариантах и Catppuccin Mocha. Свои темы — обычный JSON: палитра, геометрия, фон и цвета клавиш по категориям.",
            "feat.pred.t": "Подсказки слов",
            "feat.pred.d": "Продолжение слова, исправление опечатки в один символ и предсказание следующего слова по биграммам. До пяти подсказок, порог появления — от 1 до 4 букв.",
            "feat.clip.t": "Ряд буфера обмена",
            "feat.clip.d": "Последние записи менеджера буфера обмена рабочего стола (klipper) над клавиатурой: три чипа, нажатие вставляет текст в сфокусированное поле, кнопка у края забывает историю.",
            "feat.fkeys.t": "Ряд F1–F12",
            "feat.fkeys.d": "Необязательный ряд функциональных клавиш размером и стилем как обычные клавиши; панель растёт соответственно, а геймпад достаёт и до него.",
            "feat.latch.t": "Защёлкивание модификаторов",
            "feat.latch.d": "Ctrl, Alt и Shift защёлкиваются до следующей клавиши и затем сбрасываются; повторное нажатие снимает защёлку. Одинаково для касания, навигации и геймпада.",
            "feat.layouts.t": "Раскладки в стиле ПК",
            "feat.layouts.d": "Esc и клавиша скрытия, Ctrl/Alt, цифровой ряд, физический перевёрнутый T-блок стрелок, Del/Shift/&123 размером как Tab, без правого Shift; диакритики по долгому нажатию сохранены.",
            "feat.touch.t": "Открытие по долгому нажатию",
            "feat.touch.d": "Вместо всплытия при фокусе клавиатура может ждать долгого нажатия на сенсорном экране (100–5000 мс). Экран читается напрямую через evdev, глобальное сочетание клавиш показывает её сразу.",
            "feat.kcm.t": "Страница настроек",
            "feat.kcm.d": "Plasma Keyboard (custom) в Параметрах системы: Раскладки, Открытие, Внешний вид и Ввод — высота, шрифт, темы, подсказки, звук, вибрация, сочетание клавиш и тестовое поле.",
            "feat.update.t": "Звук, вибрация, автообновление",
            "feat.update.d": "Исправлена звуковая обратная связь (в upstream опция не компилировалась вовсе), есть вибрация, а после обновления пакета клавиатура перезапускается сама — выходить из сеанса не нужно.",
            "gallery.title": "Галерея",
            "gallery.sub": "Снято на MSI Claw A8 (1920×1200, KDE Plasma 6, Wayland). Нажмите на кадр, чтобы увеличить.",
            "gal.en": "Английская раскладка и глифы геймпада",
            "gal.ru": "Русская раскладка в стиле ПК",
            "gal.fkeys": "Ряд F1–F12",
            "gal.clipboard": "Ряд буфера обмена",
            "gal.floating": "Плавающая клавиатура, прозрачность 50 %",
            "gal.catppuccin": "Тема Catppuccin Mocha",
            "gal.googleDark": "Тема Google Dark (Gboard)",
            "gal.ios": "Тема iOS",
            "gal.material": "Тема Material",
            "themes.title": "Темы",
            "themes.sub": "Тема — это данные, а не код: файл JSON только с цветами и геометрией. Импорт чужого файла ничего не исполняет. Тема применяется к работающей клавиатуре сразу, без перезапуска.",
            "themes.thId": "Id",
            "themes.thName": "Название",
            "themes.thWhat": "Что это",
            "themes.system.n": "Системная",
            "themes.system.d": "По умолчанию: ничего не переопределяет и следует цветовой схеме Plasma.",
            "themes.light.n": "Светлая",
            "themes.light.d": "Системная палитра с более светлыми клавишами.",
            "themes.dark.n": "Тёмная",
            "themes.dark.d": "Системная палитра с более тёмными клавишами.",
            "themes.iosLight.n": "iOS (светлая)",
            "themes.iosLight.d": "Белые клавиши на сером фоне, плоская, подписи заглавными, скругления 6 px.",
            "themes.iosDark.n": "iOS (тёмная)",
            "themes.iosDark.d": "Тёмный вариант iOS.",
            "themes.materialLight.n": "Material (светлая)",
            "themes.materialLight.d": "Material 3 «Default»: белые клавиши, сине-серые акценты, скругления 12 px, без контура.",
            "themes.materialDark.n": "Material (тёмная)",
            "themes.materialDark.d": "Тёмный вариант Material 3.",
            "themes.catppuccin.d": "Почти чёрная панель, клавиши цвета mantle, спецклавиши surface0 и голубая клавиша действия; плоские клавиши, узкие зазоры.",
            "themes.googleLight.d": "Gboard в светлых цветах: белые клавиши на светло-серой панели, акцент Google Blue, плоские круглые клавиши.",
            "themes.googleDark.d": "Gboard в тёмных цветах: серо-синие клавиши, тёмные спецклавиши и бирюзовая клавиша действия.",
            "themes.custom.t": "Свои темы",
            "themes.custom.d": "Файл темы кладётся в ~/.local/share/plasma-keyboard/themes/*.json, а имя файла становится её id. Строка «Файлы тем» на вкладке «Внешний вид» импортирует, экспортирует и удаляет их, показывая ошибки прямо под строкой.",
            "pad.title": "Управление геймпадом",
            "pad.sub": "Пока клавиатура видна, геймпад перехватывается через InputPlumber (InterceptMode = GAMEPAD_ONLY), чтобы ввод не просачивался в игру или маппинг Steam. Прежний режим возвращается при скрытии клавиатуры.",
            "pad.stick": "стик",
            "pad.move": "Движение по клавишам; вверх с верхнего ряда уводит в ряд F1–F12, оттуда — в ряд буфера обмена.",
            "pad.a": "Активировать подсвеченный элемент; удержание открывает альтернативные символы клавиши.",
            "pad.b": "Закрыть список альтернативных символов или клавиатуру.",
            "pad.x": "Backspace; удержание продолжает удалять, как клавиша на аппаратной клавиатуре.",
            "pad.y": "Пробел.",
            "pad.lt": "Shift и Enter.",
            "pad.lb": "Символы и переключение раскладки.",
            "pad.select": "Перепрыгнуть в ряды над клавиатурой.",
            "pred.title": "Подсказки слов",
            "pred.sub": "Собственный движок предсказаний — штатный плагин hunspell Qt Virtual Keyboard намеренно остаётся отключённым. Чипы появляются с первой буквы, часть, которая допишется к слову, подчёркнута.",
            "pred.cont.t": "Продолжение слова",
            "pred.cont.d": "Около 1,18 млн русских и 1,03 млн английских словоформ из частотных словарей FrequencyWords (MIT). Самые частые — первыми.",
            "pred.fix.t": "Исправление опечатки",
            "pred.fix.d": "Если набранное не начало ни одного слова, предлагаются слова в одной опечатке: замена, перестановка, лишняя или пропущенная буква.",
            "pred.next.t": "Следующее слово",
            "pred.next.d": "Сразу после пробела предлагаются слова, которые могут идти за предыдущим — биграммы, посчитанные по корпусу предложений Tatoeba (CC BY 2.0 FR).",
            "pred.note": "Нажатие на чип (или A на геймпаде) применяет подсказку и добавляет пробел. Словари hunspell, установленные в системе, используются как дополнение: слова языка без встроенного списка берутся из них, и там же спрашивается исправление опечатки, если свои словари ничего не нашли. Без библиотеки или словарей всё работает как прежде.",
            "set.title": "Страница настроек",
            "set.sub": "Параметры системы → Plasma Keyboard (custom). Четыре вкладки, всё на русском, страница переведена доменом kcm_plasmakeyboardcustom.",
            "set.tab1.t": "Раскладки",
            "set.tab1.d": "Включённые языки списком: флаг, название, код, звёздочка «открывать по умолчанию», удаление и перетаскивание порядка — именно он задаёт кольцо переключения.",
            "set.tab2.t": "Открытие",
            "set.tab2.d": "Долгое нажатие и его порог, открытие при фокусе мыши, скрытие панели Plasma, пока клавиатура видна.",
            "set.tab3.t": "Внешний вид",
            "set.tab3.d": "Высота клавиатуры (20–80 %), шрифт клавиатуры, тема и файлы тем, ряд F1–F12, ряд буфера обмена, ширина и прозрачность плавающей клавиатуры.",
            "set.tab4.t": "Ввод",
            "set.tab4.d": "Подсказки слов (сколько и с какой буквы, предсказание следующего слова, исправление опечаток), альтернативные символы и задержка удержания, автозаглавные буквы, звук, вибрация, тестовое поле.",
            "set.caption": "Вкладка «Раскладки»: список языков, сочетание клавиш и тестовое поле",
            "inst.title": "Установка",
            "inst.sub": "Клавиатура ставится рядом с официальным пакетом plasma-keyboard и не заменяет его: всё переименовано, поэтому обе видны в Параметрах системы → Виртуальная клавиатура.",
            "inst.script.t": "Скриптом — самый быстрый способ",
            "inst.script.d": "Скачивает новейший релиз, сверяет с опубликованным SHA256SUMS, ставит через pacman и перезапускает клавиатуру. Нужны только curl и pacman.",
            "inst.script.opts": "Опции передаются через sh -s --: --tag, --overwrite, --no-restart, --dry-run, --help.",
            "inst.manual.t": "Вручную из релиза",
            "inst.manual.d": "Скачайте новейший пакет со страницы Releases, при желании сверьте контрольные суммы и установите.",
            "inst.repo.t": "Из pacman-репозитория",
            "inst.repo.d": "Каждый релиз публикуется в небольшой репозиторий в ветке gh-pages. Добавьте его один раз — дальше обновление одной командой.",
            "inst.steam.t": "SteamOS",
            "inst.steam.d": "Готовый пакет собран против Qt 6.9 и требует SteamOS 3.8 или новее; 3.7 и старше несут Qt 6.7/6.8 — там собирайте из исходников. Система смонтирована только для чтения, поэтому скрипт сам снимает и возвращает steamos-readonly, а в репозиториях SteamOS нет libstdc++:",
            "inst.fedora.t": "Fedora, Bazzite и другие дистрибутивы",
            "inst.fedora.d": "В релизе есть RPM для систем на базе Fedora (Fedora KDE, Bazzite, Nobara) и Flatpak-бандл, который несёт собственный стек Qt и KDE. RPM собирается под ветку Qt: fc43 для Fedora 43, fc44 для Fedora 44/45, потому что клавиатура использует приватные API Qt. На atomic-системах пакет ставится через rpm-ostree с последующей перезагрузкой.",
            "inst.flatpak.t": "Flatpak — для любого дистрибутива",
            "inst.flatpak.d": "Ставится в песочницу пользователя, поэтому KCM не попадает в системные настройки, а KWin нужно указать на desktop-файл Flatpak (настройки самой клавиатуры остаются доступны из клавиатуры):",
            "inst.after": "После установки выберите plasma-keyboard-custom в Параметры системы → Виртуальная клавиатура. Обновления ставятся той же командой: версия и pkgrel растут с каждым релизом, а клавиатура перезапускается сама.",
            "notes.title": "Хорошо знать",
            "notes.1": "Открытие по долгому нажатию читает сенсорный экран напрямую через evdev — вместе с пакетом ставится правило udev с uaccess.",
            "notes.2": "Второй экземпляр клавиатуры сразу завершается, поэтому устаревший процесс не сможет удержать старую панель.",
            "notes.3": "Плавающая клавиатура рисуется поверх других окон, не сдвигает окно под собой и не убирает панель Plasma.",
            "notes.4": "Иконки клавиатуры зашиты в приложение, поэтому её вид не зависит от темы иконок в системе.",
            "notes.5": "По умолчанию KWin показывает клавиатуру только при касании текстового поля; KWIN_IM_SHOW_ALWAYS=1 заставляет её всплывать всегда.",
            "foot.fork": "Форк KDE plasma-keyboard (6.7.90), построенного на Qt Virtual Keyboard. Лицензия и авторские права upstream сохраняются.",
            "foot.releases": "Релизы",
            "foot.license": "Лицензия",
            "foot.icons": "Иконки: Papirus (© Papirus Development Team, GPL-3.0-only) и Breeze (© участники KDE, LGPL-3.0-or-later). Словари подсказок: FrequencyWords (MIT) и Tatoeba (CC BY 2.0 FR)."
        },
        en: {
            "meta.title": "Plasma Keyboard (custom) — a virtual keyboard for handhelds",
            "meta.description": "A fork of KDE plasma-keyboard with gamepad navigation, a floating mode, themes, word suggestions and a clipboard row, for KDE Plasma 6 on Wayland.",
            "a11y.skip": "Skip to content",
            "a11y.close": "Close",
            "nav.features": "Features",
            "nav.gallery": "Gallery",
            "nav.themes": "Themes",
            "nav.gamepad": "Gamepad",
            "nav.settings": "Settings",
            "nav.install": "Install",
            "hero.badge": "A fork of KDE plasma-keyboard · 6.7.90 · KDE Plasma 6 / Wayland",
            "hero.title": "A virtual keyboard that is a joy to drive with a gamepad",
            "hero.lead": "Plasma Keyboard (custom) is a fork of KDE plasma-keyboard made for handheld consoles: gamepad navigation, a floating mode, ten themes, word suggestions, a clipboard row and PC-style layouts. It installs next to the official package instead of replacing it.",
            "hero.install": "Install",
            "hero.source": "Source",
            "hero.releases": "Releases",
            "hero.stat1v": "10",
            "hero.stat1": "built-in themes",
            "hero.stat2v": "2.2M",
            "hero.stat2": "word forms for suggestions",
            "hero.stat3": "shows the keyboard by default",
            "hero.shotCaption": "Gamepad button glyphs are drawn right on the keys they are bound to",
            "alt.keyboardEn": "English layout with gamepad button glyphs on the keys",
            "alt.keyboardRu": "Russian PC-style layout",
            "alt.fkeys": "The F1–F12 row above the keyboard",
            "alt.clipboard": "The clipboard row with three chips",
            "alt.floating": "The floating keyboard at 50% opacity",
            "alt.catppuccin": "The Catppuccin Mocha theme",
            "alt.googleDark": "The Google Dark theme",
            "alt.ios": "The iOS theme",
            "alt.material": "The Material theme",
            "alt.kcm": "The settings page, Layouts tab",
            "features.title": "What is in here",
            "features.sub": "Everything added on top of upstream, and all of it works with touch, a mouse and a gamepad.",
            "feat.pad.t": "Gamepad navigation",
            "feat.pad.d": "The D-pad and the stick walk the keys and the rows above the keyboard, A activates, B closes, X is backspace, Y is space, LT/RT are Shift and Enter, LB/RB switch symbols and layout.",
            "feat.hold.t": "Hold A for extra symbols",
            "feat.hold.d": "Holding A opens the alternative symbols of the highlighted key — the same ones a long tap shows (е → ё, 1 → !). The list appears above that key and follows the theme; the delay is configurable.",
            "feat.float.t": "Floating keyboard",
            "feat.float.d": "An arrow key switches the keyboard between a bottom panel and a floating window: drag it by the background, width 20–100%, opacity 20–100%, and the position is remembered.",
            "feat.themes.t": "Ten built-in themes",
            "feat.themes.d": "system, light and dark, iOS, Material 3, Google (Gboard) in two variants, and Catppuccin Mocha. Your own themes are plain JSON: palette, geometry, background and per-category key colours.",
            "feat.pred.t": "Word suggestions",
            "feat.pred.d": "Word completion, single-typo correction and next-word prediction from bigrams. Up to five suggestions, appearing from 1 to 4 letters.",
            "feat.clip.t": "Clipboard row",
            "feat.clip.d": "The latest entries of the desktop clipboard manager (klipper) above the keyboard: three chips, a tap inserts the text into the focused field, and the button at the edge forgets the history.",
            "feat.fkeys.t": "F1–F12 row",
            "feat.fkeys.d": "An optional row of function keys sized and styled like ordinary keys; the panel grows accordingly and the gamepad reaches it too.",
            "feat.latch.t": "Latched modifiers",
            "feat.latch.d": "Ctrl, Alt and Shift latch until the next key and then reset; pressing again releases the latch. The same for touch, key navigation and the gamepad.",
            "feat.layouts.t": "PC-style layouts",
            "feat.layouts.d": "Esc and the hide key, Ctrl/Alt, a digit row, a physical inverted-T arrow block, Del/Shift/&123 sized like Tab and no right Shift; long-press diacritics are kept.",
            "feat.touch.t": "Open on a long press",
            "feat.touch.d": "Instead of popping up on focus, the keyboard can wait for a long press on the touchscreen (100–5000 ms). The screen is read directly through evdev, and the global shortcut still shows it at once.",
            "feat.kcm.t": "Settings page",
            "feat.kcm.d": "Plasma Keyboard (custom) in System Settings: Layouts, Opening, Appearance and Typing — height, font, themes, suggestions, sound, vibration, the shortcut and a try-it field.",
            "feat.update.t": "Sound, vibration, self-update",
            "feat.update.d": "Key-click feedback is fixed (the upstream option never compiled at all), vibration is there, and after a package update the keyboard restarts itself — no session logout needed.",
            "gallery.title": "Gallery",
            "gallery.sub": "Shot on an MSI Claw A8 (1920×1200, KDE Plasma 6, Wayland). Click a frame to enlarge it.",
            "gal.en": "English layout and gamepad glyphs",
            "gal.ru": "Russian PC-style layout",
            "gal.fkeys": "The F1–F12 row",
            "gal.clipboard": "The clipboard row",
            "gal.floating": "Floating keyboard at 50% opacity",
            "gal.catppuccin": "Catppuccin Mocha theme",
            "gal.googleDark": "Google Dark theme (Gboard)",
            "gal.ios": "iOS theme",
            "gal.material": "Material theme",
            "themes.title": "Themes",
            "themes.sub": "A theme is data, not code: a JSON file with colours and a little geometry. Importing somebody else's file executes nothing. A theme applies to the running keyboard at once, with no restart.",
            "themes.thId": "Id",
            "themes.thName": "Name",
            "themes.thWhat": "What it is",
            "themes.system.n": "System",
            "themes.system.d": "The default: overrides nothing and follows the Plasma colour scheme.",
            "themes.light.n": "Light",
            "themes.light.d": "The system palette with lighter keys.",
            "themes.dark.n": "Dark",
            "themes.dark.d": "The system palette with darker keys.",
            "themes.iosLight.n": "iOS (light)",
            "themes.iosLight.d": "White keys on a grey background, flat, upper-case labels, 6 px corners.",
            "themes.iosDark.n": "iOS (dark)",
            "themes.iosDark.d": "The dark iOS variant.",
            "themes.materialLight.n": "Material (light)",
            "themes.materialLight.d": "Material 3 “Default”: white keys, blue-grey accents, 12 px corners, no outline.",
            "themes.materialDark.n": "Material (dark)",
            "themes.materialDark.d": "The dark Material 3 variant.",
            "themes.catppuccin.d": "A near-black panel, mantle-coloured letter keys, surface0 special keys and a blue action key; flat keys with narrow gaps.",
            "themes.googleLight.d": "Gboard in light colours: white keys on a light grey panel, a Google Blue action key, flat round keys.",
            "themes.googleDark.d": "Gboard in dark colours: grey-blue keys, dark special keys and a teal action key.",
            "themes.custom.t": "Your own themes",
            "themes.custom.d": "A theme file goes into ~/.local/share/plasma-keyboard/themes/*.json and the file name becomes its id. The “Theme files” row on the Appearance tab imports, exports and removes them, reporting errors right below the row.",
            "pad.title": "Gamepad controls",
            "pad.sub": "While the keyboard is visible the gamepad is intercepted through InputPlumber (InterceptMode = GAMEPAD_ONLY), so input does not leak into the game or a Steam mapping. The previous mode comes back when the keyboard is hidden.",
            "pad.stick": "stick",
            "pad.move": "Move across the keys; going up from the top row leads into the F1–F12 row and from there into the clipboard row.",
            "pad.a": "Activate the highlighted item; holding it opens the alternative symbols of the key.",
            "pad.b": "Close the alternative-symbol list or the keyboard.",
            "pad.x": "Backspace; holding it keeps deleting, like the key on a hardware keyboard.",
            "pad.y": "Space.",
            "pad.lt": "Shift and Enter.",
            "pad.lb": "Symbols and layout switching.",
            "pad.select": "Jump into the rows above the keyboard.",
            "pred.title": "Word suggestions",
            "pred.sub": "The prediction engine is our own — the stock hunspell plugin of Qt Virtual Keyboard is deliberately left off. Chips appear from the first letter, and the part that will be appended is underlined.",
            "pred.cont.t": "Word completion",
            "pred.cont.d": "About 1.18M Russian and 1.03M English word forms from the FrequencyWords frequency lists (MIT). The most frequent come first.",
            "pred.fix.t": "Typo correction",
            "pred.fix.d": "When what you typed starts no word at all, words one typo away are offered: a substitution, a transposition, an extra or a missing letter.",
            "pred.next.t": "Next word",
            "pred.next.d": "Right after a space, words that can follow the previous one are offered — bigrams counted in the Tatoeba sentence corpus (CC BY 2.0 FR).",
            "pred.note": "Clicking a chip (or pressing A on the gamepad) applies the suggestion and adds a space. hunspell dictionaries installed on the system are used as a fallback: words of a language without a built-in list come from them, and typo correction is asked there when our own lists find nothing. Without the library or the dictionaries everything works as before.",
            "set.title": "Settings page",
            "set.sub": "System Settings → Plasma Keyboard (custom). Four tabs, translated through the kcm_plasmakeyboardcustom domain.",
            "set.tab1.t": "Layouts",
            "set.tab1.d": "The enabled languages as a list: flag, name, code, a star for “open by default”, removal and drag handles for the order — that order is the switching ring.",
            "set.tab2.t": "Opening",
            "set.tab2.d": "Long press and its threshold, opening on mouse focus, hiding the Plasma panel while the keyboard is visible.",
            "set.tab3.t": "Appearance",
            "set.tab3.d": "Keyboard height (20–80%), keyboard font, the theme and theme files, the F1–F12 row, the clipboard row, floating width and opacity.",
            "set.tab4.t": "Typing",
            "set.tab4.d": "Word suggestions (how many and from which letter, next-word prediction, typo correction), alternative symbols and the hold delay, auto-capitalisation, sound, vibration and a try-it field.",
            "set.caption": "The Layouts tab: the language list, the shortcut and the try-it field",
            "inst.title": "Install",
            "inst.sub": "The keyboard installs next to the official plasma-keyboard package instead of replacing it: everything is renamed, so both show up in System Settings → Virtual Keyboard.",
            "inst.script.t": "With the script — the fastest way",
            "inst.script.d": "It fetches the newest release, verifies it against the published SHA256SUMS, installs it with pacman and restarts the keyboard. Only curl and pacman are needed.",
            "inst.script.opts": "Options go through sh -s --: --tag, --overwrite, --no-restart, --dry-run, --help.",
            "inst.manual.t": "Manually from a release",
            "inst.manual.d": "Download the newest package from the Releases page, verify the checksums if you like, and install it.",
            "inst.repo.t": "From the pacman repository",
            "inst.repo.d": "Every release is also published to a small repository on the gh-pages branch. Add it once and update with a single command afterwards.",
            "inst.steam.t": "SteamOS",
            "inst.steam.d": "The ready-made package is built against Qt 6.9 and needs SteamOS 3.8 or newer; 3.7 and older ship Qt 6.7/6.8, so build from source there. The system is mounted read-only, so the script disables and restores steamos-readonly itself, and SteamOS repositories have no libstdc++:",
            "inst.fedora.t": "Fedora, Bazzite and other distributions",
            "inst.fedora.d": "Releases also carry an RPM for Fedora-based systems (Fedora KDE, Bazzite, Nobara) and a Flatpak bundle with its own Qt and KDE stack. The RPM is built per Qt branch — fc43 for Fedora 43, fc44 for Fedora 44/45 — because the keyboard uses Qt's private APIs. On atomic systems the package is installed with rpm-ostree and a reboot.",
            "inst.flatpak.t": "Flatpak — for any distribution",
            "inst.flatpak.d": "It installs into the user's sandbox, so its KCM cannot appear in the system settings, and KWin has to be pointed at the desktop file of the Flatpak (the settings of the keyboard itself stay available from the keyboard):",
            "inst.after": "After installing, pick plasma-keyboard-custom in System Settings → Virtual Keyboard. Updates use the same command: the version and pkgrel grow with every release and the keyboard restarts itself.",
            "notes.title": "Good to know",
            "notes.1": "Open-on-long-press reads the touchscreen directly through evdev — a udev rule granting uaccess ships with the package.",
            "notes.2": "A second keyboard instance exits immediately, so a stale process can never hold on to an old panel.",
            "notes.3": "The floating keyboard is drawn above other windows, does not shift the window below it and does not remove the Plasma panel.",
            "notes.4": "The keyboard icons are shipped inside the application, so its look does not depend on the system icon theme.",
            "notes.5": "By default KWin shows the keyboard only when a text field is touched; KWIN_IM_SHOW_ALWAYS=1 makes it pop up always.",
            "foot.fork": "A fork of KDE plasma-keyboard (6.7.90), built on Qt Virtual Keyboard. Upstream licences and copyrights are kept.",
            "foot.releases": "Releases",
            "foot.license": "Licence",
            "foot.icons": "Icons: Papirus (© Papirus Development Team, GPL-3.0-only) and Breeze (© KDE contributors, LGPL-3.0-or-later). Suggestion dictionaries: FrequencyWords (MIT) and Tatoeba (CC BY 2.0 FR)."
        }
    };

    var STORAGE_KEY = "pkb-lang";

    function applyLang(lang) {
        var dict = I18N[lang] || I18N.ru;
        document.documentElement.lang = lang;

        document.querySelectorAll("[data-i18n]").forEach(function (el) {
            var value = dict[el.getAttribute("data-i18n")];
            if (value) { el.textContent = value; }
        });
        document.querySelectorAll("[data-i18n-content]").forEach(function (el) {
            var value = dict[el.getAttribute("data-i18n-content")];
            if (value) { el.setAttribute("content", value); }
        });
        document.querySelectorAll("[data-i18n-alt]").forEach(function (el) {
            var value = dict[el.getAttribute("data-i18n-alt")];
            if (value) { el.setAttribute("alt", value); }
        });
        document.querySelectorAll("[data-i18n-aria]").forEach(function (el) {
            var value = dict[el.getAttribute("data-i18n-aria")];
            if (value) { el.setAttribute("aria-label", value); }
        });
        document.querySelectorAll(".lang-switch button").forEach(function (btn) {
            btn.classList.toggle("is-active", btn.getAttribute("data-lang") === lang);
        });
        try { localStorage.setItem(STORAGE_KEY, lang); } catch (e) { /* private mode */ }
    }

    function initialLang() {
        var stored = null;
        try { stored = localStorage.getItem(STORAGE_KEY); } catch (e) { stored = null; }
        if (stored === "ru" || stored === "en") { return stored; }
        var nav = (navigator.language || "ru").toLowerCase();
        return nav.indexOf("ru") === 0 ? "ru" : "en";
    }

    function initLang() {
        document.querySelectorAll(".lang-switch button").forEach(function (btn) {
            btn.addEventListener("click", function () { applyLang(btn.getAttribute("data-lang")); });
        });
        applyLang(initialLang());
    }

    function initNav() {
        var nav = document.getElementById("nav");
        if (!nav) { return; }
        var update = function () { nav.classList.toggle("is-scrolled", window.scrollY > 8); };
        update();
        window.addEventListener("scroll", update, { passive: true });
    }

    function initLightbox() {
        var box = document.getElementById("lightbox");
        if (!box) { return; }
        var img = box.querySelector("img");
        var open = function (src, alt) {
            img.setAttribute("src", src);
            img.setAttribute("alt", alt || "");
            box.hidden = false;
            document.body.style.overflow = "hidden";
        };
        var close = function () {
            box.hidden = true;
            img.setAttribute("src", "");
            document.body.style.overflow = "";
        };
        document.querySelectorAll(".shot").forEach(function (shot) {
            shot.addEventListener("click", function () {
                var source = shot.querySelector("img");
                open(shot.getAttribute("data-full"), source ? source.getAttribute("alt") : "");
            });
        });
        box.addEventListener("click", function (event) {
            if (event.target === box || event.target.classList.contains("lightbox-close")) { close(); }
        });
        document.addEventListener("keydown", function (event) {
            if (event.key === "Escape" && !box.hidden) { close(); }
        });
    }

    function initCopy() {
        document.querySelectorAll("[data-copy]").forEach(function (button) {
            button.addEventListener("click", function () {
                var row = button.closest(".code-row");
                var code = row ? row.querySelector("code") : null;
                if (!code) { return; }
                var text = code.textContent;
                var done = function () {
                    button.classList.add("is-done");
                    window.setTimeout(function () { button.classList.remove("is-done"); }, 1400);
                };
                if (navigator.clipboard && navigator.clipboard.writeText) {
                    navigator.clipboard.writeText(text).then(done, function () { /* ignore */ });
                } else {
                    var area = document.createElement("textarea");
                    area.value = text;
                    document.body.appendChild(area);
                    area.select();
                    try { document.execCommand("copy"); done(); } catch (e) { /* ignore */ }
                    document.body.removeChild(area);
                }
            });
        });
    }

    function initReveal() {
        var targets = document.querySelectorAll(".section-head, .card, .shot, .install-card, .tabs-list li");
        if (!("IntersectionObserver" in window)) { return; }
        var observer = new IntersectionObserver(function (entries) {
            entries.forEach(function (entry) {
                if (entry.isIntersecting) {
                    entry.target.classList.add("is-visible");
                    observer.unobserve(entry.target);
                }
            });
        }, { rootMargin: "0px 0px -8% 0px", threshold: 0.08 });
        targets.forEach(function (target) {
            target.classList.add("reveal");
            observer.observe(target);
        });
    }

    document.addEventListener("DOMContentLoaded", function () {
        initLang();
        initNav();
        initLightbox();
        initCopy();
        initReveal();
    });
})();
