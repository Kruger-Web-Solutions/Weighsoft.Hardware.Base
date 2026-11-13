#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

#include <gen/wsoft_ui_presets.h>
#include <wsoft_gfx.h>
#include <wsoft_ui_model.h>
#include <wsoft_ui_paint.h>

#include "approval.h"
#include "hilo.h"
#include "hub.h"
#include "pin.h"
#include "relay.h"
#include "scale.h"
#include "switches.h"
#include "weighings.h"
#include "wsoft_log.h"
#include "wsoft_ui.h"

#ifndef WSOFT_BOARD_HEADLESS
#define WSOFT_BOARD_HEADLESS 0
#endif
#ifndef WSOFT_BOARD_CYD
#define WSOFT_BOARD_CYD 0
#endif

// ======================================================================================
// The panel and touch of each board.
//   gx_begin    start them
//   gx_paint    draw one row of the model on the panel
//   gx_flush    show what was drawn (boards that draw into a frame first)
//   gx_touch    is a finger down, and where (layout units, 320 x 240)
//   gx_capture  hand every dot of a screen to a sink, top to bottom (for SCREEN SHOT / CRC)
// ======================================================================================

static WsoftCanvas canvas;
typedef void (*DotSink)(const uint16_t *dots, size_t count, void *arg);

#if WSOFT_BOARD_HEADLESS
// ---------- no panel ----------
static const int PANEL_W = 0, PANEL_H = 0;
static const uint32_t TAP_GAP_MS = 400;
static bool gx_begin() { return false; }
static void gx_paint(const WsoftUiModel &, int) {}
static void gx_flush() {}
static bool gx_touch(int *, int *) { return false; }
static void gx_capture(const WsoftUiModel *, DotSink, void *) {}

#elif WSOFT_BOARD_CYD
// ---------- yellow board: Sunton ESP32-2432S028 ("CYD"), 2.8" 320 x 240 over SPI ----------
// No PSRAM, so no frame: a row is drawn 16 lines at a time into `band` and pushed to the panel.
// Each band is complete (background first), so nothing flickers.
#include <SPI.h>
#include <TFT_eSPI.h>
#include <gen/wsoft_ui_fonts_1x.h>
#ifndef WSOFT_CYD_ROTATION
#define WSOFT_CYD_ROTATION 3
#endif
#ifndef WSOFT_CYD_INVERT
#define WSOFT_CYD_INVERT 0
#endif
// XPT2046 resistive touch on its own SPI pins (CYD pinout). TFT_eSPI drives the panel on VSPI,
// so touch uses HSPI. Raw readings are mapped to the 320 x 240 screen with these limits; every
// tap logs "TOUCH raw=... screen=..." on USB so a different batch can be calibrated.
// A small XPT2046 reader below (no extra library): the same commands and rotation mapping
// as PaulStoffregen/XPT2046_Touchscreen.
#ifndef WSOFT_TOUCH_CS
#define WSOFT_TOUCH_CS 33
#define WSOFT_TOUCH_IRQ 36
#define WSOFT_TOUCH_CLK 25
#define WSOFT_TOUCH_MISO 39
#define WSOFT_TOUCH_MOSI 32
#endif
#ifndef WSOFT_TOUCH_ROTATION
#define WSOFT_TOUCH_ROTATION WSOFT_CYD_ROTATION
#endif
#ifndef WSOFT_TOUCH_X_MIN
#define WSOFT_TOUCH_X_MIN 200
#define WSOFT_TOUCH_X_MAX 3700
#define WSOFT_TOUCH_Y_MIN 240
#define WSOFT_TOUCH_Y_MAX 3800
#endif

static const int PANEL_W = 320, PANEL_H = 240;
static const int BAND_ROWS = 16;
static const uint32_t TAP_GAP_MS = 600;
static const uint16_t TOUCH_Z_MIN = 400;  // pressure below this is no touch
static TFT_eSPI tft;
static SPIClass touch_spi(HSPI);
static uint16_t band[PANEL_W * BAND_ROWS];
static int touch_raw[3];  // x, y, pressure of the last reading, for the log

