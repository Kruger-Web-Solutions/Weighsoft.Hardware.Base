#include "wsoft_ui_paint.h"

#include "gen/wsoft_ui_colors.h"

// Line-for-line with the paint functions in tools/wsoft-ui/ref.js. Every number here is a
// position in the 320 x 240 layout. Keep the order of every sum and product the same there
// and here, or a dot lands differently and test_wsoft_ui fails.

typedef WsoftUiTheme Theme;
static const WsoftUiBar &BAR = WSOFT_UI_BAR;
static const uint16_t WHITE = WSOFT_UI_WHITE;
static const uint16_t DARK = WSOFT_UI_DARK_INK;

enum { TINY = WSOFT_UI_TINY, SMALL = WSOFT_UI_SMALL, MID = WSOFT_UI_MID, BIG = WSOFT_UI_BIG, HERO = WSOFT_UI_HERO };

// Room for the longest text of the model, cut to fit its place (WsoftCanvas::fit).
static const size_t CUT_SIZE = 72;

// The colour of a word, and of the dot beside it.
static uint16_t tone(const Theme &T, int t) {
  return t == WSOFT_UI_TONE_OK ? T.ok_ink : t == WSOFT_UI_TONE_WAIT ? T.wait_ink : t == WSOFT_UI_TONE_BAD ? T.bad_ink : T.soft;
}

static uint16_t dot(const Theme &T, int t) {
  return t == WSOFT_UI_TONE_OK ? T.ok : t == WSOFT_UI_TONE_WAIT ? T.wait : t == WSOFT_UI_TONE_BAD ? T.bad : T.line;
}

static uint16_t bar_tone(int t) {
  return t == WSOFT_UI_TONE_OK ? BAR.ok : t == WSOFT_UI_TONE_WAIT ? BAR.warn : t == WSOFT_UI_TONE_BAD ? BAR.bad : BAR.mute;
}

// ---------- the live page, row by row ----------

static void paint_bar(WsoftCanvas &c, const WsoftUiModel &m) {
  c.rect(0, 0, 320, 20, BAR.bg);
  c.rect(0, 19, 320, 1, BAR.line);
  for (int i = 0; i < 4; i++) {
    int h = 4 + i * 3;
    c.rect(8 + i * 5, 15 - h, 3, h, m.wifi_up && i < m.wifi_bars ? BAR.ok : BAR.dim);
  }
  c.text(m.ip, 32, 10, SMALL, m.wifi_up ? BAR.ink : BAR.bad);
  double x = 312;
  x -= c.text(m.link, x, 10, SMALL, m.link_tone == WSOFT_UI_TONE_OK ? BAR.ink : bar_tone(m.link_tone), WSOFT_RIGHT);
  c.disc(x - 7, 10, 3.5, bar_tone(m.link_tone));
  x -= 20;
  x -= c.text(m.hub, x, 10, SMALL, m.hub_tone == WSOFT_UI_TONE_OK ? BAR.ink : bar_tone(m.hub_tone), WSOFT_RIGHT);
  c.disc(x - 7, 10, 3.5, bar_tone(m.hub_tone));
}

