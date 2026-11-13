#pragma once

// The W-Soft screen on a board (Rhyno, 13 Nov 2026: one layout in the Edge look for every
// board, light and dark, the IP and Wi-Fi always shown, the weight panel coloured with the
// Hi-Lo relays).
//
// Three layers, two of them pure C++ in wsoft_core and tested on the host (test_wsoft_ui):
//   wsoft_ui_model   from the facts (scale, trade, Hi-Lo, save state, Wi-Fi, Hub) to the words
//   wsoft_ui_paint   the words onto a canvas, row by row, in a 320 x 240 layout
//   wsoft_gfx        the canvas: boxes, discs and letters into 16-bit dots
// This file joins them to a board: it reads the facts from the services every poll, paints the
// rows that changed, runs the pages and the taps, and holds each board's panel and touch:
//   yellow board (CYD)  no memory for a frame: each row is drawn in 16-line bands, pushed over SPI
//   Waveshare 4.3"      a frame in PSRAM, pushed whole
//   screenless S3       nothing to draw; every call here does nothing
// The screen as it must look: tools/wsoft-ui/ref.js and docs/img/wsoft-screen.

#include <Arduino.h>

#include "approval.h"
#include "scale.h"

// What the weigh loop hands the screen after every poll.
struct WsoftUiView {
  bool linked = false;   // a live weight is arriving
  bool alarm = false;    // the junction box reports a blocking alarm
  int32_t gross = 0;     // in decimals and unit
  int32_t net = 0;       // gross less the tare (the gross when no tare is on)
  bool tared = false;
  int decimals = 3;
  int unit = 1;          // 1 kg, 2 t, 3 g
  WsoftScaleHold hold = WSOFT_HOLD_NONE;
  TradeCheck check = TRADE_OFF;
  const char *source = "RS485";  // RS485, RS232 or WRITER (status bar)
  bool writer = false;   // Writer mode: this board never saves
};

// Starts the panel and touch. False when there is no panel (screenless board) or it failed.
bool ui_begin();
// Every poll: the screen paints the rows whose words changed.
void ui_show(const WsoftUiView &view);
// Every loop pass: taps, and the pages' way back to the live page when nobody taps.
void ui_loop();

// USB technician commands, for checking a board without standing at it:
//   SCREEN CRC        paints every test screen (gen/wsoft_ui_presets.h) and prints its CRC-32;
//                     they must equal test/test_wsoft_ui/golden.h
//   SCREEN SHOT       sends the screen as it is now (run-length text, tools/wsoft-ui/shot.ps1)
//   SCREEN SHOT n     sends test screen n
//   SCREEN TAP x y    a tap at layout position x, y (320 x 240)
// While a picture is sent the weigh loop waits, so the relays rest (their zone goes stale).
void ui_handle_command(const String &command, Stream &port);