static bool gx_begin() {
  tft.init();
  // Rotation 3 is the unmirrored landscape (rotation 1 mirrors). Inversion is set explicitly
  // either way, because CYD batches ship with different panel controllers.
  tft.setRotation(WSOFT_CYD_ROTATION);
  tft.invertDisplay(WSOFT_CYD_INVERT ? true : false);
  tft.fillScreen(0);
  canvas.begin(PANEL_W, PANEL_H, 1, 1, WSOFT_UI_FONTS_1X);
  pinMode(WSOFT_TOUCH_CS, OUTPUT);
  digitalWrite(WSOFT_TOUCH_CS, HIGH);
  pinMode(WSOFT_TOUCH_IRQ, INPUT);
  touch_spi.begin(WSOFT_TOUCH_CLK, WSOFT_TOUCH_MISO, WSOFT_TOUCH_MOSI, -1);
  WSOFT_LOG.println("TOUCH ready");
  return tft.width() == PANEL_W;
}

static void gx_paint(const WsoftUiModel &model, int row) {
  WsoftUiSpan span = wsoft_ui_row_span(row);
  tft.startWrite();
  for (int top = span.y0; top < span.y1; top += BAND_ROWS) {
    int rows = span.y1 - top < BAND_ROWS ? span.y1 - top : BAND_ROWS;
    canvas.window(band, top, rows);
    wsoft_ui_paint_row(canvas, model, row);
    tft.setAddrWindow(0, top, PANEL_W, rows);
    // true: each dot goes out high byte first, as the panel wants it.
    tft.pushColors(band, (uint32_t)PANEL_W * rows, true);
  }
  tft.endWrite();
}

static void gx_flush() {}

// The same bands, to the sink instead of the panel. Nothing on the panel changes.
static void gx_capture(const WsoftUiModel *model, DotSink sink, void *arg) {
  if (model == nullptr) return;
  for (int row = 0; row < WSOFT_UI_ROW_COUNT; row++) {
    if (!wsoft_ui_row_on_page(row, model->page)) continue;
    WsoftUiSpan span = wsoft_ui_row_span(row);
    for (int top = span.y0; top < span.y1; top += BAND_ROWS) {
      int rows = span.y1 - top < BAND_ROWS ? span.y1 - top : BAND_ROWS;
      canvas.window(band, top, rows);
      wsoft_ui_paint_row(canvas, *model, row);
      sink(band, (size_t)PANEL_W * rows, arg);
    }
  }
}

// One XPT2046 read: pressure, then X and Y (average of two readings after a dropped first one).
static bool gx_touch(int *sx, int *sy) {
  if (digitalRead(WSOFT_TOUCH_IRQ) == HIGH) return false;  // pen IRQ is low while pressed
  touch_spi.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(WSOFT_TOUCH_CS, LOW);
  touch_spi.transfer(0xB1);                          // Z1
  int z1 = touch_spi.transfer16(0xC1) >> 3;          // Z2 next
  int z = z1 + 4095;
  int z2 = touch_spi.transfer16(0x91) >> 3;          // X next
  z -= z2;
  touch_spi.transfer16(0x91);                        // first X is noisy: drop it
  int x1 = touch_spi.transfer16(0xD1) >> 3;          // Y next
  int y1 = touch_spi.transfer16(0x91) >> 3;
  int x2 = touch_spi.transfer16(0xD1) >> 3;
  int y2 = touch_spi.transfer16(0xD0) >> 3;          // power down after
  touch_spi.transfer16(0);
  digitalWrite(WSOFT_TOUCH_CS, HIGH);
  touch_spi.endTransaction();
  if (z < TOUCH_Z_MIN) return false;
  int x = (x1 + x2) / 2, y = (y1 + y2) / 2, px, py;
  switch (WSOFT_TOUCH_ROTATION) {
    case 0: px = 4095 - y; py = x; break;
    case 1: px = x; py = y; break;
    case 2: px = y; py = 4095 - x; break;
    default: px = 4095 - x; py = 4095 - y; break;
  }
  touch_raw[0] = px;
  touch_raw[1] = py;
  touch_raw[2] = z;
  *sx = constrain(map(px, WSOFT_TOUCH_X_MIN, WSOFT_TOUCH_X_MAX, 0, PANEL_W - 1), 0, PANEL_W - 1);
  *sy = constrain(map(py, WSOFT_TOUCH_Y_MIN, WSOFT_TOUCH_Y_MAX, 0, PANEL_H - 1), 0, PANEL_H - 1);
  return true;
}