static void paint_top(WsoftCanvas &c, const WsoftUiModel &m, const Theme &T) {
  char cut[CUT_SIZE];
  c.rect(0, 20, 320, 42, T.bg);
  if (m.slot == WSOFT_UI_SLOT_WIFI) {
    c.rect(6, 22, 308, 40, T.wait, 8);
    c.text("WI-FI IS OFF", 18, 42, MID, DARK);
    c.rect(160, 26, 100, 32, DARK, 6);
    c.text("CONNECT", 210, 42, SMALL, WHITE, WSOFT_CENTER);
    c.rect(266, 26, 42, 32, DARK, 6);
    c.rect(267.5, 27.5, 39, 29, T.wait, 5);
    c.text("X", 287, 42, MID, DARK, WSOFT_CENTER);
    return;
  }
  if (m.slot != WSOFT_UI_SLOT_NONE) {
    bool stop = m.slot == WSOFT_UI_SLOT_STOP;
    uint16_t ink = stop ? WHITE : DARK;
    c.rect(6, 22, 308, 40, stop ? T.stop : T.wait, 8);
    if (c.width(m.slot_title, BIG) <= 292) c.text(m.slot_title, 160, 34, BIG, ink, WSOFT_CENTER);
    else c.text(c.fit(m.slot_title, MID, 296, cut, sizeof cut), 160, 34, MID, ink, WSOFT_CENTER);
    c.text(c.fit(m.slot_detail, SMALL, 296, cut, sizeof cut), 160, 53, SMALL, ink, WSOFT_CENTER);
    return;
  }
  const char *chip = m.trade_ok ? "TRADE CHECKS ON" : "NOT FOR TRADE";
  double w = c.width(chip, SMALL) + 14;
  c.rect(8, 24, w, 17, m.trade_ok ? T.ok : T.bad, 4);
  c.rect(9, 25, w - 2, 15, m.trade_ok ? T.chip_ok : T.chip_bad, 3);
  c.text(chip, 8 + w / 2, 32.5, SMALL, m.trade_ok ? T.ok_ink : T.bad_ink, WSOFT_CENTER);
  double sw = c.text(m.steady, 312, 32.5, SMALL, tone(T, m.steady_tone), WSOFT_RIGHT);
  c.disc(312 - sw - 8, 32.5, 4, dot(T, m.steady_tone));
  c.text(c.fit(m.reason, TINY, 312 - sw - 20 - (8 + w + 7), cut, sizeof cut), 8 + w + 7, 32.5, TINY,
         tone(T, m.reason_tone));
  double aw = c.text(m.audit, 312, 52, TINY, T.soft, WSOFT_RIGHT);
  c.text(c.fit(m.plate, TINY, 312 - aw - 8 - 10, cut, sizeof cut), 10, 52, TINY, T.soft);
}

// The weight panel takes the colour of the Hi-Lo zone the relays are in: yellow LOW, green
// OK, red HIGH. No zone (no product, empty scale, no signal): the plain background.
static void paint_weight(WsoftCanvas &c, const WsoftUiModel &m, const Theme &T) {
  c.rect(0, 62, 320, 60, T.bg);
  uint16_t ink = m.live ? T.ink : T.soft, soft = T.soft;
  if (m.zone != WSOFT_UI_ZONE_NONE) {
    c.rect(6, 64, 308, 56, m.zone == WSOFT_UI_ZONE_LOW ? T.z_low : m.zone == WSOFT_UI_ZONE_OK ? T.z_ok : T.z_high, 8);
    ink = soft = m.zone == WSOFT_UI_ZONE_LOW ? T.z_low_ink : m.zone == WSOFT_UI_ZONE_OK ? T.z_ok_ink : T.z_high_ink;
  }
  c.text(m.label, 14, 72, TINY, soft);
  c.text(m.weight, 268, 95, c.width(m.weight, HERO) <= 250 ? HERO : BIG, ink, WSOFT_RIGHT);
  c.text(m.unit, 274, 106, MID, soft);
}

static void paint_hilo(WsoftCanvas &c, const WsoftUiModel &m, const Theme &T) {
  char cut[CUT_SIZE];
  c.rect(0, 122, 320, 66, T.bg);
  c.disc(16, 137, 5, dot(T, m.word_tone));
  double ww = c.text(m.word, 28, 137, m.product_on ? BIG : MID, tone(T, m.word_tone));
  // What is left of the line for the product, right of the word.
  double room = 310 - (28 + ww + 10);
  const double bx = 10, bw = 300, by = 157, bh = 12;
  if (!m.product_on) {
    c.text(c.fit(m.target, TINY, room, cut, sizeof cut), 310, 137, TINY, T.soft, WSOFT_RIGHT);
    c.rect(bx, by, bw, bh, T.line, -1);
    return;
  }
  c.text(c.fit(m.product, SMALL, room, cut, sizeof cut), 310, 129.5, SMALL, T.ink, WSOFT_RIGHT);
  c.text(c.fit(m.target, TINY, room, cut, sizeof cut), 310, 143, TINY, T.soft, WSOFT_RIGHT);
  // The bar: LOW, OK and HIGH. The limits sit 6/22 and 16/22 along it, as on the Edge.
  double lo = bx + bw * 6 / 22, hi = bx + bw * 16 / 22;
  c.rect(bx, by, bw, bh, T.bad, -1);
  c.rect(bx, by, lo - bx + bh, bh, T.wait, -1);
  c.rect(lo, by, hi - lo, bh, T.ok);
  c.text(m.low, lo, 181, TINY, T.soft, WSOFT_CENTER);
  c.text(m.high, hi, 181, TINY, T.soft, WSOFT_CENTER);
  if (m.pointer < 0) return;
  double mx = bx + bw * m.pointer / 1000;
  if (mx < bx + 4) mx = bx + 4;
  if (mx > bx + bw - 4) mx = bx + bw - 4;
  c.rect(mx - 6, by - 7, 12, bh + 14, T.bg, -1);
  c.rect(mx - 4, by - 5, 8, bh + 10, T.pointer, -1);
}

