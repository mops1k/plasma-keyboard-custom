<!--
  SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
-->

# Наступний крок після RPM12: контракт видимості й output

## Перевірений стан, 2026-10-21

Користувач підтвердив: keyboard видима, TOUCHFIX не ховає UI після release,
але панель лишається на default output, з'являється до greeter та рухає session
windows. Кнопка Virtual Keyboard у KScreenLocker не приховує/не показує панель.
Підняття поля не є повним acceptance, оскільки keyboard на іншому output.

Не змінювати тему, калібрування/прив'язку сенсора або збирати наступний RPM
лише на підставі нового припущення. Focus patch попереднього етапу withdrawn.

## Конкретні source contracts

1. `syncKeyboardScreen()` викликає LayerShellQt `setScreen(stub->screen())`.
   На встановленій LayerShellQt 6.7.5 це вимикає попереднє
   `setWantsToBeOnActiveScreen(true)`. Новий `layershelloutputpolicytest`
   відтворює точний setter sequence без native surface: 4 QtTest cases passed.
   Це доказ override/pin, НЕ повний доказ primary-only routing на KWin.
2. `KeyboardHotkeyController::updatePanelVisibility()` не виконує compositor
   hide при `hideOnInputFocusLoss=false` (поточне налаштування користувача).
   Втрата input focus і явний KWin hide не є однією подією.
3. `SessionLockState::stateChanged` примусово показує панель при
   `locked && hasContext()`, хоча context може бути старим desktop context.
   Layer-shell exclusive zone при цьому не обнуляється на час lock.
4. `syncStubVisibility()` прирівнює mapping input-panel stub до нашої layer
   visibility. Після unmap KWin `show()` може лише повторно надіслати
   surrounding/content/commit у той самий context, без activate. Наш
   `m_hiddenByUser` блокує цей шлях. Просто змінити один hide conditional
   недостатньо.

## Корекція попередніх припущень

Input-panel-v1 не має configure-output event. Наш `applyConfigure()` читає
поточний Qt screen і відправляє set_toplevel; KWin ігнорує його output argument
і позиціонує panel за activeOutput. Qt оновлює screen через справжні
wl_surface.enter/leave та oldestEnteredScreen.

KWin output membership базується на frameGeometry, не всьому прозорому buffer.
Тому buffer overlap не вважається доведеною причиною primary pinning. Перед
зміною routing треба спостерігати wire enter/leave та get_layer_surface output.

## PreparingLock

На реальному /ScreenSaver доступний `org.kde.screensaver.AboutToLock`.
KWin 6.7.5 сам підключає KSldApp::aboutToLock до InputMethod::hide. Цей hide
має діяти незалежно від hide-on-focus-loss; сигнал AboutToLock і D-Bus
visible=false можуть надходити в різному порядку.

Кандидат state machine: Unknown / Unlocked / PreparingLock / Locked;
visibility/request/context epochs; окремі причини local hide, compositor hide,
context loss, role recreation. AboutToLock інвалідовує старі requests, ховає
layer і обнуляє zone; сам ActiveChanged(true) не є show intent.

Важливо: навіть свіжий GetActive після AboutToLock може законно бути false,
бо acquiring/grab ще триває. Такий reply або timeout не доводить cancellation.
При невдалому grab ActiveChanged(false) може не надійти. Cancellation/reconcile
policy лишається відкритою і потребує окремого source/test доказу; не розблоковувати
privacy автоматично лише через таймер.

## Обов'язковий harness перед наступним пакетом

Тестувати production bridge/controller разом із recreate callbacks, не лише
окремий helper чи заново написану спрощену модель. Без watchdog/повного daemon,
на приватній D-Bus і тимчасовому config/runtime каталозі.

- false → true → false → true від compositor із hideOnInputFocusLoss=false,
  тим самим context і повторними polls: hide виконується, повторний show працює.
- Compositor hide не є user hide і сам не руйнує mapped stub, потрібний для
  наступного panel->show(). Local hide та його unmap echo перевірити окремо.
- AboutToLock зі старим desktop context: жодного show або exclusive zone.
- Обидва порядки AboutToLock/visible=false; initial query → AboutToLock →
  stale false; fresh false під час PreparingLock; ActiveChanged(true); owner loss.
- Greeter adopt без нового activate, а також greeter context раніше за GetActive.
- Два справжні wl_output: A-only → A+B → B-only enter/leave; output argument
  нового layer role; зміна Qt screen; resize і fractional scale.
- Unlock під час pending relocation/recreate: нічого не показується повторно.
- Десктопна long-tap політика та privacy guards не мають регресувати.

Поточний mockinputmethodcompositor має один output, немає layer-shell server,
залежить від таймера activation і може запускати production watchdog із
успадкованою D-Bus. Не запускати його на реальному bus і не вважати його
init failure доказом конкретної production-причини.

Метадані для перевірок: enums/epochs, booleans, output IDs і geometry. Без
тексту полів, clipboard, keycodes, screenshots реальних password fields або
unfiltered child stderr від production session.