#else
// ---------- Waveshare ESP32-S3-Touch-LCD-4.3: 800 x 480 RGB panel, GT911 touch ----------
// The rows are drawn into one frame in PSRAM; the whole frame goes to the panel when
// something was drawn.
#include <driver/gpio.h>
#include <esp_display_panel.hpp>
#include <esp_expander_ch422g.hpp>
#include <esp_heap_caps.h>
#include <gen/wsoft_ui_fonts_2x.h>

using namespace esp_panel::board;
using namespace esp_panel::drivers;

static const int PANEL_W = 800, PANEL_H = 480;
static const uint32_t TAP_GAP_MS = 400;
static Board *board = nullptr;
static LCD *lcd = nullptr;
static Touch *touch = nullptr;
static bool touch_ok = false;
static uint16_t *frame = nullptr;
static bool frame_dirty = false;

static bool gx_begin() {
  board = new Board();
  board->init();
  auto *expander = board->getIO_Expander();
  // After a cold power-on the CH422G can miss the first few I2C writes.
  bool expander_ok = false;
  for (int attempt = 1; expander != nullptr && attempt <= 20 && !expander_ok; attempt++) {
    expander_ok = expander->begin();
    if (!expander_ok) {
      WSOFT_LOG.printf("expander begin failed, attempt %d\n", attempt);
      delay(200);
    }
  }
  if (!expander_ok) {
    WSOFT_LOG.println("expander gave up");
    return false;
  }
  auto *chip = static_cast<esp_expander::CH422G *>(expander->getBase());
  chip->enableAllIO_Output();
  chip->digitalWrite(2, 1);
  // EXIO5 (USB_SEL / CAN_SEL) resets high, which selects CAN and can take over the RS485 GPIO15/16.
  chip->digitalWrite(5, 0);
  chip->digitalWrite(3, 0);
  delay(20);
  chip->digitalWrite(3, 1);
  delay(100);

  lcd = board->getLCD();
  auto *bus = static_cast<BusRGB *>(lcd->getBus());
  if (!bus->configRGB_BounceBufferSize(PANEL_W * 10) || !lcd->begin()) {
    WSOFT_LOG.println("lcd begin failed");
    return false;
  }

  const size_t bytes = (size_t)PANEL_W * PANEL_H * sizeof(uint16_t);
  frame = static_cast<uint16_t *>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (frame == nullptr) {
    WSOFT_LOG.println("frame alloc failed");
    lcd->colorBarTest();
    return false;
  }
  memset(frame, 0, bytes);
  canvas.begin(PANEL_W, PANEL_H, 2.5, 2, WSOFT_UI_FONTS_2X);
  canvas.window(frame, 0, PANEL_H);

  // GT911 touch on the shared I2C bus. The same reset steps as the library's board file
  // (TP_INT GPIO4 low, TP_RST on EXIO1). The screen still shows the weight without it.
  touch = board->getTouch();
  if (touch != nullptr) {
    gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_4, 0);
    delay(10);
    chip->digitalWrite(1, 0);
    delay(100);
    chip->digitalWrite(1, 1);
    delay(200);
    gpio_reset_pin(GPIO_NUM_4);
    touch_ok = touch->begin();
  }
  WSOFT_LOG.println(touch_ok ? "touch ready" : "touch failed (no taps)");
  return true;
}

static void gx_paint(const WsoftUiModel &model, int row) {
  canvas.window(frame, 0, PANEL_H);
  wsoft_ui_paint_row(canvas, model, row);
  frame_dirty = true;
}

static void gx_flush() {
  if (!frame_dirty) return;
  frame_dirty = false;
  lcd->drawBitmap(0, 0, PANEL_W, PANEL_H, reinterpret_cast<const uint8_t *>(frame), 1000);
}

// model == nullptr: the frame as it is on the panel now. Else the model is painted into the
// frame (not shown): the caller paints the live screen again afterwards.
static void gx_capture(const WsoftUiModel *model, DotSink sink, void *arg) {
  if (model != nullptr) {
    canvas.window(frame, 0, PANEL_H);
    wsoft_ui_paint(canvas, *model);
  }
  sink(frame, (size_t)PANEL_W * PANEL_H, arg);
}