static void paint_bottom(WsoftCanvas &c, const WsoftUiModel &m, const Theme &T) {
  char cut[CUT_SIZE];
  c.rect(0, 188, 320, 52, T.bg);
  c.rect(8, 190, 104, 46, T.line, 8);
  c.rect(9, 191, 102, 44, T.panel, 7);
  c.text("GROSS", 17, 202, TINY, T.soft);
  if (c.width(m.gross, MID) <= 88) c.text(m.gross, 17, 221, MID, m.live ? T.ink : T.soft);
  else c.text(c.fit(m.gross, SMALL, 88, cut, sizeof cut), 17, 221, SMALL, m.live ? T.ink : T.soft);
  c.rect(120, 190, 72, 46, T.btn, 8);
  for (int i = 0; i < 3; i++) c.rect(144, 200 + i * 6, 24, 3, T.btn_ink, 1);
  c.text("MENU", 156, 226, SMALL, T.btn_ink, WSOFT_CENTER);
  c.rect(200, 190, 112, 46, m.save_look == WSOFT_UI_LOOK_ON ? T.ok : m.save_look == WSOFT_UI_LOOK_BAD ? T.stop : T.btn, 8);
  uint16_t ink = m.save_look == WSOFT_UI_LOOK_BTN ? T.btn_ink : WHITE;
  c.text("SAVE", 256, 206, MID, ink, WSOFT_CENTER);
  if (c.width(m.save_line, SMALL) <= 104) c.text(m.save_line, 256, 225, SMALL, ink, WSOFT_CENTER);
  else c.text(c.fit(m.save_line, TINY, 104, cut, sizeof cut), 256, 225, TINY, ink, WSOFT_CENTER);
}

// ---------- the menu pages (everything under the status bar) ----------

static void paint_tile(WsoftCanvas &c, const Theme &T, const WsoftUiBox &box, const char *caption, const char *value,
                       int look) {
  double x = box.x, y = box.y, w = box.w, h = box.h;
  bool strong = look == WSOFT_UI_LOOK_ON || look == WSOFT_UI_LOOK_ACCENT;
  if (look == WSOFT_UI_LOOK_PLAIN) {
    c.rect(x, y, w, h, T.line, 8);
    c.rect(x + 1, y + 1, w - 2, h - 2, T.panel, 7);
  } else {
    c.rect(x, y, w, h, look == WSOFT_UI_LOOK_ON ? T.ok : look == WSOFT_UI_LOOK_ACCENT ? T.accent : T.btn, 8);
  }
  uint16_t ink = strong ? WHITE : look == WSOFT_UI_LOOK_PLAIN ? T.ink : T.btn_ink;
  if (caption[0]) {
    char cut[CUT_SIZE];
    c.text(caption, x + 10, y + 13, TINY, strong ? WHITE : T.soft);
    if (c.width(value, MID) <= w - 20) c.text(value, x + 10, y + 30, MID, ink);
    else c.text(c.fit(value, SMALL, w - 20, cut, sizeof cut), x + 10, y + 30, SMALL, ink);
  } else {
    c.text(value, x + w / 2, y + h / 2, MID, ink, WSOFT_CENTER);
  }
}

