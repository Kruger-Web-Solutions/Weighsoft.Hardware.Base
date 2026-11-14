#include "wsoft_ui_model.h"

#include <stdio.h>
#include <string.h>

#include "wsoft_view.h"

// Copies text into a model field, cut to the room it has.
template <size_t N>
static void put(char (&field)[N], const char *text) {
  size_t n = 0;
  for (; text != nullptr && text[n] && n + 1 < N; n++) field[n] = text[n];
  field[n] = 0;
}

// Capitals for the screen. A character the letters do not hold becomes one '?'.
template <size_t N>
static void put_upper(char (&field)[N], const char *text) {
  size_t n = 0;
  for (const char *p = text; p != nullptr && *p && n + 1 < N; p++) {
    unsigned char ch = (unsigned char)*p;
    if (ch >= 0x80) {
      if ((ch & 0xC0) == 0xC0) field[n++] = '?';  // the first byte of a UTF-8 letter; the rest are dropped
    } else if (ch < 0x20) {
      field[n++] = '?';
    } else if (ch == '`') {
      field[n++] = '\'';  // a backtick stands taller than a capital and would leave its row
    } else {
      field[n++] = ch >= 'a' && ch <= 'z' ? (char)(ch - 32) : (char)ch;
    }
  }
  field[n] = 0;
}

int wsoft_ui_wifi_bars(int rssi) {
  return rssi >= -55 ? 4 : rssi >= -67 ? 3 : rssi >= -75 ? 2 : 1;
}

void wsoft_ui_plate_text(const WsoftScaleSetup &setup, const WsoftCardFormat &card, char *sa, size_t sa_size, char *limits,
                         size_t limits_size) {
  if (sa == nullptr || sa_size == 0 || limits == nullptr || limits_size == 0) return;
  sa[0] = limits[0] = 0;
  if (setup.sa_number[0] == 0 && setup.max_kg == WSOFT_KG_UNSET) return;
  char max_text[24], min_text[24], e_text[24] = "?";
  wsoft_kg_text(setup.max_kg, max_text, sizeof max_text);
  wsoft_kg_text(setup.min_kg, min_text, sizeof min_text);
  int64_t e;
  if (wsoft_card_e(card, &e)) wsoft_kg_text(e, e_text, sizeof e_text);
  snprintf(sa, sa_size, "SA %s", setup.sa_number[0] ? setup.sa_number : "-");
  snprintf(limits, limits_size, "MAX %s MIN %s E %s KG", max_text[0] ? max_text : "-", min_text[0] ? min_text : "-", e_text);
}

// Why it is not for trade, or the number of the last trade weight.
static const char *trade_reason(const WsoftUiInput &in, char *line, size_t size, uint8_t *tone) {
  *tone = WSOFT_UI_TONE_WAIT;
  if (in.check != TRADE_OFF && in.hold == WSOFT_HOLD_BLANK) return "OUT OF RANGE";
  if (in.check != TRADE_OFF && in.hold == WSOFT_HOLD_BELOW_MIN) return "BELOW MIN";
  switch (in.check) {
    case TRADE_OK:
      snprintf(line, size, "SEQ %lu", (unsigned long)in.trade_seq);
      *tone = WSOFT_UI_TONE_OK;
      return line;
    case TRADE_OFF:
      *tone = WSOFT_UI_TONE_SOFT;
      return "";  // the star says it (Rhyno, 14 Nov 2026: the words were too much)
    case TRADE_NO_SIGNAL: return "NO SIGNAL";
    case TRADE_STALE: return "NO REPLY";
    case TRADE_QUEUE_FULL: return "QUEUE FULL";
    case TRADE_ALARM: return "ALARM";
    case TRADE_MOTION: return "MOTION";
    case TRADE_EMPTY: return "EMPTY";
    case TRADE_NO_CLOCK: return "NO CLOCK";
  }
  return "CHECK";
}

static void bar(const WsoftUiInput &in, WsoftUiModel *m) {
  m->wifi_up = in.wifi_up;
  m->wifi_bars = in.wifi_up ? (uint8_t)wsoft_ui_wifi_bars(in.rssi) : 0;
  put(m->ip, in.wifi_up ? in.ip : "NO WI-FI");
  // Online is just "HUB". A fault shows its own words; with Wi-Fi down the bar already says
  // why, so the Hub is only red.
  if (in.hub_online && in.wifi_up) {
    put(m->hub, "HUB");
    m->hub_tone = WSOFT_UI_TONE_OK;
  } else if (in.hub_setup) {
    put(m->hub, in.hub_text);
    m->hub_tone = WSOFT_UI_TONE_SOFT;
  } else {
    put(m->hub, in.wifi_up ? in.hub_text : "HUB");
    m->hub_tone = WSOFT_UI_TONE_BAD;
  }
  put(m->link, in.source);
  m->link_tone = !in.linked ? WSOFT_UI_TONE_BAD : in.alarm ? WSOFT_UI_TONE_WAIT : WSOFT_UI_TONE_OK;
}

