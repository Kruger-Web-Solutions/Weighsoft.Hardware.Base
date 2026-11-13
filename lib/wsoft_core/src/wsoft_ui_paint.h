#pragma once

// The W-Soft board screen: what goes where. Pure C++ (no Arduino), tested on the host against
// the reference drawing tools/wsoft-ui/ref.js (test/test_wsoft_ui).
//
// The board code (lib/wsoft_esp/src/wsoft_ui.cpp) fills a WsoftUiModel with every word and
// state the screen shows; the functions here paint it. Nothing in this file reads a service,
// so a model always paints the same dots.
//
// One layout, 320 x 240 units, for every board. The live page is five rows; each paints its
// own background first, so the board repaints only the rows whose part of the model changed:
//   status bar 0-20    Wi-Fi bars and IP, Hub, scale link
//   top 20-62          trade chip and why, steady word, plate, audit; or one band: a red or
//                      yellow save notice, or the Wi-Fi alert
//   weight 62-122      NET weight; the panel is yellow / green / red with the Hi-Lo relays
//   Hi-Lo 122-188      the word, product and target, the LOW / OK / HIGH bar with its marker
//   bottom 188-240     GROSS read-out, MENU, SAVE
// The menu pages (MENU, PIN pad, the no-PIN note, Wi-Fi) are the status bar plus one page row.

#include <stddef.h>
#include <stdint.h>

#include "wsoft_gfx.h"

// Font numbers for WsoftCanvas, in the order of the generated WSOFT_UI_FONTS_1X / _2X.
enum { WSOFT_UI_TINY, WSOFT_UI_SMALL, WSOFT_UI_MID, WSOFT_UI_BIG, WSOFT_UI_HERO, WSOFT_UI_FONT_COUNT };

enum { WSOFT_UI_PAGE_LIVE, WSOFT_UI_PAGE_MENU, WSOFT_UI_PAGE_PIN, WSOFT_UI_PAGE_NO_PIN, WSOFT_UI_PAGE_WIFI };
// How a word is coloured: plain, good (green), wait (yellow) or bad (red).
enum { WSOFT_UI_TONE_SOFT, WSOFT_UI_TONE_OK, WSOFT_UI_TONE_WAIT, WSOFT_UI_TONE_BAD };
// What the band above the weight shows instead of the trade row.
enum { WSOFT_UI_SLOT_NONE, WSOFT_UI_SLOT_STOP, WSOFT_UI_SLOT_WAIT, WSOFT_UI_SLOT_WIFI };
// The Hi-Lo zone that colours the weight panel: the same numbers as WsoftRelayZone.
enum { WSOFT_UI_ZONE_NONE, WSOFT_UI_ZONE_LOW, WSOFT_UI_ZONE_OK, WSOFT_UI_ZONE_HIGH };
// How a button looks: plain button, on (green), accent (blue), outlined, or bad (red, SAVE only).
enum { WSOFT_UI_LOOK_BTN, WSOFT_UI_LOOK_ON, WSOFT_UI_LOOK_ACCENT, WSOFT_UI_LOOK_PLAIN, WSOFT_UI_LOOK_BAD };
enum {
  WSOFT_UI_TILE_NONE,
  WSOFT_UI_TILE_APPROVAL,
  WSOFT_UI_TILE_AUTO,
  WSOFT_UI_TILE_TARE,
  WSOFT_UI_TILE_PRESET,
  WSOFT_UI_TILE_PRODUCT,
  WSOFT_UI_TILE_THEME,
  WSOFT_UI_TILE_WIFI
};

static const int WSOFT_UI_TILES_MAX = 7;

// One menu button: a small caption and the value it is set to now.
struct WsoftUiTile {
  uint8_t id;
  uint8_t look;
  char caption[14];
  char value[26];
};