static void paint_page(WsoftCanvas &c, const WsoftUiModel &m, const Theme &T) {
  char cut[CUT_SIZE];
  c.rect(0, 20, 320, 220, T.bg);
  double tw = c.text(m.title, 10, 32, SMALL, T.ink);
  c.text(c.fit(m.message, SMALL, 310 - (10 + tw + 12 + m.pin_count * 14), cut, sizeof cut), 310, 32, SMALL, T.wait_ink,
         WSOFT_RIGHT);
  if (m.page == WSOFT_UI_PAGE_MENU) {
    for (int i = 0; i < m.tile_count && i < WSOFT_UI_TILES_MAX; i++) {
      paint_tile(c, T, wsoft_ui_tile_box(i), m.tiles[i].caption, m.tiles[i].value, m.tiles[i].look);
    }
    paint_tile(c, T, WSOFT_UI_BOX_BACK, "", "BACK", WSOFT_UI_LOOK_PLAIN);
  } else if (m.page == WSOFT_UI_PAGE_PIN) {
    for (int i = 0; i < m.pin_count; i++) c.disc(100 + i * 14, 32, 4, T.ink);
    for (int i = 0; i < 9; i++) {
      char digit[2] = {(char)('1' + i), 0};
      paint_tile(c, T, wsoft_ui_key_box(i % 3, i / 3, 1, 1), "", digit, WSOFT_UI_LOOK_BTN);
    }
    paint_tile(c, T, wsoft_ui_key_box(0, 3, 3, 1), "", "0", WSOFT_UI_LOOK_BTN);
    paint_tile(c, T, wsoft_ui_key_box(3, 0, 1, 1), "", "DEL", WSOFT_UI_LOOK_BTN);
    paint_tile(c, T, wsoft_ui_key_box(3, 1, 1, 1), "", "BACK", WSOFT_UI_LOOK_PLAIN);
    paint_tile(c, T, wsoft_ui_key_box(3, 2, 1, 2), "", "OK", WSOFT_UI_LOOK_ON);
  } else if (m.page == WSOFT_UI_PAGE_NO_PIN) {
    c.text("SET A SCREEN PIN FIRST", 160, 62, MID, T.bad_ink, WSOFT_CENTER);
    c.text("On a phone or PC, open", 160, 90, SMALL, T.soft, WSOFT_CENTER);
    c.text(m.address, 160, 112, MID, T.ink, WSOFT_CENTER);
    c.text("Log in as admin, then", 160, 140, SMALL, T.soft, WSOFT_CENTER);
    c.text("W-Soft  >  Settings  >  Approval PIN", 160, 160, SMALL, T.ink, WSOFT_CENTER);
    paint_tile(c, T, WSOFT_UI_BOX_WIDE, "", "BACK", WSOFT_UI_LOOK_PLAIN);
  } else if (m.wifi_up) {
    c.text("CONNECTED", 160, 84, BIG, T.ok_ink, WSOFT_CENTER);
    c.text(m.ip, 160, 118, MID, T.ink, WSOFT_CENTER);
    c.text("Type this address in a browser on the same Wi-Fi", 160, 144, TINY, T.soft, WSOFT_CENTER);
    paint_tile(c, T, WSOFT_UI_BOX_WIDE, "", "BACK", WSOFT_UI_LOOK_PLAIN);
  } else {
    for (int i = 0; i < 3; i++) {
      char number[2] = {(char)('1' + i), 0};
      c.disc(22, 58 + i * 40, 10, T.accent);
      c.text(number, 22, 58 + i * 40, SMALL, WHITE, WSOFT_CENTER);
    }
    c.text("On your phone, join this Wi-Fi", 42, 50, TINY, T.soft);
    if (c.width(m.ap_name, MID) <= 268) c.text(m.ap_name, 42, 67, MID, T.ink);
    else c.text(c.fit(m.ap_name, SMALL, 268, cut, sizeof cut), 42, 67, SMALL, T.ink);
    c.text("Then open this address", 42, 90, TINY, T.soft);
    c.text(m.address, 42, 107, MID, T.ink);
    c.text("Log in, tap Wi-Fi and pick yours", 42, 138, SMALL, T.ink);
    c.text("The board keeps looking for its saved Wi-Fi by itself", 42, 170, TINY, T.wait_ink);
    paint_tile(c, T, WSOFT_UI_BOX_WIDE, "", "BACK", WSOFT_UI_LOOK_PLAIN);
  }
}

// ---------- rows ----------

WsoftUiSpan wsoft_ui_row_span(int row) {
  static const WsoftUiSpan SPANS[WSOFT_UI_ROW_COUNT] = {{0, 20}, {20, 62}, {62, 122}, {122, 188}, {188, 240}, {20, 240}};
  if (row < 0 || row >= WSOFT_UI_ROW_COUNT) return WsoftUiSpan{0, 0};
  return SPANS[row];
}

