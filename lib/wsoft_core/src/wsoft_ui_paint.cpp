#include "wsoft_ui_paint.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "gen/wsoft_ui_colors.h"

// Line-for-line with the paint functions in tools/wsoft-ui/ref.js. Every number here is a
// position in the 320 x 240 layout. Keep the order of every sum and product the same there
// and here, or a dot lands differently and test_wsoft_ui fails.

typedef WsoftUiTheme Theme;
static const WsoftUiBar &BAR = WSOFT_UI_BAR;
static const uint16_t WHITE = WSOFT_UI_WHITE;
static const uint16_t DARK = WSOFT_UI_DARK_INK;

enum {
  TINY = WSOFT_UI_TINY,
  SMALL = WSOFT_UI_SMALL,
  MID = WSOFT_UI_MID,
  BIG = WSOFT_UI_BIG,
  HERO = WSOFT_UI_HERO,
  HUGE_ = WSOFT_UI_HUGE,  // HUGE is a macro of math.h
  XL = WSOFT_UI_XL
};

// Room for the longest text of the model, cut to fit its place (WsoftCanvas::fit).
static const size_t CUT_SIZE = 72;

// The Hi-Lo zone's fill and ink; `none` when there is no zone.
static uint16_t zone_fill(const Theme &T, int zone, uint16_t none) {
  return zone == WSOFT_UI_ZONE_LOW ? T.z_low : zone == WSOFT_UI_ZONE_OK ? T.z_ok : zone == WSOFT_UI_ZONE_HIGH ? T.z_high : none;
}
static uint16_t zone_ink(const Theme &T, int zone, uint16_t none) {
  return zone == WSOFT_UI_ZONE_LOW    ? T.z_low_ink
         : zone == WSOFT_UI_ZONE_OK   ? T.z_ok_ink
         : zone == WSOFT_UI_ZONE_HIGH ? T.z_high_ink
                                      : none;
}

// Text in `big` letters when it fits in `room`, else cut to fit in `small` letters.
static void text_fit(WsoftCanvas &c, const char *text, double x, double y, int big, int small, double room,
                     uint16_t ink, WsoftAlign align = WSOFT_LEFT) {
  char cut[CUT_SIZE];
  if (c.width(text, big) <= room) c.text(text, x, y, big, ink, align);
  else c.text(c.fit(text, small, room, cut, sizeof cut), x, y, small, ink, align);
}

// The words on the bottom row's left button (ZERO, or the GROSS read-out) and on SAVE. The
// classic and Full Colour screens draw the same words in their own colours.
static void bottom_left_words(WsoftCanvas &c, const WsoftUiModel &m, uint16_t zero_ink, uint16_t zero_line,
                              uint16_t gross_label, uint16_t gross_ink) {
  if (m.zero_btn) {
    c.text("ZERO", 60, 206, MID, zero_ink, WSOFT_CENTER);
    text_fit(c, m.zero_line, 60, 225, SMALL, TINY, 96, zero_line, WSOFT_CENTER);
  } else {
    c.text("GROSS", 17, 202, TINY, gross_label);
    text_fit(c, m.gross, 17, 221, MID, SMALL, 88, gross_ink);
  }
}
static void save_words(WsoftCanvas &c, const WsoftUiModel &m, uint16_t ink) {
  c.text("SAVE", 256, 206, MID, ink, WSOFT_CENTER);
  text_fit(c, m.save_line, 256, 225, SMALL, TINY, 104, ink, WSOFT_CENTER);
}

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

static void paint_bar(WsoftCanvas &c, const WsoftUiModel &m, uint16_t bg, uint16_t line) {
  c.rect(0, 0, 320, 20, bg);
  c.rect(0, 19, 320, 1, line);
  for (int i = 0; i < 4; i++) {
    int h = 4 + i * 3;
    c.rect(8 + i * 5, 15 - h, 3, h, m.wifi_up && i < m.wifi_bars ? BAR.ok : BAR.dim);
  }
  // The address is never cut: a cut address is another address. When the bar is too full,
  // the Hub's words give way and only its dot tells its state.
  bool full = 32 + c.width(m.ip, SMALL) <= 312 - c.width(m.link, SMALL) - 20 - c.width(m.hub, SMALL) - 16.5;
  double x = 312;
  x -= c.text(m.link, x, 10, SMALL, m.link_tone == WSOFT_UI_TONE_OK ? BAR.ink : bar_tone(m.link_tone), WSOFT_RIGHT);
  c.disc(x - 7, 10, 3.5, bar_tone(m.link_tone));
  x -= 20;
  x -= c.text(full ? m.hub : "HUB", x, 10, SMALL, m.hub_tone == WSOFT_UI_TONE_OK ? BAR.ink : bar_tone(m.hub_tone), WSOFT_RIGHT);
  c.disc(x - 7, 10, 3.5, bar_tone(m.hub_tone));
  c.text(m.ip, 32, 10, SMALL, m.wifi_up ? BAR.ink : BAR.bad);
}