// Read every 30 ms; in between the last answer stands.
static bool gx_touch(int *x, int *y) {
  static uint32_t last_read = 0;
  static bool down = false;
  static int last_x = 0, last_y = 0;
  if (!touch_ok) return false;
  if (millis() - last_read >= 30) {
    last_read = millis();
    TouchPoint points[1];
    down = touch->readPoints(points, 1, 0) > 0;
    if (down) {
      last_x = constrain(points[0].x, 0, PANEL_W - 1) * 320 / PANEL_W;
      last_y = constrain(points[0].y, 0, PANEL_H - 1) * 240 / PANEL_H;
    }
  }
  *x = last_x;
  *y = last_y;
  return down;
}
#endif

// ======================================================================================
// The pages and the taps, the same on every board.
// ======================================================================================

static const uint32_t IDLE_MS = 30000;        // back to the live page after 30 s without a tap
static const uint32_t WIFI_IDLE_MS = 180000;  // the Wi-Fi page stays 3 min: the person is on a phone
static const uint32_t WIFI_GRACE_MS = 15000;  // no Wi-Fi alert while the board is still starting
static const uint32_t RESULT_MS = 3000;       // how long SAVE shows its last result
static const char *SCREEN_REASON = "switched on the screen";

static bool ready = false;
static bool dark = true;
static int page = WSOFT_UI_PAGE_LIVE;
static WsoftUiView view;           // the newest poll
static WsoftUiModel shown;         // what is on the panel
static uint32_t shown_key[WSOFT_UI_ROW_COUNT];
static bool shown_valid[WSOFT_UI_ROW_COUNT];
static WsoftPinPad pad;
static bool pin_target = false;    // Approval ON (true) or OFF, once the PIN is right
static char message[40] = "";      // the open page's top-right line
static uint32_t last_tap_ms = 0;
static bool wifi_dismissed = false;

static bool wifi_up() {
  return WiFi.status() == WL_CONNECTED;
}

// Dark or light is kept in flash (NVS "wsoft_ui" / "dark"); dark until somebody switches it.
static void theme_load() {
  Preferences prefs;
  if (prefs.begin("wsoft_ui", false)) {
    dark = prefs.getBool("dark", true);
    prefs.end();
  }
}

static void theme_save() {
  Preferences prefs;
  if (prefs.begin("wsoft_ui", false)) {
    prefs.putBool("dark", dark);
    prefs.end();
  }
}

// "SETUP_INCOMPLETE" -> "SETUP INCOMPLETE".
static void say(const char *text) {
  strlcpy(message, text, sizeof message);
  for (char *p = message; *p; p++) {
    if (*p == '_') *p = ' ';
  }
}

static void open_page(int next) {
  page = next;
  last_tap_ms = millis();
  if (next != WSOFT_UI_PAGE_PIN) pad.clear();
  if (next == WSOFT_UI_PAGE_LIVE) message[0] = 0;
}

// ----- the facts, read from the services -----

// The facts and the texts they point at.
struct Facts {
  WsoftUiInput in;
  WsoftHiloProduct product;
  char ip[20];
  char address[20];
  char ap_name[34];
  char plate[64];
  char preset[24];
};