// The band above the weight: a red save notice first (nothing saves until the person acts),
// then the Wi-Fi alert, then a yellow notice. "All is well" notices never take the band.
static void slot(const WsoftUiInput &in, WsoftUiModel *m) {
  WsoftSaveNotice notice;
  bool on = wsoft_save_notice(in.waiting, &notice) && notice.level != WSOFT_NOTICE_INFO;
  if (on && notice.level == WSOFT_NOTICE_STOP) m->slot = WSOFT_UI_SLOT_STOP;
  else if (in.wifi_alert) m->slot = WSOFT_UI_SLOT_WIFI;
  else if (on) m->slot = WSOFT_UI_SLOT_WAIT;
  if (m->slot == WSOFT_UI_SLOT_STOP || m->slot == WSOFT_UI_SLOT_WAIT) {
    put(m->slot_title, notice.title);
    put(m->slot_detail, notice.detail);
  }
}

static void top(const WsoftUiInput &in, WsoftUiModel *m) {
  char line[18];
  m->trade_ok = in.check == TRADE_OK;
  put(m->reason, trade_reason(in, line, sizeof line, &m->reason_tone));
  if (!in.linked) {
    put(m->steady, "NO SIGNAL");
    m->steady_tone = WSOFT_UI_TONE_BAD;
  } else if (in.alarm) {
    put(m->steady, "ALARM");
    m->steady_tone = WSOFT_UI_TONE_BAD;
  } else {
    put(m->steady, in.stable ? "STABLE" : "UNSTABLE");
    m->steady_tone = in.stable ? WSOFT_UI_TONE_OK : WSOFT_UI_TONE_WAIT;
  }
  put(m->plate_sa, in.plate_sa);
  put(m->plate_limits, in.plate_limits);
  snprintf(m->audit, sizeof m->audit, "AUDIT %lu", (unsigned long)in.audit);
}

static void weight(const WsoftUiInput &in, WsoftUiModel *m) {
  char value[16];
  put(m->label, in.tared ? "NET (TARE)" : "NET");
  m->live = in.linked;
  put(m->unit, weight_unit_text(in.unit));
  // A failed poll or a blanked reading is never a number.
  if (in.linked && in.hold != WSOFT_HOLD_BLANK) {
    format_weight(in.net, in.decimals, value, sizeof value);
    put(m->weight, value);
  } else {
    put(m->weight, "----");
  }
  if (in.linked) {
    format_weight(in.gross, in.decimals, value, sizeof value);
    snprintf(m->gross, sizeof m->gross, "%s %s", value, weight_unit_text(in.unit));
  } else {
    put(m->gross, "----");
  }
}

static void kg_text(int32_t grams, char *out, size_t size) {
  if (!wsoft_weight_kg_text(grams, 3, 1, out, size)) snprintf(out, size, "?");
}

// The word, the product and the marker. The colour (zone) is the relays' own zone, so the
// screen and the relays always agree; with no zone the word says why.
static void hilo(const WsoftUiInput &in, WsoftUiModel *m) {
  m->zone = (uint8_t)in.zone;
  m->product_on = in.product != nullptr;
  m->pointer = -1;
  bool shown = in.linked && !in.alarm && in.hold != WSOFT_HOLD_BLANK;
  if (in.zone == WSOFT_RELAY_LOW) {
    put(m->word, "TOO LOW");
    m->word_tone = WSOFT_UI_TONE_WAIT;
  } else if (in.zone == WSOFT_RELAY_OK) {
    put(m->word, "OK");
    m->word_tone = WSOFT_UI_TONE_OK;
  } else if (in.zone == WSOFT_RELAY_HIGH) {
    put(m->word, "TOO HIGH");
    m->word_tone = WSOFT_UI_TONE_BAD;
  } else if (!in.linked) {
    put(m->word, "WAITING");
  } else if (in.alarm) {
    put(m->word, "ALARM");
    m->word_tone = WSOFT_UI_TONE_BAD;
  } else if (in.product == nullptr) {
    put(m->word, "NO PRODUCT");
  } else if (in.hold == WSOFT_HOLD_BLANK) {
    put(m->word, "OUT OF RANGE");
    m->word_tone = WSOFT_UI_TONE_BAD;
  } else if (in.net <= 0) {
    put(m->word, "EMPTY");
  } else {
    // A load is on and the relays have not switched yet: the word of the check, uncoloured.
    WsoftHiloState state = wsoft_hilo_check(*in.product, in.net, in.decimals, in.unit);
    put(m->word, state == WSOFT_HILO_NONE ? "--" : wsoft_hilo_state_words(state));
  }
  if (in.product == nullptr) {
    put(m->target, in.product_count > 0 ? "PICK ONE IN THE MENU" : "SET LIMITS ON THE HUB");
    return;
  }
  char text[16];
  put_upper(m->product, in.product->name[0] ? in.product->name : in.product->code);
  kg_text(in.product->target_g, text, sizeof text);
  snprintf(m->target, sizeof m->target, "TARGET %s KG", text);
  kg_text(in.product->low_g, m->low, sizeof m->low);
  kg_text(in.product->high_g, m->high, sizeof m->high);
  int64_t grams;
  if (shown && wsoft_weight_grams(in.net, in.decimals, in.unit, &grams)) {
    m->pointer = (int16_t)wsoft_ui_pointer(grams, in.product->low_g, in.product->high_g);
  }
}