const char *wsoft_ui_plate_line(const WsoftCanvas &c, const WsoftUiModel &m, double room, char *out, size_t size) {
  char sa[sizeof m.plate_sa];
  out[0] = 0;
  if (!m.plate_sa[0] && !m.plate_limits[0]) return out;
  snprintf(out, size, "%s   %s", m.plate_sa, m.plate_limits);
  if (c.width(out, TINY) <= room) return out;
  snprintf(out, size, "...   %s", m.plate_limits);
  c.fit(m.plate_sa, TINY, room - c.width(out, TINY), sa, sizeof sa);
  if (strlen(sa) > 3) {
    snprintf(out, size, "%s...   %s", sa, m.plate_limits);
    return out;
  }
  if (c.width(m.plate_limits, TINY) <= room) {
    snprintf(out, size, "%s", m.plate_limits);
    return out;
  }
  char cut[sizeof m.plate_limits];
  snprintf(cut, sizeof cut, "%s", m.plate_limits);
  for (;;) {
    snprintf(out, size, "%s ...", cut);
    if (cut[0] == 0 || c.width(out, TINY) <= room) return out;
    char *space = strrchr(cut, ' ');
    cut[space ? space - cut : 0] = 0;
  }
}

// The band over the trade row: the Wi-Fi alert or a save notice. False when there is none.
static bool paint_slot(WsoftCanvas &c, const WsoftUiModel &m, const Theme &T) {
  char cut[CUT_SIZE];
  if (m.slot == WSOFT_UI_SLOT_WIFI) {
    c.rect(6, 22, 308, 40, T.z_low, 8);
    c.text("WI-FI IS OFF", 18, 42, MID, T.z_low_ink);
    c.rect(160, 26, 100, 32, DARK, 6);
    c.text("CONNECT", 210, 42, SMALL, WHITE, WSOFT_CENTER);
    c.rect(266, 26, 42, 32, DARK, 6);
    c.rect(267.5, 27.5, 39, 29, T.z_low, 5);
    c.text("X", 287, 42, MID, T.z_low_ink, WSOFT_CENTER);
    return true;
  }
  if (m.slot != WSOFT_UI_SLOT_NONE) {
    bool stop = m.slot == WSOFT_UI_SLOT_STOP;
    uint16_t ink = stop ? WHITE : T.z_low_ink;
    c.rect(6, 22, 308, 40, stop ? T.stop : T.z_low, 8);
    if (c.width(m.slot_title, BIG) <= 292) c.text(m.slot_title, 160, 34, BIG, ink, WSOFT_CENTER);
    else c.text(c.fit(m.slot_title, MID, 296, cut, sizeof cut), 160, 34, MID, ink, WSOFT_CENTER);
    c.text(c.fit(m.slot_detail, SMALL, 296, cut, sizeof cut), 160, 53, SMALL, ink, WSOFT_CENTER);
    return true;
  }
  return false;
}

static void paint_top(WsoftCanvas &c, const WsoftUiModel &m, const Theme &T) {
  char cut[CUT_SIZE];
  c.rect(0, 20, 320, 42, T.bg);
  if (paint_slot(c, m, T)) return;
  // Not for trade: a small six-pointed star instead of the words (Rhyno, 14 Nov 2026); a tap
  // on it says NOT TRADE APPROVED (WSOFT_UI_BOX_STAR).
  double w = WSOFT_UI_STAR_W;
  if (m.trade_ok) {
    const char *chip = "TRADE CHECKS ON";
    w = c.width(chip, SMALL) + 14;
    c.rect(8, 24, w, 17, T.ok, 4);
    c.rect(9, 25, w - 2, 15, T.chip_ok, 3);
    c.text(chip, 8 + w / 2, 32.5, SMALL, T.ok_ink, WSOFT_CENTER);
  } else {
    c.star6(8 + w / 2, 32.5, 9, T.bad);
  }
  double sw = c.text(m.steady, 312, 32.5, SMALL, tone(T, m.steady_tone), WSOFT_RIGHT);
  c.disc(312 - sw - 8, 32.5, 4, dot(T, m.steady_tone));
  c.text(c.fit(m.reason, TINY, 312 - sw - 20 - (8 + w + 7), cut, sizeof cut), 8 + w + 7, 32.5, TINY,
         tone(T, m.reason_tone));
  double aw = c.text(m.audit, 312, 52, TINY, T.soft, WSOFT_RIGHT);
  char plate[128];
  c.text(wsoft_ui_plate_line(c, m, 312 - aw - 8 - 10, plate, sizeof plate), 10, 52, TINY, T.soft);
}

