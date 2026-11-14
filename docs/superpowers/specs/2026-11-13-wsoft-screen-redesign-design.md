# W-Soft board screen redesign

Date: 13 Nov 2026. Owner: Rhyno. Boards: yellow CYD 2.8" (320 x 240, `wsoft_cyd28`) and
Waveshare 4.3" (800 x 480, `wsoft_waveshare43`). The screenless S3 board draws nothing and
only keeps compiling.

## Why

Rhyno (13 Nov 2026, in his words): "update the UI layout for the ESP display board. Separate
the Gross value and Menu into distinct, well spaced buttons. Add a persistent display of the
device's current IP address and its Wi-Fi connection status. If disconnected, include an
option to tap to connect to Wi-Fi or dismiss the alert, while maintaining the existing
functional integrity. Make the weight and the high low the [heroes], and look at the
Weighsoft Edge bench design and scheme layout ... create the light mode and the dark mode ...
simplify the code and make sure we have all the functionality we need."

After the first mock-up (20:26): "I like it. Looks good. Make sure we have enough space on the
yellow board ESP and all the other ESP that has screens. And make something like the weights
or the background change color like the low, okay, high changes color. So it's obvious that
you're on the right place and obvious that you're on the wrong place. So the screen changes
with the relay basically."

Go-ahead to build and flash: 13 Nov 20:31 ("get all the work done ... flash the ESP devices").

## Goals

1. One layout on both boards, in the look of the Edge bench screen (navy bar, dark deck,
   Sora for the weight, Manrope for words), with a light mode.
2. The weight and the Hi-Lo result are the two biggest things on the screen.
3. The weight panel takes the colour of the Hi-Lo relay zone: yellow LOW, green OK, red HIGH,
   plain when the relays are off (no product, empty scale, no signal, alarm, blanked).
4. GROSS, MENU and SAVE are three separate boxes.
5. The IP address and Wi-Fi strength are always on the status bar. When Wi-Fi is down a yellow
   alert offers CONNECT (the steps to put the board on Wi-Fi from a phone) or X.
6. Every function of the old screen stays: SAVE with its result line, the red and yellow
   save notices, trade band and reason, plate line, audit counter, NET / NET (TARE), Hub
   state, scale link, MENU with Approval (PIN pad, the no-PIN note), Auto save, TARE, PRESET
   TARE, PRODUCT, the 30 s idle return.
7. Less code that draws, shared by both boards, tested on the laptop without a board.

## Not in this change

- No change to the Modbus poll, approval, weighings, Hub, web page or MQTT. The weigh loop
  changes in one place only: the relay zone is judged on the net weight (see "Bug fixed").
- No Wi-Fi setup typed on the screen (no keyboard, no network list): the phone steps stay.
- The screen never shows a password.
- No QR code.

## Things that would make it a failure

- A board that no longer boots, weighs, saves or drives its relays after the flash.
- Trade wording changed: `TRADE CHECKS ON`, `NOT FOR TRADE`, the plate line, `AUDIT n`,
  `NET` / `NET (TARE)` keep their exact words.
- A notice that hides the weight or the Hi-Lo bar.
- A screen colour that disagrees with the relays.

## Approaches considered

| | What | Trade-off |
|---|---|---|
| A (chosen) | One small drawing engine in `wsoft_core` (rounded boxes, discs, 4-bit letters into a 16-bit buffer) and one paint function per screen row. The Waveshare draws into its PSRAM frame; the yellow board draws 16-line bands and pushes each over SPI. Letters are generated from Manrope and Sora at each board's size. | Most new code, but all of it is pure C++ tested on the host against a reference drawing, both boards look the same, lower case and every ASCII sign work on the Waveshare, no flicker on the yellow board. About 28 KB of letters on the yellow board, 100 KB on the Waveshare. |
| B | Keep each board's own drawing (TFT_eSPI fonts on the yellow board, the two bitmap fonts and the background picture on the Waveshare) and only move things around. | Least new code. The Waveshare font has capitals only and one size too big for the small rows; the two boards drift apart; nothing is testable off the board. |
| C | LVGL. | A full UI toolkit for five pages: about 250 KB more flash, a draw buffer the yellow board (no PSRAM) can hardly spare, a new dependency to keep in step with two panel drivers. |

Rhyno talks in outcomes, so the how is the builder's call: A.

## The screen

Layout is in 320 x 240 units. A panel scales it: yellow board 1 x 1, Waveshare 2.5 x 2
(letters 2 x). `tools/wsoft-ui/ref.js` is the reference drawing: it holds every position,
size and colour, and the firmware paint code is its line-for-line port. Pictures of all test
screens: `docs/img/wsoft-screen/`. The mock-up page Rhyno checks is built from the same file.

Live page, top to bottom:

| Row (units) | Shows |
|---|---|
| Status bar 0-20 | Wi-Fi bars (from RSSI) and the IP, or `NO WI-FI` in red; `HUB` with a dot (green online, red with its state when not, grey `HUB SETUP`); the scale source (`RS485` / `RS232` / `WRITER`) with a dot (green, red no signal, yellow alarm) |
| Top 20-62 | Trade chip (`TRADE CHECKS ON` green / `NOT FOR TRADE` red), why or `SEQ n` or `APPROVAL OFF`; `STABLE` / `UNSTABLE` / `ALARM` / `NO SIGNAL` with a dot; the plate line and `AUDIT n`. Replaced by one band when needed, first match wins: a red save notice, the Wi-Fi alert, a yellow save notice |
| Weight 62-122 | `NET` or `NET (TARE)`, the net weight in the big letters, the unit. The panel is filled yellow / green / red by the relay zone |
| Hi-Lo 122-188 | The word (`TOO LOW` / `OK` / `TOO HIGH`, else `WAITING`, `ALARM`, `NO PRODUCT`, `OUT OF RANGE`, `EMPTY`), the product and its target, the bar (LOW yellow, OK green, HIGH red; limits at 6/22 and 16/22 like the Edge band) with the marker and the two limits |
| Bottom 188-240 | `GROSS` read-out, `MENU` button, `SAVE` button with its result line |

Pages under the status bar: MENU (tiles that show their current value: APPROVAL, AUTO SAVE,
TARE and PRESET TARE on RS485, PRODUCT, SCREEN dark/light, WI-FI, and BACK), the PIN pad
(phone layout, DEL, BACK, OK), the no-PIN note, the Wi-Fi page (connected: the address;
not connected: three phone steps and BACK).

Rules:

- The zone for the panel colour and the word is `relay_zone()`, the zone the relays are
  driven with, so screen and relays always agree.
- Dark mode is the default; MENU > SCREEN switches and stores it (NVS `wsoft_ui` / `dark`).
- The Wi-Fi alert shows 15 s after start while Wi-Fi is down; X hides it until Wi-Fi has been
  up again. Tapping the Wi-Fi part of the status bar opens the Wi-Fi page at any time. A red
  `CLOCK NOT SET` notice opens it too while Wi-Fi is down.
- There is no retry button: the framework's Wi-Fi service (`WiFiSettingsService`) already tries
  its saved network again as soon as an attempt fails, so a manual retry would add nothing.
  The Wi-Fi page says so and turns into the connected page by itself.
- Pages other than the live page close after 30 s without a tap (the Wi-Fi page after 3 min).

## How it is built

- `lib/wsoft_core/src/wsoft_gfx.*`: the drawing engine, no Arduino. Colours 5-6-5, blended
  per channel; rounded corners by coverage; letters 4-bit coverage, no kerning. It draws
  into a window of panel lines, so the same calls fill a whole frame or one band.
- `lib/wsoft_core/src/wsoft_ui_paint.*`: `WsoftUiModel` (every word and state on the screen),
  one paint function per row, the tap boxes. Pure.
- `lib/wsoft_core/src/gen/`: generated letters (1x, 2x), colours and the test screens.
- `lib/wsoft_esp/src/wsoft_ui.*`: fills the model from the services each poll, repaints the
  rows whose model part changed, pages and taps, the panel and touch drivers of each board.
  `wsoft_screen.cpp` keeps the scale poll, the jobs and the snapshot and hands over one view.
- USB technician commands for checks without a person at the board: `SCREEN CRC` (paints
  every test screen and prints its CRC), `SCREEN SHOT` (the screen as it is now, run-length
  text), `SCREEN SHOT n` (test screen n), `SCREEN PIXEL x y` (one dot read back from the
  yellow board's panel), `SCREEN TAP x y` (a tap at layout position x, y). Bench use: the
  weigh loop waits while a picture is sent.
- `screen_rle.h` (the Waveshare background picture) and `fonts.h` go.

Bug fixed on the way: the relay zone was judged on the gross weight while the screen word and
the saved weighing use the net weight. With a tare on they disagreed. The relays now get the
net weight too.

## Tests

- Host (`pio test -e native`, `test/test_wsoft_ui`): each test screen is painted on both panel
  sizes, whole-frame and in bands, and its CRC-32 must equal the reference drawing's
  (`golden.h`, written by `tools/wsoft-ui/gen.mjs`). Unit tests for blend, clip and the marker
  position.
- On each board after the flash: `SCREEN CRC` must print the same CRCs; `SCREEN SHOT`
  must decode to the expected live screen; `SCREEN TAP` walks MENU, SCREEN, WI-FI and back.

## Builder's calls (Rhyno can change any of them)

- Bottom order GROSS, MENU, SAVE, as in the mock-up he approved.
- An empty scale shows no colour and the word `EMPTY`, because the relays are off then.
- The SAVE result line is in capitals; the notice sentence keeps its mixed case.
- Menu tiles show the value, not "TAP FOR OFF".
- Status bar says `HUB` when online and the full state only when not.
- Letters: Manrope (words) and Sora (weight), both SIL Open Font License 1.1.

## Status log

- 13 Nov 2026 20:26: Rhyno approved the look and asked for the Hi-Lo colour change.
- 13 Nov 2026 20:31: Rhyno: build it and flash both boards tonight.
- 13 Nov 2026 (build): TRY AGAIN dropped from the Wi-Fi page after reading the framework's
  reconnect loop; long texts are cut to their room (`WsoftCanvas::fit`); 20 test screens.
