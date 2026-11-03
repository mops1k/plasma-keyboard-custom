<!--
  SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
-->

# KScreenLocker: захищений ввід, фокус та геометрія

> **Runtime regression / withdrawn, 2026-10-20:** наведений нижче focus patch
> НЕ прийнятий. Після реального тесту користувач повідомив приховану клавіатуру
> та втрату старої touch/release поведінки. Відновити весь оригінальний theme
> QML з backup; не встановлювати цю версію LocalTouchFocus. Installer у source
> заблокований. Passing offscreen harness не є runtime acceptance.
>
> Для окремої visibility regression RPM11 знайдено помилку: authorization з
> QWindow::visibleChanged(true) надто рання — сигнал передує створенню platform
> window та shell role. У підготовці RPM12 цей early callback прибрано;
> post-show QInputMethod/recreate шляхи залишено. Це не новий експеримент із
> фокусом теми. Потрібна перевірка користувачем після встановлення.

## Межі

Це KScreenLocker у Wayland-сесії, не SDDM. Користувач підтвердив точне
попадання дотиків у кнопки, зокрема з кількома моніторами. Не змінювати
калібрування або mapping сенсора за цим симптомом. Другий сенсорний дисплей
ще не підключений; поточний reader першого evdev touchscreen і проєкція
AT-SPI hit test на primaryScreen — окрема майбутня задача.

Не збирати текст вводу, clipboard, keycodes або debugger locals. Для перевірки
фокусу та розміщення достатньо торкатися порожніх полів без введення пароля.

## Виправлення в першому безпековому пакеті

- `InputContentPolicy`: заборонити власні підказки для password purpose,
  hidden/sensitive hints та нового контексту до отримання content type.
- Перевіряти дозвіл до читання prediction text і повторно перед commit.
- На зміну content type очищати/інвалідувати асинхронні підказки.
- `SessionLockState`: асинхронний GetActive, generation guard, невідомий стан
  при втраті сервісу. Підказки заборонені на всьому lock screen і до з'ясування
  стану. Синхронного GetActive в обробнику зміни input context більше немає.
- На перехід locked → unlocked приховувати/скидати панель до оновлення
  підказок; зупиняти hide timer та long-touch gesture. Початковий unlocked
  стан не є подією розблокування.
- Видалити debug-лог заміненого/вибраного слова з `applyPrediction`.

Це не є твердженням про виправлення мультимоніторного фокусу або геометрії.
Перевірено збіркою, policy/async тестами та SessionLockState на приватній D-Bus.
Повний Wayland compositor integration залишається непідтвердженим.

## Виявлений механізм геометрії

У KWin v6.7.5 `InputPanelV1Window::resetPosition()` бере
`surface()->input().boundingRect()`. Геометрія передається клієнту як overlap
input panel; Wayland-компонент Breeze бере висоту з
`InputMethod.keyboardRectangle`. Наш stub має mask 1×1.

Layer-shell exclusive zone видимої клавіатури не замінює це повідомлення.
Вона рухає звичайні вікна, але lock-screen view працює як fullscreen surface.

Не можна просто перетворити весь прозорий stub на input region клавіатури:
input-panel і layer-shell overlay можуть опинитися в одному stacking layer,
і stub перехоплюватиме дотики.

Кандидат на малу перевірку: лише для docked+locked задати дві input-точки
по протилежних кутах реального panel rect. Bounding box повідомляє розмір,
а точна hit-test region охоплює лише дві точки. Потрібна перевірка KWin/Qt
hit testing, clipping до buffer size, fractional scale та зміни екрана.
Поза lock screen зберігати stub 1×1, щоб не резервувати простір двічі.
Цей експеримент ще не входить до першого безпекового пакета.

## Кандидат на причину стрибка фокусу

У наявному локальному `LockScreenUi.qml` є TOUCHFIX:

1. `blockUI` залежить від глобальних `hasTransientTouchInput` і
   `inputPanel.keyboardActive`.
2. `onBlockUIChanged` встановлює `uiVisible = true` у різних screen views.
3. `onUiVisibleChanged` викликає `Window.window.requestActivate()`.