// The weight panel takes the colour of the Hi-Lo zone the relays are in: yellow LOW, green
// OK, red HIGH. No zone (no product, empty scale, no signal): the plain background.
static void paint_weight(WsoftCanvas &c, const WsoftUiModel &m, const Theme &T) {
  c.rect(0, 62, 320, 60, T.bg);
  uint16_t ink = m.live ? T.ink : T.soft, soft = T.soft;
  if (m.zone != WSOFT_UI_ZONE_NONE) {
    c.rect(6, 64, 308, 56, zone_fill(T, m.zone, T.z_high), 8);
    ink = soft = zone_ink(T, m.zone, T.z_high_ink);
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
  c.rect(0, 188, 320, 52, T.bg);
  if (m.zero_btn) {
    // A board that weighs itself (load cell): ZERO takes the place of the GROSS read-out
    // (with no tare on, gross is the big number anyway).
    c.rect(8, 190, 104, 46, T.btn, 8);
  } else {
    c.rect(8, 190, 104, 46, T.line, 8);
    c.rect(9, 191, 102, 44, T.panel, 7);
  }
  bottom_left_words(c, m, T.btn_ink, T.btn_ink, T.soft, m.live ? T.ink : T.soft);
  c.rect(120, 190, 72, 46, T.btn, 8);
  for (int i = 0; i < 3; i++) c.rect(144, 200 + i * 6, 24, 3, T.btn_ink, 1);
  c.text("MENU", 156, 226, SMALL, T.btn_ink, WSOFT_CENTER);
  c.rect(200, 190, 112, 46, m.save_look == WSOFT_UI_LOOK_ON ? T.ok : m.save_look == WSOFT_UI_LOOK_BAD ? T.stop : T.btn, 8);
  save_words(c, m, m.save_look == WSOFT_UI_LOOK_BTN ? T.btn_ink : WHITE);
}

// ---------- the Hi-Lo word's sign: a tick, or an arrow down (low) or up (high) ----------

static void paint_sign(WsoftCanvas &c, int zone, double x, double y, uint16_t ink) {
  if (zone == WSOFT_UI_ZONE_OK) {
    c.line(x - 8, y, x - 2.5, y + 5.5, 4, ink);
    c.line(x - 2.5, y + 5.5, x + 8, y - 6, 4, ink);
  } else if (zone == WSOFT_UI_ZONE_LOW) {
    c.line(x - 7.5, y + 4, x, y - 4.5, 4, ink);
    c.line(x, y - 4.5, x + 7.5, y + 4, 4, ink);
  } else {
    c.line(x - 7.5, y - 4, x, y + 4.5, 4, ink);
    c.line(x, y + 4.5, x + 7.5, y - 4, 4, ink);
  }
}

// The Hi-Lo word and what to do, as one line: "TOO LOW  ADD MORE". out needs 40 bytes.
static const char *hilo_words(const WsoftUiModel &m, char *out, size_t size) {
  if (m.hint[0]) snprintf(out, size, "%s  %s", m.word, m.hint);
  else snprintf(out, size, "%s", m.word);
  return out;
}

// The plate and the audit counter belong on the screen when there is a plate or the trade checks are on.
static bool plate_on(const WsoftUiModel &m) {
  return m.trade_ok || m.plate_sa[0] || m.plate_limits[0];
}

// "MAIZE   TARGET 50 KG": the product and its target on one line.
static const char *product_line(const WsoftUiModel &m, char *out, size_t size) {
  snprintf(out, size, "%s   %s", m.product, m.target);
  return out;
}

// ---------- Two looks share one layout: the word in a pill, a slim limits bar ----------
// Light mode is the faceplate look (Rhyno, 22 Nov 2026: the box's plate is light grey-white
// with a red swoosh): plate grey, the brand's blue bar, navy letters, a thin red strip by the
// weight, the word in a pill of the zone's colour. Full Colour is the bright look (Rhyno, same
// day: "make the colours brighter"): the whole screen in a vivid zone colour, the word in a
// white (or navy) pill. With no zone (no product, empty scale, no signal) both fall back to
// the classic screen of their theme.
static const uint16_t PLATE = 0xEF7E;       // #E9EEF2
static const uint16_t NAVY = 0x18A8;        // #1B1446
static const uint16_t SLATE = 0x5B0E;       // #5A6270
static const uint16_t PLATE_LINE = 0xCE9B;  // #C9D2DA
static const uint16_t SKY = 0x14DA;         // #1699D6
static const uint16_t SWOOSH = 0xE127;      // #E4243B
// Full strength (Rhyno, 22 Nov 2026: "nice and bright", Ferrari red, no mustard).
static const uint16_t VIVID_LOW = 0xFC60;   // #FF8C00
static const uint16_t VIVID_OK = 0x064A;    // #00C853
static const uint16_t VIVID_HIGH = 0xF940;  // #FF2800
static const uint16_t NIGHT = 0x1066;       // #120D36
// Full Colour is "Aurora" (Rhyno, 22 Nov 2026): a night-blue screen that fades down to Bunting,
// the weight in a banner filled with the zone's colour, and the word in black on a pill of the
// same colour.
static const uint16_t AUR_TOP = 0x0824;     // #0B0726
static const uint16_t AUR_LOW = 0x18A8;     // #1B1446
static const uint16_t AUR_SOFT = 0x9D9B;    // #9FB3D9
static const uint16_t AUR_BTN = 0x20EA;     // #221C55
static const uint16_t AUR_LINE = 0x39AF;    // #3A3478
static const uint16_t AUR_BAR = 0x0023;     // #07051A
static const uint16_t INK0 = 0x0862;        // #0B0D10

struct ColourSet {
  uint16_t bg, ink, soft, shade, btn, btn_ink, btn_soft, bar, on, on_ink;
  bool grad, band, card, border;
  uint16_t btn_line;
  uint16_t w_bg, w_ink, w_soft, strip, pill, pill_ink, out, mid, over, dot, mark, m_dot;
};

static ColourSet colour_set(const WsoftUiModel &m, const Theme &) {
  const bool low = m.zone == WSOFT_UI_ZONE_LOW, ok = m.zone == WSOFT_UI_ZONE_OK;
  ColourSet P;
  if (m.style != WSOFT_UI_STYLE_COLOUR) {
    uint16_t z = low ? VIVID_LOW : ok ? VIVID_OK : VIVID_HIGH;
    P.bg = PLATE;
    P.ink = NAVY;
    P.soft = SLATE;
    P.shade = PLATE_LINE;
    P.btn = WHITE;
    P.btn_ink = NAVY;
    P.btn_soft = SLATE;
    P.bar = SKY;
    P.on = NAVY;
    P.on_ink = WHITE;
    P.grad = false;
    P.band = false;
    P.card = false;
    P.border = false;
    P.btn_line = PLATE_LINE;
    P.w_bg = PLATE;
    P.w_ink = NAVY;
    P.w_soft = SLATE;
    P.strip = SWOOSH;
    P.pill = z;
    P.pill_ink = low ? NIGHT : WHITE;
    P.out = wsoft_blend565(PLATE, VIVID_LOW, 130);
    P.mid = wsoft_blend565(PLATE, VIVID_OK, 130);
    P.over = wsoft_blend565(PLATE, VIVID_HIGH, 130);
    P.dot = z;
    P.mark = NAVY;
    P.m_dot = z;
    return P;
  }
  uint16_t z = low ? VIVID_LOW : ok ? VIVID_OK : VIVID_HIGH;
  P.bg = wsoft_blend565(AUR_TOP, AUR_LOW, 174);
  P.grad = true;
  P.ink = WHITE;
  P.soft = AUR_SOFT;
  P.shade = wsoft_blend565(P.bg, WHITE, 30);
  P.btn = AUR_BTN;
  P.btn_ink = WHITE;
  P.btn_soft = AUR_SOFT;
  P.bar = AUR_BAR;
  P.on = SKY;
  P.on_ink = WHITE;
  P.band = false;
  P.card = true;
  P.border = true;
  P.btn_line = AUR_LINE;
  P.w_bg = AUR_BTN;
  P.w_ink = WHITE;
  P.w_soft = AUR_SOFT;
  P.strip = z;
  P.pill = z;
  P.pill_ink = INK0;
  P.out = wsoft_blend565(P.bg, VIVID_LOW, 110);
  P.mid = wsoft_blend565(P.bg, VIVID_OK, 110);
  P.over = wsoft_blend565(P.bg, VIVID_HIGH, 110);
  P.dot = z;
  P.mark = WHITE;
  P.m_dot = z;
  return P;
}

// The ground: flat, or Aurora's fade in 6-line steps (y 20 is the top colour, y 240 the bottom).
static void ground(WsoftCanvas &c, const ColourSet &P, int y0, int h) {
  if (!P.grad) {
    c.rect(0, y0, 320, h, P.bg);
    return;
  }
  for (int y = y0; y < y0 + h; y += 6) {
    int step = y0 + h - y < 6 ? y0 + h - y : 6;
    c.rect(0, y, 320, step, wsoft_blend565(AUR_TOP, AUR_LOW, (y - 20) * 255 / 220));
  }
}

// Light mode on the live page draws with these looks too.
static bool look_style(const WsoftUiModel &m, int style) {
  return style == WSOFT_UI_STYLE_COLOUR || (style == WSOFT_UI_STYLE_CLASSIC && !m.dark);
}

static void paint_top(WsoftCanvas &c, const WsoftUiModel &m, const Theme &T);
static void paint_hilo(WsoftCanvas &c, const WsoftUiModel &m, const Theme &T);
static void paint_bottom(WsoftCanvas &c, const WsoftUiModel &m, const Theme &T);

static void paint_top_colour(WsoftCanvas &c, const WsoftUiModel &m, const Theme &T) {
  if (m.zone == WSOFT_UI_ZONE_NONE) return paint_top(c, m, T);
  char cut[CUT_SIZE];
  ColourSet P = colour_set(m, T);
  ground(c, P, 20, 42);
  if (m.slot == WSOFT_UI_SLOT_WIFI) {
    c.rect(6, 22, 308, 40, P.btn, 8);
    c.text("WI-FI IS OFF", 18, 42, MID, P.btn_ink);
    c.rect(160, 26, 100, 32, P.on, 6);
    c.text("CONNECT", 210, 42, SMALL, P.on_ink, WSOFT_CENTER);
    c.rect(266, 26, 42, 32, P.btn_ink, 6);
    c.rect(267.5, 27.5, 39, 29, P.btn, 5);
    c.text("X", 287, 42, MID, P.btn_ink, WSOFT_CENTER);
    return;
  }
  if (m.slot != WSOFT_UI_SLOT_NONE) {
    bool stop = m.slot == WSOFT_UI_SLOT_STOP;
    uint16_t sk = stop ? WHITE : P.btn_ink;
    c.rect(6, 22, 308, 40, stop ? T.stop : P.btn, 8);
    if (c.width(m.slot_title, BIG) <= 292) c.text(m.slot_title, 160, 34, BIG, sk, WSOFT_CENTER);
    else c.text(c.fit(m.slot_title, MID, 296, cut, sizeof cut), 160, 34, MID, sk, WSOFT_CENTER);
    c.text(c.fit(m.slot_detail, SMALL, 296, cut, sizeof cut), 160, 53, SMALL, sk, WSOFT_CENTER);
    return;
  }
  double w = WSOFT_UI_STAR_W;
  if (m.trade_ok) {
    const char *chip = "TRADE CHECKS ON";
    w = c.width(chip, SMALL) + 14;
    c.rect(8, 24, w, 17, P.btn, 4);
    c.text(chip, 8 + w / 2, 32.5, SMALL, P.btn_ink, WSOFT_CENTER);
  } else {
    c.star6(8 + w / 2, 32.5, 9, P.soft);
  }
  double sw = c.text(m.steady, 312, 32.5, SMALL, P.soft, WSOFT_RIGHT);
  c.disc(312 - sw - 8, 32.5, 4, P.dot);
  c.text(c.fit(m.reason, TINY, 312 - sw - 20 - (8 + w + 7), cut, sizeof cut), 8 + w + 7, 32.5, TINY, P.soft);
  if (plate_on(m)) {
    double aw = c.text(m.audit, 312, 52, TINY, P.soft, WSOFT_RIGHT);
    char plate[128];
    c.text(wsoft_ui_plate_line(c, m, 312 - aw - 8 - 10, plate, sizeof plate), 10, 52, TINY, P.soft);
    return;
  }
  double tw = c.text(m.target, 312, 52, SMALL, P.soft, WSOFT_RIGHT);
  c.text(c.fit(m.product, MID, 312 - tw - 12 - 10, cut, sizeof cut), 10, 51, MID, P.ink);
}

static void paint_weight_colour(WsoftCanvas &c, const WsoftUiModel &m, const Theme &T) {
  uint16_t bg = T.bg, ink = m.live ? T.ink : T.soft, soft = T.soft;
  if (m.zone != WSOFT_UI_ZONE_NONE) {
    ColourSet P = colour_set(m, T);
    ground(c, P, 62, 60);
    if (P.card) {
      // Aurora glass card (Rhyno picked A, 22 Nov 2026): a thin line round it and a zone-coloured bar on the left.
      c.rect(6, 62, 308, 60, P.btn_line, 10);
      c.rect(7, 63, 306, 58, P.w_bg, 9);
      c.rect(11, 69, 5, 46, P.strip, 2);
    } else if (!P.band) c.rect(0, 64, 4, 56, P.strip);
    ink = P.w_ink;
    soft = P.w_soft;
  } else {
    c.rect(0, 62, 320, 60, bg);
  }
  // Aurora's card: the weight sits inside it with room to spare.
  if (m.zone != WSOFT_UI_ZONE_NONE && colour_set(m, T).card) {
    c.text(m.label, 302, 73, TINY, soft, WSOFT_RIGHT);
    // Big numbers with a thick outline in the full zone colour (Rhyno, 22 Nov 2026: "as big as
    // under range", outline "thicker", "brighter").
    const ColourSet P = colour_set(m, T);
    const bool huge = c.width(m.weight, HUGE_) <= 256;
    const int role = huge ? HUGE_ : c.width(m.weight, HERO) <= 236 ? HERO : BIG;
    const double x = huge ? 280 : 264, y = huge ? 92 : 94;
    static const int8_t outline[8][2] = {{-3, 0}, {3, 0}, {0, -3}, {0, 3}, {-2, -2}, {2, -2}, {-2, 2}, {2, 2}};
    for (const auto &o : outline) c.text(m.weight, x + o[0], y + o[1], role, P.strip, WSOFT_RIGHT);
    c.text(m.weight, x, y, role, ink, WSOFT_RIGHT);
    c.text(m.unit, x + 6, huge ? 109 : 106, MID, soft);
    return;
  }
  if (c.width(m.label, TINY) <= 24 && c.width(m.weight, HUGE_) <= 270) {
    c.text(m.label, 306, 74, TINY, soft, WSOFT_RIGHT);
    c.text(m.weight, 280, 92, HUGE_, ink, WSOFT_RIGHT);
    c.text(m.unit, 286, 109, MID, soft);
    return;
  }
  c.text(m.label, 16, 74, TINY, soft);
  c.text(m.weight, 268, 95, c.width(m.weight, HERO) <= 250 ? HERO : BIG, ink, WSOFT_RIGHT);
  c.text(m.unit, 274, 106, MID, soft);
}

static void paint_hilo_colour(WsoftCanvas &c, const WsoftUiModel &m, const Theme &T) {
  if (m.zone == WSOFT_UI_ZONE_NONE) return paint_hilo(c, m, T);
  char cut[CUT_SIZE], line[CUT_SIZE];
  ColourSet P = colour_set(m, T);
  ground(c, P, 122, 66);
  // The word and what to do in a pill of the zone's own colour.
  char words[40];
  hilo_words(m, words, sizeof words);
  int role = BIG;
  if (c.width(words, BIG) + 52 > 304) role = MID;
  if (role == MID && c.width(words, MID) + 52 > 304) role = SMALL;
  double wy = plate_on(m) ? 139 : 143;
  double pw = c.width(words, role) + 52, px = 160 - pw / 2;
  // The band is as tall as the pill when the plate needs its line under it.
  if (P.band) {
    double bh = plate_on(m) ? 28 : 34;
    c.rect(0, wy - bh / 2, 320, bh, P.pill);
  }
  else {
    if (P.card) c.rect(px - 4, wy - 18, pw + 8, 36, wsoft_blend565(P.bg, P.pill, 70), -1);
    c.rect(px, wy - 14, pw, 28, P.pill, -1);
  }
  paint_sign(c, m.zone, px + 23, wy, P.pill_ink);
  c.text(words, px + 38, wy, role, P.pill_ink);
  if (plate_on(m)) c.text(c.fit(product_line(m, line, sizeof line), SMALL, 300, cut, sizeof cut), 160, 160, SMALL, P.soft, WSOFT_CENTER);
  // The limits as three soft bands, and the marker.
  const double bx = 14, bw = 292, by = 168, bh = 6;
  double lo = bx + bw * 6 / 22, hi = bx + bw * 16 / 22;
  if (P.card) {
    // Aurora: forty short segments, and the marker a white bar.
    const double sw = bw / 40;
    for (int i = 0; i < 40; i++) {
      double t = (i + 0.5) / 40;
      c.rect(bx + i * sw, by, sw - 2, bh, t < 6.0 / 22 ? P.out : t < 16.0 / 22 ? P.mid : P.over);
    }
  } else {
    c.rect(bx, by, bw, bh, P.shade, -1);
    c.rect(bx, by, lo - bx, bh, P.out, -1);
    c.rect(lo, by, hi - lo, bh, P.mid);
    c.rect(hi, by, bx + bw - hi, bh, P.over, -1);
  }
  c.text(m.low, lo, 182, TINY, P.soft, WSOFT_CENTER);
  c.text(m.high, hi, 182, TINY, P.soft, WSOFT_CENTER);
  if (m.pointer < 0) return;
  double mx = bx + bw * m.pointer / 1000;
  if (mx < bx + 7) mx = bx + 7;
  if (mx > bx + bw - 7) mx = bx + bw - 7;
  if (P.card) {
    c.rect(mx - 3.5, by - 6, 7, bh + 12, P.bg, 2);
    c.rect(mx - 2.5, by - 5, 5, bh + 10, P.mark, 2);
    return;
  }
  c.disc(mx, by + bh / 2, 8, P.bg);
  c.disc(mx, by + bh / 2, 6.5, P.mark);
  c.disc(mx, by + bh / 2, 3, P.m_dot);
}

static void paint_bottom_colour(WsoftCanvas &c, const WsoftUiModel &m, const Theme &T) {
  if (m.zone == WSOFT_UI_ZONE_NONE) return paint_bottom(c, m, T);
  ColourSet P = colour_set(m, T);
  ground(c, P, 188, 52);
  if (P.border) c.rect(7, 189, 106, 48, P.btn_line, 9);
  c.rect(8, 190, 104, 46, P.btn, 8);
  bottom_left_words(c, m, P.btn_ink, P.btn_soft, P.btn_soft, P.btn_ink);
  if (P.border) c.rect(119, 189, 74, 48, P.btn_line, 9);
  c.rect(120, 190, 72, 46, P.btn, 8);
  for (int i = 0; i < 3; i++) c.rect(144, 200 + i * 6, 24, 3, P.btn_ink, 1);
  c.text("MENU", 156, 226, SMALL, P.btn_ink, WSOFT_CENTER);
  bool on = m.save_look == WSOFT_UI_LOOK_ON, bad = m.save_look == WSOFT_UI_LOOK_BAD;
  c.rect(200, 190, 112, 46, on ? P.on : bad ? T.stop : P.btn, 8);
  save_words(c, m, on ? P.on_ink : bad ? WHITE : P.btn_ink);
}

// ---------- Dial: the Hi-Lo bar bent into a gauge over the weight ----------

static const double DIAL_X = 160, DIAL_Y = 176, DIAL_R = 152, DIAL_W = 11;

struct DialDir {
  double x, y;
};

// The direction from the dial's centre for a place 0..1000 along it: from the left end over
// the top to the right end. Square roots only, so a PC and a board find the same dot.
static DialDir dial_dir(double p) {
  const double ex = 0.9703, ey = -0.2419;
  double vx, vy;
  if (p < 500) {
    double u = p / 500;
    vx = -ex + ex * u;
    vy = ey + (-1 - ey) * u;
  } else {
    double u = (p - 500) / 500;
    vx = ex * u;
    vy = -1 + (ey + 1) * u;
  }
  double n = sqrt(vx * vx + vy * vy);
  return DialDir{vx / n, vy / n};
}

// The arc, the two limits and the marker. Three rows share it: each paints its own part.
static void paint_dial(WsoftCanvas &c, const WsoftUiModel &m, const Theme &T) {
  DialDir a = dial_dir(0), lo = dial_dir(6000.0 / 22), hi = dial_dir(16000.0 / 22), b = dial_dir(1000);
  if (!m.product_on) {
    c.ring(DIAL_X, DIAL_Y, DIAL_R, DIAL_W, a.x, a.y, 0, -1, T.line);
    c.ring(DIAL_X, DIAL_Y, DIAL_R, DIAL_W, 0, -1, b.x, b.y, T.line);
    return;
  }
  c.ring(DIAL_X, DIAL_Y, DIAL_R, DIAL_W, a.x, a.y, lo.x, lo.y, T.wait);
  c.ring(DIAL_X, DIAL_Y, DIAL_R, DIAL_W, lo.x, lo.y, hi.x, hi.y, T.ok);
  c.ring(DIAL_X, DIAL_Y, DIAL_R, DIAL_W, hi.x, hi.y, b.x, b.y, T.bad);
  double k = c.scale_y() / c.scale_x(), lr = DIAL_R - DIAL_W - 12;
  c.text(m.low, DIAL_X + lo.x * lr * k, DIAL_Y + lo.y * lr, TINY, T.soft, WSOFT_CENTER);
  c.text(m.high, DIAL_X + hi.x * lr * k, DIAL_Y + hi.y * lr, TINY, T.soft, WSOFT_CENTER);
  if (m.pointer < 0) return;
  DialDir d = dial_dir(m.pointer);
  double mr = DIAL_R - DIAL_W / 2;
  double mx = DIAL_X + d.x * mr * k, my = DIAL_Y + d.y * mr;
  c.disc(mx, my, 10, T.bg);
  c.disc(mx, my, 8, T.pointer);
  c.disc(mx, my, 3.5, zone_fill(T, m.zone, T.bg));
}

static void paint_top_dial(WsoftCanvas &c, const WsoftUiModel &m, const Theme &T) {
  char cut[CUT_SIZE];
  c.rect(0, 20, 320, 42, T.bg);
  paint_dial(c, m, T);
  if (paint_slot(c, m, T)) return;
  if (m.trade_ok) {
    // Two short lines: the dial leaves no room for one long one.
    c.rect(8, 24, 68, 24, T.ok, 4);
    c.rect(9, 25, 66, 22, T.chip_ok, 3);
    c.text("TRADE", 42, 30.5, TINY, T.ok_ink, WSOFT_CENTER);
    c.text("CHECKS ON", 42, 41.5, TINY, T.ok_ink, WSOFT_CENTER);
    c.text(c.fit(m.reason, TINY, 48, cut, sizeof cut), 10, 56, TINY, tone(T, m.reason_tone));
  } else {
    c.star6(18, 32.5, 9, T.bad);
    c.text(c.fit(m.reason, TINY, 66, cut, sizeof cut), 35, 32.5, TINY, tone(T, m.reason_tone));
  }
  double sw = c.text(m.steady, 312, 32.5, SMALL, tone(T, m.steady_tone), WSOFT_RIGHT);
  c.disc(312 - sw - 8, 32.5, 4, dot(T, m.steady_tone));
}

static void paint_weight_dial(WsoftCanvas &c, const WsoftUiModel &m, const Theme &T) {
  char line[CUT_SIZE];
  c.rect(0, 62, 320, 60, T.bg);
  paint_dial(c, m, T);
  snprintf(line, sizeof line, "%s  %s", m.label, m.unit);
  c.text(line, 160, 71, TINY, T.soft, WSOFT_CENTER);
  // The number takes a soft tint of its zone (Rhyno, 22 Nov 2026).
  uint16_t ink = !m.live                        ? T.soft
                 : m.zone == WSOFT_UI_ZONE_LOW  ? T.wait_ink
                 : m.zone == WSOFT_UI_ZONE_OK   ? T.ok_ink
                 : m.zone == WSOFT_UI_ZONE_HIGH ? T.bad_ink
                                                : T.ink;
  c.text(m.weight, 160, 100, c.width(m.weight, HERO) <= 200 ? HERO : BIG, ink, WSOFT_CENTER);
}

static void paint_hilo_dial(WsoftCanvas &c, const WsoftUiModel &m, const Theme &T) {
  char cut[CUT_SIZE], line[CUT_SIZE];
  c.rect(0, 122, 320, 66, T.bg);
  paint_dial(c, m, T);
  // The word in a pill of the zone's colour, with its sign.
  bool zoned = m.zone != WSOFT_UI_ZONE_NONE;
  uint16_t fill = zone_fill(T, m.zone, T.btn);
  uint16_t ink = zone_ink(T, m.zone, tone(T, m.word_tone));
  // The word and what to do, in the biggest letters that fit.
  char words[40];
  hilo_words(m, words, sizeof words);
  double sign = zoned ? 24 : 0;
  int role = m.product_on ? BIG : MID;
  if (role == BIG && c.width(words, BIG) + sign + 28 > 304) role = MID;
  if (role == MID && c.width(words, MID) + sign + 28 > 304) role = SMALL;
  double pw = c.width(words, role) + sign + 28, px = 160 - pw / 2;
  c.rect(px, 141, pw, 25, fill, -1);
  if (zoned) paint_sign(c, m.zone, px + 23, 153.5, ink);
  c.text(words, px + 14 + sign, 153.5, role, ink);
  // The plate takes the bottom line when it is there: the product line moves up in small letters.
  int role2 = plate_on(m) ? TINY : SMALL;
  double y2 = plate_on(m) ? 171.5 : 174;
  if (m.product_on) c.text(c.fit(product_line(m, line, sizeof line), role2, 300, cut, sizeof cut), 160, y2, role2, T.soft, WSOFT_CENTER);
  else c.text(c.fit(m.target, role2, 300, cut, sizeof cut), 160, y2, role2, T.soft, WSOFT_CENTER);
  if (!plate_on(m)) return;
  double aw = c.text(m.audit, 312, 183.5, TINY, T.soft, WSOFT_RIGHT);
  char plate[128];
  c.text(wsoft_ui_plate_line(c, m, 312 - aw - 8 - 10, plate, sizeof plate), 10, 183.5, TINY, T.soft);
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
    if (m.address[0]) {
      c.text("On a phone or PC, open", 160, 90, SMALL, T.soft, WSOFT_CENTER);
      c.text(m.address, 160, 112, MID, T.ink, WSOFT_CENTER);
    } else {
      // No Wi-Fi and no hotspot: there is no address to give yet.
      c.text("Connect the board to Wi-Fi,", 160, 90, SMALL, T.ink, WSOFT_CENTER);
      c.text("then open its web page", 160, 112, SMALL, T.ink, WSOFT_CENTER);
    }
    c.text("Log in as admin, then", 160, 140, SMALL, T.soft, WSOFT_CENTER);
    c.text("W-Soft  >  Settings  >  Approval PIN", 160, 160, SMALL, T.ink, WSOFT_CENTER);
    paint_tile(c, T, WSOFT_UI_BOX_WIDE, "", "BACK", WSOFT_UI_LOOK_PLAIN);
  } else if (m.wifi_up) {
    c.text("CONNECTED", 160, 84, BIG, T.ok_ink, WSOFT_CENTER);
    c.text(m.ip, 160, 118, MID, T.ink, WSOFT_CENTER);
    c.text("Type this address in a browser on the same Wi-Fi", 160, 144, TINY, T.soft, WSOFT_CENTER);
    paint_tile(c, T, WSOFT_UI_BOX_WIDE, "", "BACK", WSOFT_UI_LOOK_PLAIN);
  } else if (!m.ap_name[0]) {
    // No Wi-Fi and the board's own hotspot is off: nothing to join.
    c.text("NO WI-FI", 160, 84, BIG, T.bad_ink, WSOFT_CENTER);
    c.text("The board hotspot is off too.", 160, 118, SMALL, T.ink, WSOFT_CENTER);
    c.text("Restart the board to open its hotspot", 160, 144, TINY, T.soft, WSOFT_CENTER);
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

// One field of the model added to a key.
#define FIELD(key, name) stretch(key, m, offsetof(WsoftUiModel, name), offsetof(WsoftUiModel, name) + sizeof m.name)

uint32_t wsoft_ui_row_key(const WsoftUiModel &m, int row) {
  // The menu pages look the same in every style.
  const int style = m.page == WSOFT_UI_PAGE_LIVE ? m.style : WSOFT_UI_STYLE_CLASSIC;
  const size_t bar = offsetof(WsoftUiModel, wifi_up), top = offsetof(WsoftUiModel, slot);
  const size_t weight = offsetof(WsoftUiModel, label), hilo = offsetof(WsoftUiModel, word);
  const size_t bottom = offsetof(WsoftUiModel, gross), page = offsetof(WsoftUiModel, title);
  uint32_t key = stretch(0, m, 0, bar);  // dark, page, style
  const bool look = look_style(m, style);
  switch (row) {
    case WSOFT_UI_ROW_BAR:
      key = stretch(key, m, bar, top);
      // Full Colour and light mode: the bar takes the look's colour once there is a zone.
      return look ? FIELD(key, zone) : key;
    case WSOFT_UI_ROW_TOP:
      key = stretch(key, m, top, weight);
      // Full Colour and light mode: the zone's colour, and the product and target sit here.
      if (look) key = FIELD(FIELD(FIELD(key, zone), product), target);
      // Dial: the top of the arc, the limits and the marker.
      if (style == WSOFT_UI_STYLE_DIAL) key = FIELD(stretch(key, m, hilo, bottom), zone);
      return key;
    case WSOFT_UI_ROW_WEIGHT:
      key = stretch(key, m, weight, hilo);
      if (look) key = FIELD(key, zone);
      return style == WSOFT_UI_STYLE_DIAL ? stretch(key, m, hilo, bottom) : key;
    case WSOFT_UI_ROW_HILO:
      key = stretch(key, m, hilo, bottom);
      // The pill looks and Dial: the zone's colour, and the plate line or what it pushes aside.
      if (look || style == WSOFT_UI_STYLE_DIAL) key = FIELD(FIELD(FIELD(FIELD(FIELD(key, zone), trade_ok), plate_sa), plate_limits), audit);
      return key;
    case WSOFT_UI_ROW_BOTTOM:
      // GROSS is greyed when no weight arrives.
      key = stretch(key, m, bottom, page);
      key = FIELD(key, live);
      return look ? FIELD(key, zone) : key;
    case WSOFT_UI_ROW_PAGE:
      // The Wi-Fi page shows whether Wi-Fi is up and the address. Not the whole status bar:
      // a signal bar that flickers must not paint the page again.
      key = stretch(key, m, page, sizeof m);
      key = stretch(key, m, offsetof(WsoftUiModel, wifi_up), offsetof(WsoftUiModel, wifi_up) + sizeof m.wifi_up);
      return stretch(key, m, offsetof(WsoftUiModel, ip), offsetof(WsoftUiModel, ip) + sizeof m.ip);
    default: return 0;
  }
}

void wsoft_ui_paint_row(WsoftCanvas &c, const WsoftUiModel &m, int row) {
  // Full Colour and Dial are dark screens. A menu page looks the same in every style.
  const int style = m.page == WSOFT_UI_PAGE_LIVE ? m.style : WSOFT_UI_STYLE_CLASSIC;
  const bool colour = look_style(m, style), dial = style == WSOFT_UI_STYLE_DIAL;
  const Theme &T = m.dark || m.style != WSOFT_UI_STYLE_CLASSIC ? WSOFT_UI_DARK : WSOFT_UI_LIGHT;
  WsoftUiSpan span = wsoft_ui_row_span(row);
  c.clip(span.y0, span.y1);
  switch (row) {
    case WSOFT_UI_ROW_BAR:
      if (colour && m.zone != WSOFT_UI_ZONE_NONE) {
        uint16_t bg = colour_set(m, T).bar;
        paint_bar(c, m, bg, bg);
      } else {
        paint_bar(c, m, T.bar, T.bar_line);
      }
      break;
    case WSOFT_UI_ROW_TOP: colour ? paint_top_colour(c, m, T) : dial ? paint_top_dial(c, m, T) : paint_top(c, m, T); break;
    case WSOFT_UI_ROW_WEIGHT: colour ? paint_weight_colour(c, m, T) : dial ? paint_weight_dial(c, m, T) : paint_weight(c, m, T); break;
    case WSOFT_UI_ROW_HILO: colour ? paint_hilo_colour(c, m, T) : dial ? paint_hilo_dial(c, m, T) : paint_hilo(c, m, T); break;
    case WSOFT_UI_ROW_BOTTOM: colour ? paint_bottom_colour(c, m, T) : paint_bottom(c, m, T); break;
    case WSOFT_UI_ROW_PAGE: paint_page(c, m, T); break;
    default: break;
  }
  c.clip(0, 240);
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