static void gather(Facts *f) {
  WsoftUiInput &in = f->in;
  in = WsoftUiInput();
  in.dark = dark;
  in.page = page;

  in.wifi_up = wifi_up();
  bool hotspot = (WiFi.getMode() & WIFI_AP) != 0;
  f->ip[0] = 0;
  if (in.wifi_up) {
    in.rssi = WiFi.RSSI();
    strlcpy(f->ip, WiFi.localIP().toString().c_str(), sizeof f->ip);
  }
  in.ip = f->ip;
  in.wifi_alert = !in.wifi_up && !wifi_dismissed && millis() > WIFI_GRACE_MS;
  HubState hub = hub_state();
  in.hub_online = hub == HUB_ONLINE;
  in.hub_setup = hub == HUB_SETUP;
  in.hub_text = hub_state_text(hub);

  in.source = view.source;
  in.linked = view.linked;
  in.alarm = view.alarm;
  in.stable = approval_stable();
  in.gross = view.gross;
  in.net = view.net;
  in.tared = view.tared;
  in.decimals = view.decimals;
  in.unit = view.unit;
  in.hold = view.hold;

  in.check = view.check;
  in.trade_seq = approval_last_trade_weight().seq;
  in.audit = approval_audit_counter();
  ScaleInfo scale;
  scale_info(&scale);
  f->plate[0] = 0;
  if (scale.rs485) wsoft_ui_plate_text(scale.setup, scale.card, f->plate, sizeof f->plate);
  in.plate = f->plate;

  // The zone the relays are driven with, so the weight panel and the relays always agree.
  in.zone = relay_zone();
  if (hilo_current(&f->product)) in.product = &f->product;
  HiloInfo hilo;
  hilo_info(&hilo);
  in.product_count = hilo.count;

  WeighingsInfo weigh;
  weighings_info(&weigh);
  in.writer = view.writer;
  in.auto_save = weigh.auto_save;
  in.pending = weigh.pending;
  if (weigh.last_result != WSOFT_SAVE_NONE && millis() - weigh.last_result_ms < RESULT_MS) in.recent = weigh.last_result;
  in.saved_seq = weigh.last.seq;
  in.waiting = weigh.waiting;

  SwitchesInfo switches;
  switches_info(&switches);
  in.approval_on = switches.approval_on;
  in.rs485 = switches.rs485;
  WsoftTare tare = scale_tare();
  in.tare_on = tare.on;
  in.tare_value = tare.value;
  f->preset[0] = 0;
  in.preset_on = scale.setup.preset_tare_kg != WSOFT_KG_UNSET && scale.setup.preset_tare_kg > 0;
  if (in.preset_on) {
    char kg[16];
    wsoft_kg_text(scale.setup.preset_tare_kg, kg, sizeof kg);
    snprintf(f->preset, sizeof f->preset, "%s kg", kg);
  }
  in.preset_text = f->preset;

  in.message = message;
  in.pin_count = (int)strlen(pad.entry());
  // The address to type on a phone or PC: the Wi-Fi address, else the board's own hotspot.
  // The Wi-Fi page is about joining the hotspot, so it always shows the hotspot's.
  strlcpy(f->address, "-", sizeof f->address);
  strlcpy(f->ap_name, "-", sizeof f->ap_name);
  if (hotspot) {
    strlcpy(f->address, WiFi.softAPIP().toString().c_str(), sizeof f->address);
    strlcpy(f->ap_name, WiFi.softAPSSID().c_str(), sizeof f->ap_name);
  }
  if (page == WSOFT_UI_PAGE_NO_PIN && in.wifi_up) strlcpy(f->address, f->ip, sizeof f->address);
  in.address = f->address;
  in.ap_name = f->ap_name;
}

// Builds the model from the facts and paints the rows whose part of it changed.
static void refresh(bool all) {
  static Facts facts;
  static WsoftUiModel next;
  if (!ready) return;
  gather(&facts);
  wsoft_ui_model(facts.in, &next);
  for (int row = 0; row < WSOFT_UI_ROW_COUNT; row++) {
    if (!wsoft_ui_row_on_page(row, next.page)) {
      shown_valid[row] = false;  // painted over by the other page: paint it when it is back
      continue;
    }
    uint32_t key = wsoft_ui_row_key(next, row);
    if (!all && shown_valid[row] && shown_key[row] == key) continue;
    gx_paint(next, row);
    shown_key[row] = key;
    shown_valid[row] = true;
  }
  shown = next;
  gx_flush();
}

// ----- what a tap does -----

// Approval was tapped: check what can be checked before asking for the PIN.
static void start_approval() {
  SwitchesInfo info;
  switches_info(&info);
  pin_target = !info.approval_on;
  char line[40];
  if (!info.pin_set) {
    // No PIN is set: say plainly where to set it (Rhyno, 8 Nov 2026).
    message[0] = 0;
    open_page(WSOFT_UI_PAGE_NO_PIN);
  } else if (info.pin_locked_s > 0) {
    snprintf(line, sizeof line, "PIN LOCKED %lu S", (unsigned long)info.pin_locked_s);
    say(line);
  } else if (pin_target && info.blocked != WSOFT_SWITCH_OK) {
    say(wsoft_switch_error_code(info.blocked));
  } else {
    message[0] = 0;
    open_page(WSOFT_UI_PAGE_PIN);
  }
}