// The SAVE button's second line: the last result for its 3 s, then "take it off", then the
// queue, then whether a tap or Auto save does the saving. Capitals, like the rest.
static void save(const WsoftUiInput &in, WsoftUiModel *m) {
  m->save_look = WSOFT_UI_LOOK_ON;
  if (in.writer) {
    put(m->save_line, "WRITER: NO SAVE");
    m->save_look = WSOFT_UI_LOOK_BTN;
  } else if (in.recent == WSOFT_SAVE_SAVED) {
    snprintf(m->save_line, sizeof m->save_line, "SAVED #%lu", (unsigned long)in.saved_seq);
  } else if (in.recent != WSOFT_SAVE_NONE) {
    put_upper(m->save_line, wsoft_save_result_text(in.recent));
    m->save_look = in.recent == WSOFT_SAVE_ALREADY || in.recent == WSOFT_SAVE_NEED_ZERO ? WSOFT_UI_LOOK_BTN : WSOFT_UI_LOOK_BAD;
  } else if (in.waiting == WSOFT_SAVE_ALREADY) {
    put(m->save_line, "TAKE LOAD OFF");
    m->save_look = WSOFT_UI_LOOK_BTN;
  } else if (in.pending > 0) {
    snprintf(m->save_line, sizeof m->save_line, "%u TO SEND", (unsigned)in.pending);
  } else {
    put(m->save_line, in.auto_save ? "AUTO SAVE ON" : "TAP TO SAVE");
  }
}

static void tile(WsoftUiModel *m, uint8_t id, const char *caption, const char *value, bool on) {
  if (m->tile_count >= WSOFT_UI_TILES_MAX) return;
  WsoftUiTile &t = m->tiles[m->tile_count++];
  t.id = id;
  t.look = on ? WSOFT_UI_LOOK_ON : WSOFT_UI_LOOK_BTN;
  put(t.caption, caption);
  put(t.value, value);
}

// Each menu button shows what it is set to now; green means on.
static void menu(const WsoftUiInput &in, WsoftUiModel *m) {
  tile(m, WSOFT_UI_TILE_APPROVAL, "APPROVAL", in.approval_on ? "ON" : "OFF", in.approval_on);
  tile(m, WSOFT_UI_TILE_AUTO, "AUTO SAVE", in.auto_save ? "ON" : "OFF", in.auto_save);
  if (in.rs485) {
    char value[16], text[26] = "OFF";
    if (in.tare_on) {
      format_weight(in.tare_value, in.decimals, value, sizeof value);
      snprintf(text, sizeof text, "%s %s", value, weight_unit_text(in.unit));
    }
    tile(m, WSOFT_UI_TILE_TARE, "TARE", text, in.tare_on);
    if (in.preset_on) tile(m, WSOFT_UI_TILE_PRESET, "PRESET TARE", in.preset_text, false);
  }
  if (in.product_count == 0) {
    tile(m, WSOFT_UI_TILE_PRODUCT, "PRODUCT", "NONE ON THE HUB", false);
  } else if (in.product == nullptr) {
    tile(m, WSOFT_UI_TILE_PRODUCT, "PRODUCT", "NONE", false);
  } else {
    tile(m, WSOFT_UI_TILE_PRODUCT, "PRODUCT", m->product, true);
  }
  tile(m, WSOFT_UI_TILE_THEME, "SCREEN", in.dark ? "DARK" : "LIGHT", false);
  tile(m, WSOFT_UI_TILE_WIFI, "WI-FI", in.wifi_up ? in.ip : "OFF", false);
}

void wsoft_ui_model(const WsoftUiInput &in, WsoftUiModel *m) {
  static const char *const TITLES[] = {"", "MENU", "ENTER PIN", "APPROVAL", "WI-FI"};
  memset(m, 0, sizeof *m);
  m->dark = in.dark;
  m->page = (uint8_t)(in.page >= WSOFT_UI_PAGE_LIVE && in.page <= WSOFT_UI_PAGE_WIFI ? in.page : WSOFT_UI_PAGE_LIVE);
  bar(in, m);
  slot(in, m);
  top(in, m);
  weight(in, m);
  hilo(in, m);
  save(in, m);
  menu(in, m);
  put(m->title, TITLES[m->page]);
  put(m->message, in.message);
  m->pin_count = (uint8_t)(in.pin_count < 0 ? 0 : in.pin_count > 6 ? 6 : in.pin_count);
  put(m->address, in.address);
  put(m->ap_name, in.ap_name);
}