// Every word and state on the screen. The board zero-fills it before filling it in, so two
// models with the same content are the same bytes.
struct WsoftUiModel {
  bool dark;
  uint8_t page;
  // status bar
  bool wifi_up;
  uint8_t wifi_bars;  // 0..4
  char ip[16];        // the address, or "NO WI-FI"
  char hub[14];
  uint8_t hub_tone;
  char link[8];       // where the weight comes from: RS485, RS232, WRITER
  uint8_t link_tone;
  // top row
  uint8_t slot;
  char slot_title[24];
  char slot_detail[36];
  bool trade_ok;
  char reason[18];
  uint8_t reason_tone;
  char steady[10];
  uint8_t steady_tone;
  char plate[64];
  char audit[18];
  // weight row
  char label[12];
  char weight[14];
  bool live;          // a weight is arriving (else the number is greyed)
  char unit[4];
  uint8_t zone;
  // Hi-Lo row
  char word[14];
  uint8_t word_tone;
  bool product_on;
  char product[26];
  char target[26];    // "TARGET 50 KG", or the hint when no product is picked
  char low[12];
  char high[12];
  int16_t pointer;    // marker on the bar, 0..1000; below 0 = no marker
  // bottom row
  char gross[22];
  char save_line[18];
  uint8_t save_look;
  // menu pages
  char title[12];
  char message[40];
  uint8_t tile_count;
  WsoftUiTile tiles[WSOFT_UI_TILES_MAX];
  uint8_t pin_count;  // digits typed on the PIN pad
  char address[20];   // the address to open: on the no-PIN note and the Wi-Fi page
  char ap_name[34];   // the board's own hotspot
};

enum {
  WSOFT_UI_ROW_BAR,
  WSOFT_UI_ROW_TOP,
  WSOFT_UI_ROW_WEIGHT,
  WSOFT_UI_ROW_HILO,
  WSOFT_UI_ROW_BOTTOM,
  WSOFT_UI_ROW_PAGE,
  WSOFT_UI_ROW_COUNT
};

// The layout lines a row covers: y0 up to, not including, y1.
struct WsoftUiSpan {
  int y0, y1;
};
WsoftUiSpan wsoft_ui_row_span(int row);
// The live page is the bar and the four live rows; any other page is the bar and the page row.
bool wsoft_ui_row_on_page(int row, int page);

// A number that changes when what the row draws changes: the board paints a row again only
// when its key moved. The model must have been zero-filled before it was filled in.
uint32_t wsoft_ui_row_key(const WsoftUiModel &model, int row);

// Paints one row into the canvas's window. Dots of the row outside the window are skipped.
void wsoft_ui_paint_row(WsoftCanvas &canvas, const WsoftUiModel &model, int row);
// Paints every row of the model's page.
void wsoft_ui_paint(WsoftCanvas &canvas, const WsoftUiModel &model);

// A box a finger can tap, in layout units. A tap counts up to 3 units outside it.
struct WsoftUiBox {
  int16_t x, y, w, h;
  bool hit(int px, int py) const { return px >= x - 3 && px < x + w + 3 && py >= y - 3 && py < y + h + 3; }
};
static const WsoftUiBox WSOFT_UI_BOX_WIFI_BAR = {0, 0, 150, 20};   // the Wi-Fi part of the status bar
static const WsoftUiBox WSOFT_UI_BOX_SLOT = {6, 22, 308, 40};      // the notice band
static const WsoftUiBox WSOFT_UI_BOX_CONNECT = {160, 26, 100, 32}; // on the Wi-Fi alert
static const WsoftUiBox WSOFT_UI_BOX_DISMISS = {266, 26, 42, 32};
static const WsoftUiBox WSOFT_UI_BOX_MENU = {120, 190, 72, 46};
static const WsoftUiBox WSOFT_UI_BOX_SAVE = {200, 190, 112, 46};
static const WsoftUiBox WSOFT_UI_BOX_BACK = {164, 190, 148, 44};   // bottom right of a page
static const WsoftUiBox WSOFT_UI_BOX_WIDE = {8, 190, 304, 44};     // one button across a page
// Menu tile i: two columns, rows of 48.
WsoftUiBox wsoft_ui_tile_box(int i);
// A PIN pad key: column 0..3, row 0..3, spanning cols x rows keys.
WsoftUiBox wsoft_ui_key_box(int col, int row, int cols, int rows);

// Where the marker sits on the Hi-Lo bar, 0..1000, for a net weight of g grams: the bar runs
// 0.6 of the span past each limit (the Edge's band), so low is at 6/22 and high at 16/22.
int wsoft_ui_pointer(int64_t g, int64_t low, int64_t high);