static void pin_ok() {
  WsoftPinResult result = pin_try(pad.entry());
  pad.clear();
  char line[40];
  if (result == WSOFT_PIN_OK) {
    WsoftSwitchError error =
        switches_apply_here(WSOFT_SWITCH_APPROVAL, pin_target, "PIN", WSOFT_WHERE_SCREEN, SCREEN_REASON);
    open_page(WSOFT_UI_PAGE_MENU);
    say(error == WSOFT_SWITCH_OK ? (pin_target ? "APPROVAL ON" : "APPROVAL OFF") : wsoft_switch_error_code(error));
  } else if (result == WSOFT_PIN_WRONG) {
    snprintf(line, sizeof line, "WRONG PIN %d LEFT", pin_tries_left());
    say(line);
  } else if (result == WSOFT_PIN_LOCKED) {
    open_page(WSOFT_UI_PAGE_MENU);
    snprintf(line, sizeof line, "PIN LOCKED %lu S", (unsigned long)pin_locked_s());
    say(line);
  } else {
    open_page(WSOFT_UI_PAGE_NO_PIN);
  }
}

static void tare(bool preset) {
  WsoftTareError error;
  bool was_on = scale_tare().on;
  if (preset) {
    error = scale_tare_preset(view.decimals, view.unit);
  } else {
    bool stable = view.linked && approval_stable() && view.hold != WSOFT_HOLD_NOT_STABLE;
    error = scale_tare_toggle(stable, view.gross, view.decimals, view.unit);
  }
  say(error == WSOFT_TARE_OK ? (!preset && was_on ? "TARE CLEARED" : "TARE ON") : wsoft_tare_error_code(error));
}

// PRODUCT on the menu: the next Hi-Lo product (none, first, ..., last, none). The operator's
// pick wins until the Hub sets a new default (Hub ADR 0008, last change wins).
static void next_product() {
  HiloInfo before;
  hilo_info(&before);
  if (before.count == 0) {
    say("SET LIMITS ON THE HUB");
    return;
  }
  hilo_select_next();
  HiloInfo after;
  hilo_info(&after);
  char line[40];
  if (after.picked) snprintf(line, sizeof line, "PRODUCT %s", after.current.code);
  else strlcpy(line, "NO PRODUCT", sizeof line);
  say(line);
}

static void menu_tap(int tile) {
  switch (tile) {
    case WSOFT_UI_TILE_APPROVAL:
      start_approval();
      break;
    case WSOFT_UI_TILE_AUTO: {
      WeighingsInfo weigh;
      weighings_info(&weigh);
      WsoftSwitchError error =
          switches_apply_here(WSOFT_SWITCH_AUTO_SAVE, !weigh.auto_save, "screen", WSOFT_WHERE_SCREEN, "");
      say(error == WSOFT_SWITCH_OK ? (weigh.auto_save ? "AUTO SAVE OFF" : "AUTO SAVE ON") : wsoft_switch_error_code(error));
      break;
    }
    case WSOFT_UI_TILE_TARE:
      if (scale_rs485()) tare(false);
      break;
    case WSOFT_UI_TILE_PRESET:
      if (scale_rs485()) tare(true);
      break;
    case WSOFT_UI_TILE_PRODUCT:
      next_product();
      break;
    case WSOFT_UI_TILE_THEME:
      dark = !dark;
      theme_save();
      say(dark ? "DARK MODE" : "LIGHT MODE");
      break;
    case WSOFT_UI_TILE_WIFI:
      message[0] = 0;
      open_page(WSOFT_UI_PAGE_WIFI);
      break;
    default:
      break;
  }
}

static void pin_tap(int x, int y) {
  if (wsoft_ui_key_box(3, 1, 1, 1).hit(x, y)) {  // BACK
    open_page(WSOFT_UI_PAGE_MENU);
  } else if (wsoft_ui_key_box(3, 0, 1, 1).hit(x, y)) {
    pad.del();
  } else if (wsoft_ui_key_box(3, 2, 1, 2).hit(x, y)) {
    pin_ok();
  } else if (wsoft_ui_key_box(0, 3, 3, 1).hit(x, y)) {
    pad.press('0');
  } else {
    for (int i = 0; i < 9; i++) {
      if (!wsoft_ui_key_box(i % 3, i / 3, 1, 1).hit(x, y)) continue;
      pad.press((char)('1' + i));
      return;
    }
  }
}