Неторкнуте вікно може активуватися після появи клавіатури й забрати фокус.
Сам `passwordField.forceActiveFocus()` не є доказом активації іншого native
window: це item focus усередині view.

Потрібно відділити локальний натиск від глобального keyboard-visible update
і не активувати інші views. Перед правкою системної теми зберегти всі наявні
TOUCHFIX-зміни та перевірити, що press на дочірньому password control не
обходить обраний обробник. Зміни теми не підміняються виправленням IM.

## Перевірка на реальному KWin

Після погодженого встановлення, без raw input logging:

- Порожнє поле на екрані A → caret/keyboard на A; повторити з мишею на B.
- Повторити, коли keyboard уже відкрита, та з іншим порядком outputs.
- Docked: поле вище клавіатури, кнопки клавіатури отримують дотики.
- Unlock під час відкладеної relocation: панель не з'являється повторно.
- Повторити з `hideOnInputFocusLoss` увімкненим і вимкненим.
- Різні масштаби/геометрії екранів і hotplug; без hardcoded primary/output.

Окремий тест privacy виконувати на фіктивних полях із синтетичними даними,
не на справжньому паролі.

## Підготовка наступного пакета (RPM11)

- ClipboardHistory тепер має backend guards для unknown/locked/protected field
  і вимкненого clipboard setting. Немає синхронних Klipper calls. Локальний
  cache очищається; desktop Klipper history НЕ очищається. Запізнілі відповіді
  відкидаються, paste передає лише індекс із повторною backend перевіркою.
- Допоміжний UI приховано: clipboard/suggestions row, F1–F12, microphone/STT,
  settings. Мовний перемикач та звичайні клавіші збережено. Це також стосується
  захищених полів у звичайній сесії.
- Sparse geometry застосовано лише для known locked+docked. На unlocked
  desktop лишається 1×1. Stub стежить за розміром свого output, а unlock
  скасовує pending relocation.
- Desktop-native surface не авторизується для показу над lock screen. Зміна
  restricted/unrestricted режиму потребує hide/destroy/recreate ролі, щоб
  старий desktop buffer не став дозволеним lock-screen buffer. Невдала
  authorization не повторюється на тій самій потенційно mapped surface.

Unit/private-D-Bus тести перевіряють guards, cache purge, stale replies,
coalescing, lifetime, sparse regions та role-action policy. Це не замінює
перевірку wire geometry/першого buffer/native role transitions на реальному KWin.

## Окремий focus patch теми

`tools/lockscreen/prepare-focus-patch.py` створює новий QML без in-place
редагування; він відмовляється працювати з невідомою або дубльованою структурою.
Generic uiVisible більше не запитує native activation. Глобальний blockUI
може показати лише вже активний view.

`LocalTouchFocus.qml` використовує пасивний PointHandler, приєднаний **до
самого password control**, не до його предка. Це важлива відмінність: у
синтетичному тесті ancestor handler не отримував press над TextField, хоча
TouchBegin доходив до правильного вікна; натиск поза полем працював. Пряме
приєднання виправило тест. При кожному новому press активується саме цей view,
незалежно від того, чи його UI вже видимий.

`lockscreenfocustest` перевіряє два QQuickWindow, MouseArea з
`drag.filterChildren: true` і справжні TextField: глобальний показ клавіатури не
активує сусіда, повторні A→B→A торкання активують потрібне вікно, а поле отримує
focus. Це offscreen test наміру/native Qt activation, не end-to-end KWin seat test.

На keefrog системна lockscreen directory — read-only bind mount із
`/var/usrlocal/share/plasma/shells/org.kde.plasma.desktop.touchfix/contents/lockscreen`.
Підготовлений patch застосовується **окремо від RPM**, лише за явною
інтерактивною командою: `apply-prepared-focus.sh` перевіряє unlocked стан,
SHA256 оригіналу й staging, робить backup і зберігає ownership/mode/SELinux
context. Він не remount-ить файлову систему, не змінює PAM і не запускає
lock/unlock. За невідповідності layout/hash потрібно зупинитись і переглянути
оновлений source, а не форсувати запис. Після застосування потрібен ручний
A/B на порожньому полі; backup дозволяє повернути попередній QML.