bool wsoft_ui_row_on_page(int row, int page) {
  if (row == WSOFT_UI_ROW_BAR) return true;
  if (row == WSOFT_UI_ROW_PAGE) return page != WSOFT_UI_PAGE_LIVE;
  return row > WSOFT_UI_ROW_BAR && row < WSOFT_UI_ROW_PAGE && page == WSOFT_UI_PAGE_LIVE;
}

// The model's fields are in the order of the rows, so a row's key is the CRC of its stretch
// of the model, plus the mode and page that every row depends on.
static uint32_t stretch(uint32_t crc, const WsoftUiModel &m, size_t from, size_t to) {
  return wsoft_crc32(crc, (const uint8_t *)&m + from, to - from);
}

uint32_t wsoft_ui_row_key(const WsoftUiModel &m, int row) {
  const size_t bar = offsetof(WsoftUiModel, wifi_up), top = offsetof(WsoftUiModel, slot);
  const size_t weight = offsetof(WsoftUiModel, label), hilo = offsetof(WsoftUiModel, word);
  const size_t bottom = offsetof(WsoftUiModel, gross), page = offsetof(WsoftUiModel, title);
  uint32_t key = stretch(0, m, 0, bar);  // dark, page
  switch (row) {
    case WSOFT_UI_ROW_BAR: return stretch(key, m, bar, top);
    case WSOFT_UI_ROW_TOP: return stretch(key, m, top, weight);
    case WSOFT_UI_ROW_WEIGHT: return stretch(key, m, weight, hilo);
    case WSOFT_UI_ROW_HILO: return stretch(key, m, hilo, bottom);
    case WSOFT_UI_ROW_BOTTOM:
      // GROSS is greyed when no weight arrives.
      key = stretch(key, m, bottom, page);
      return stretch(key, m, offsetof(WsoftUiModel, live), offsetof(WsoftUiModel, live) + sizeof m.live);
    case WSOFT_UI_ROW_PAGE:
      // The Wi-Fi page shows the address from the status bar's part.
      key = stretch(key, m, page, sizeof m);
      return stretch(key, m, bar, top);
    default: return 0;
  }
}

void wsoft_ui_paint_row(WsoftCanvas &c, const WsoftUiModel &m, int row) {
  const Theme &T = m.dark ? WSOFT_UI_DARK : WSOFT_UI_LIGHT;
  switch (row) {
    case WSOFT_UI_ROW_BAR: paint_bar(c, m); break;
    case WSOFT_UI_ROW_TOP: paint_top(c, m, T); break;
    case WSOFT_UI_ROW_WEIGHT: paint_weight(c, m, T); break;
    case WSOFT_UI_ROW_HILO: paint_hilo(c, m, T); break;
    case WSOFT_UI_ROW_BOTTOM: paint_bottom(c, m, T); break;
    case WSOFT_UI_ROW_PAGE: paint_page(c, m, T); break;
    default: break;
  }
}

void wsoft_ui_paint(WsoftCanvas &c, const WsoftUiModel &m) {
  for (int row = 0; row < WSOFT_UI_ROW_COUNT; row++) {
    if (wsoft_ui_row_on_page(row, m.page)) wsoft_ui_paint_row(c, m, row);
  }
}

// ---------- taps and the marker ----------

WsoftUiBox wsoft_ui_tile_box(int i) {
  return WsoftUiBox{(int16_t)(i % 2 ? 164 : 8), (int16_t)(46 + (i >> 1) * 48), 148, 44};
}

WsoftUiBox wsoft_ui_key_box(int col, int row, int cols, int rows) {
  static const int16_t KEY_X[4] = {8, 86, 164, 242};
  if (col < 0 || col > 3) col = 0;
  return WsoftUiBox{KEY_X[col], (int16_t)(46 + row * 48), (int16_t)(72 * cols + 6 * (cols - 1)),
                    (int16_t)(44 * rows + 4 * (rows - 1))};
}

int wsoft_ui_pointer(int64_t g, int64_t low, int64_t high) {
  int64_t span = high - low;
  if (span <= 0) return g < low ? 0 : g > high ? 1000 : 500;
  int64_t num = (g - low) * 10 + 6 * span, den = 22 * span;
  if (num <= 0) return 0;
  if (num >= den) return 1000;
  return (int)((1000 * num + den / 2) / den);
}