// x, y in layout units. Works on what is on the panel now (`shown`).
static void tap(int x, int y) {
  last_tap_ms = millis();
  switch (page) {
    case WSOFT_UI_PAGE_LIVE:
      if (shown.slot == WSOFT_UI_SLOT_WIFI && WSOFT_UI_BOX_CONNECT.hit(x, y)) {
        open_page(WSOFT_UI_PAGE_WIFI);
      } else if (shown.slot == WSOFT_UI_SLOT_WIFI && WSOFT_UI_BOX_DISMISS.hit(x, y)) {
        wifi_dismissed = true;
      } else if (WSOFT_UI_BOX_WIFI_BAR.hit(x, y)) {
        open_page(WSOFT_UI_PAGE_WIFI);
      } else if (shown.slot == WSOFT_UI_SLOT_STOP && !shown.wifi_up && WSOFT_UI_BOX_SLOT.hit(x, y)) {
        // "CLOCK NOT SET - Connect Wi-Fi to get the time": a tap on it shows how.
        WeighingsInfo weigh;
        weighings_info(&weigh);
        if (weigh.waiting == WSOFT_SAVE_NO_CLOCK) open_page(WSOFT_UI_PAGE_WIFI);
      } else if (x >= WSOFT_UI_BOX_SAVE.x - 4 && y >= WSOFT_UI_BOX_SAVE.y - 4) {
        // SAVE reaches to the corner of the screen: a tap near the edge still saves.
        weighings_request_save();
      } else if (WSOFT_UI_BOX_MENU.hit(x, y)) {
        open_page(WSOFT_UI_PAGE_MENU);
      }
      return;
    case WSOFT_UI_PAGE_MENU:
      if (WSOFT_UI_BOX_BACK.hit(x, y)) {
        open_page(WSOFT_UI_PAGE_LIVE);
        return;
      }
      for (int i = 0; i < shown.tile_count && i < WSOFT_UI_TILES_MAX; i++) {
        if (!wsoft_ui_tile_box(i).hit(x, y)) continue;
        menu_tap(shown.tiles[i].id);
        return;
      }
      return;
    case WSOFT_UI_PAGE_PIN:
      pin_tap(x, y);
      return;
    case WSOFT_UI_PAGE_NO_PIN:
      open_page(WSOFT_UI_PAGE_MENU);  // a tap anywhere closes the note
      return;
    default:  // the Wi-Fi page
      if (WSOFT_UI_BOX_WIDE.hit(x, y)) open_page(WSOFT_UI_PAGE_LIVE);
      return;
  }
}

bool ui_begin() {
  theme_load();
  ready = gx_begin();
  return ready;
}

void ui_show(const WsoftUiView &next) {
  view = next;
  refresh(false);
}

void ui_loop() {
  if (!ready) return;
  // One tap per press.
  static bool was_down = false;
  static uint32_t last_press = 0;
  int x = 0, y = 0;
  bool down = gx_touch(&x, &y);
  if (down && !was_down && millis() - last_press > TAP_GAP_MS) {
    last_press = millis();
#if WSOFT_BOARD_CYD
    WSOFT_LOG.printf("TOUCH raw=%d,%d,%d screen=%d,%d\n", touch_raw[0], touch_raw[1], touch_raw[2], x, y);
#else
    WSOFT_LOG.printf("TOUCH screen=%d,%d\n", x, y);
#endif
    tap(x, y);
    refresh(false);
  }
  was_down = down;
  if (wifi_up()) wifi_dismissed = false;  // the alert comes back the next time Wi-Fi drops
  if (page != WSOFT_UI_PAGE_LIVE && millis() - last_tap_ms > (page == WSOFT_UI_PAGE_WIFI ? WIFI_IDLE_MS : IDLE_MS)) {
    open_page(WSOFT_UI_PAGE_LIVE);
    refresh(false);
  }
}

// ======================================================================================
// USB technician commands: SCREEN CRC, SCREEN SHOT [n], SCREEN TAP x y
// ======================================================================================

static void crc_sink(const uint16_t *dots, size_t count, void *arg) {
  uint32_t *crc = static_cast<uint32_t *>(arg);
  *crc = wsoft_crc32(*crc, reinterpret_cast<const uint8_t *>(dots), count * 2);
}

// Run-length text: lines of "SCR <count>*<colour> ..." (hex). Each line goes out in one write,
// so a log line from another task cannot land inside it.
struct ShotSink {
  Stream *port;
  uint32_t crc;
  uint32_t run;
  uint16_t color;
  char line[120];
  size_t used;
  uint32_t lines;
};

static void shot_line(ShotSink *s) {
  if (s->used == 0) return;
  s->line[s->used++] = '\n';
  s->port->write(reinterpret_cast<const uint8_t *>(s->line), s->used);
  s->used = 0;
  if (++s->lines % 32 == 0) delay(1);  // let the other tasks run
}

static void shot_run(ShotSink *s) {
  if (s->run == 0) return;
  if (s->used == 0) s->used = (size_t)snprintf(s->line, sizeof s->line, "SCR");
  s->used += (size_t)snprintf(s->line + s->used, sizeof s->line - s->used, " %lx*%04x", (unsigned long)s->run, s->color);
  s->run = 0;
  if (s->used > sizeof s->line - 20) shot_line(s);
}

static void shot_sink(const uint16_t *dots, size_t count, void *arg) {
  ShotSink *s = static_cast<ShotSink *>(arg);
  s->crc = wsoft_crc32(s->crc, reinterpret_cast<const uint8_t *>(dots), count * 2);
  for (size_t i = 0; i < count; i++) {
    if (s->run > 0 && dots[i] == s->color) {
      s->run++;
      continue;
    }
    shot_run(s);
    s->color = dots[i];
    s->run = 1;
  }
}

void ui_handle_command(const String &command, Stream &port) {
  static WsoftUiModel preset;
  if (!ready) {
    port.println("SCREEN_ERR no screen on this board");
    return;
  }
  if (command == "SCREEN CRC") {
    for (int n = 0; n < WSOFT_UI_PRESET_COUNT; n++) {
      uint32_t crc = 0;
      wsoft_ui_preset(n, &preset);
      gx_capture(&preset, crc_sink, &crc);
      port.printf("SCREEN_CRC %d %08lX\n", n, (unsigned long)crc);
      delay(1);
    }
    port.println("SCREEN_OK crc");
    refresh(true);
  } else if (command.startsWith("SCREEN SHOT")) {
    String rest = command.substring(11);
    rest.trim();
    const WsoftUiModel *model = &shown;
    if (rest.length() > 0) {
      if (!wsoft_ui_preset(rest.toInt(), &preset) || !isDigit(rest[0])) {
        port.println("SCREEN_ERR no such test screen");
        return;
      }
      model = &preset;
    }
#if !WSOFT_BOARD_CYD
    // A board with a frame sends the frame itself: exactly what is on the panel.
    if (rest.length() == 0) model = nullptr;
#endif
    static ShotSink sink;
    memset(&sink, 0, sizeof sink);
    sink.port = &port;
    port.printf("SCREEN_BEGIN %d %d\n", PANEL_W, PANEL_H);
    gx_capture(model, shot_sink, &sink);
    shot_run(&sink);
    shot_line(&sink);
    port.printf("SCREEN_END %08lX\n", (unsigned long)sink.crc);
    refresh(true);
  } else if (command.startsWith("SCREEN TAP ")) {
    int x = -1, y = -1;
    if (sscanf(command.c_str() + 11, "%d %d", &x, &y) != 2 || x < 0 || x >= 320 || y < 0 || y >= 240) {
      port.println("SCREEN_ERR use SCREEN TAP x y, x 0-319, y 0-239");
      return;
    }
    tap(x, y);
    refresh(false);
    port.printf("SCREEN_OK tap %d %d page=%d\n", x, y, page);
  } else {
    port.println("SCREEN_ERR use SCREEN CRC, SCREEN SHOT [n] or SCREEN TAP x y");
  }
}
