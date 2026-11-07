#include <Arduino.h>
#include <math.h>

#include "approval.h"
#include "approval_base.h"
#include "hub.h"
#include "pin.h"
#include "scale.h"
#include "switches.h"
#include "weighings.h"
#include "wsoft_screen.h"

// Two boards share this file. The poll, the Modbus jobs and the snapshot are the same on
// both; only the panel (start_display, render) and the pins differ.
#if WSOFT_BOARD_CYD
// Sunton ESP32-2432S028 ("CYD"): 2.8" 320x240 ILI9341 over SPI, no PSRAM, no RS485 transceiver.
// RS485 comes from an external 3.3 V TTL-to-RS485 module with automatic direction on the
// CN1 connector (GPIO22 / GPIO27), the same free pads the cyd28 env used for its scale UART.
#include <TFT_eSPI.h>
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
#include <SPI.h>
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
static const int RS485_RX = WSOFT_RS485_RX;
static const int RS485_TX = WSOFT_RS485_TX;
#else
#include <esp_display_panel.hpp>
#include <esp_expander_ch422g.hpp>
#include <esp_heap_caps.h>

#include "fonts.h"
#include "screen_rle.h"

using namespace esp_panel::board;
using namespace esp_panel::drivers;

// The Waveshare wiki lists TXD GPIO15 / RXD GPIO16, but on this board (bench, 2026-10-24) the
// transceiver output reaches GPIO15, so RX is 15 and TX is 16. Direction switching is automatic.
static const int RS485_RX = 15;
static const int RS485_TX = 16;
#endif

// DJB-M1X Modbus RTU (docs/contracts/djb-m1x-rs485.md): 9600 8N1, slave 1,
// holding registers are doc address - 40001, signed longs high word first (dF = 4321).
static const uint32_t DJB_BAUD = 9600;
static const uint8_t DJB_SLAVE = 1;
static const uint16_t REG_LC1_WEIGHT = 1004;  // 41005, followed by alarm state 41007
static const uint16_t REG_WEIGHT_UNIT = 1302;  // 41303: 0 none, 1 kg, 2 t, 3 g
static const uint16_t REG_DECIMAL = 1304;      // 41305: digits after the point
static const uint32_t ALARM_BLOCKING_MASK = 0xE0000000;  // bits 13-15 of the high word

static const uint32_t POLL_MS = 250;
static const int FAILS_BEFORE_NO_SIGNAL = 3;
// RS232: a scale line older than this is no longer live (same 3 s stale rule as the Pi).
static const uint32_t RS232_STALE_MS = 3000;

static bool rs232_mode = false;
static WsoftLineReader line_reader = nullptr;

void wsoft_screen_set_line_reader(WsoftLineReader reader) {
  line_reader = reader;
}

// Demo target band, matching the static "Target 1.000 - 2.000 kg" line.
static const float TARGET_LOW_KG = 1.0f;
static const float TARGET_HIGH_KG = 2.0f;
static const float GAUGE_FULL_KG = 3.0f;
#if !WSOFT_BOARD_CYD
static const int GAUGE_CX = 250, GAUGE_CY = 292, GAUGE_R = 92;
#endif

static const uint16_t INK = 0x1149;
static const uint16_t MUTED = 0x6390;
static const uint16_t GREEN = 0x1469;
static const uint16_t YELLOW = 0xD524;
static const uint16_t RED = 0xC1C6;
static const uint16_t HEADER_OK = 0x7EF2;
static const uint16_t HEADER_BAD = 0xFB2C;
static const uint16_t WHITE = 0xFFFF;

struct Reading {
  bool linked = false;
  bool alarm = false;
  int32_t value = 0;
  int decimals = 3;
  int unit = 1;
};

static void log_line(const char *message) {
  Serial0.println(message);
}

#if !WSOFT_BOARD_CYD
static Board *board = nullptr;
static LCD *lcd = nullptr;
static uint16_t *background = nullptr;
static uint16_t *frame = nullptr;

static void expand_background(uint16_t *pixels) {
  const uint16_t *cursor = SCREEN_RLE;
  for (unsigned pair = 0, i = 0; pair < SCREEN_RLE_PAIRS; pair++, cursor += 2) {
    for (uint16_t n = 0; n < cursor[0] && i < (unsigned)(SCREEN_W * SCREEN_H); n++) {
      pixels[i++] = cursor[1];
    }
  }
}

// ---------- drawing ----------

static uint16_t blend(uint16_t bg, uint16_t fg, uint8_t alpha) {
  uint32_t a = alpha, ia = 255 - alpha;
  uint32_t r = (((fg >> 11) & 0x1F) * a + ((bg >> 11) & 0x1F) * ia) / 255;
  uint32_t g = (((fg >> 5) & 0x3F) * a + ((bg >> 5) & 0x3F) * ia) / 255;
  uint32_t b = ((fg & 0x1F) * a + (bg & 0x1F) * ia) / 255;
  return (r << 11) | (g << 5) | b;
}

static const Glyph *find_glyph(const Font &font, char ch) {
  for (int i = 0; i < font.count; i++) {
    if (font.glyphs[i].ch == ch) return &font.glyphs[i];
  }
  return nullptr;
}

static int text_width(const Font &font, const char *text) {
  int width = 0;
  for (const char *p = text; *p; p++) {
    const Glyph *g = find_glyph(font, *p);
    if (g) width += g->advance;
  }
  return width;
}

enum Align { LEFT, RIGHT, CENTER };

static void draw_text(const Font &font, int x, int y, const char *text, uint16_t color, Align align = LEFT) {
  if (align == RIGHT) x -= text_width(font, text);
  if (align == CENTER) x -= text_width(font, text) / 2;
  for (const char *p = text; *p; p++) {
    const Glyph *g = find_glyph(font, *p);
    if (!g) continue;
    const uint8_t *alpha = font.alpha + g->offset;
    for (int row = 0; row < font.height; row++) {
      int py = y + row;
      if (py < 0 || py >= SCREEN_H) continue;
      for (int col = 0; col < g->width; col++) {
        int px = x + col;
        uint8_t a = alpha[row * g->width + col];
        if (a == 0 || px < 0 || px >= SCREEN_W) continue;
        uint16_t &dst = frame[py * SCREEN_W + px];
        dst = blend(dst, color, a);
      }
    }
    x += g->advance;
  }
}

static void fill_disc(int cx, int cy, int r, uint16_t color) {
  for (int dy = -r; dy <= r; dy++) {
    for (int dx = -r; dx <= r; dx++) {
      if (dx * dx + dy * dy > r * r) continue;
      int px = cx + dx, py = cy + dy;
      if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) frame[py * SCREEN_W + px] = color;
    }
  }
}

static void fill_rect(int x0, int y0, int x1, int y1, uint16_t color) {
  for (int py = max(y0, 0); py < min(y1, (int)SCREEN_H); py++) {
    for (int px = max(x0, 0); px < min(x1, (int)SCREEN_W); px++) frame[py * SCREEN_W + px] = color;
  }
}

static void draw_needle(float fraction) {
  fraction = constrain(fraction, 0.0f, 1.0f);
  float angle = PI - fraction * PI;
  float length = GAUGE_R - 8;
  for (float t = 0; t <= length; t += 1.0f) {
    fill_disc(GAUGE_CX + (int)lroundf(cosf(angle) * t), GAUGE_CY - (int)lroundf(sinf(angle) * t), 3, INK);
  }
  fill_disc(GAUGE_CX, GAUGE_CY, 8, INK);
}

#endif  // !WSOFT_BOARD_CYD

static float to_kg(const Reading &r) {
  float value = r.value / powf(10.0f, r.decimals);
  if (r.unit == 2) return value * 1000.0f;
  if (r.unit == 3) return value / 1000.0f;
  return value;
}

// Why the band says NOT FOR TRADE (approval on), or the sequence of the last trade weight.
static const char *trade_line(TradeCheck check, char *line, size_t size, uint16_t *color) {
  if (check == TRADE_OK) {
    snprintf(line, size, "SEQ %lu", (unsigned long)approval_last_trade_weight().seq);
    *color = GREEN;
    return line;
  }
  if (check == TRADE_OFF) {
    *color = MUTED;
    return "APPROVAL OFF";
  }
  *color = YELLOW;
  switch (check) {
    case TRADE_NO_SIGNAL: return "NO SIGNAL";
    case TRADE_STALE: return "NO REPLY";
    case TRADE_QUEUE_FULL: return "QUEUE FULL";
    case TRADE_ALARM: return "ALARM";
    case TRADE_MOTION: return "MOTION";
    case TRADE_EMPTY: return "EMPTY";
    case TRADE_NO_CLOCK: return "NO CLOCK";
    default: return "CHECK";
  }
}

// The demo target band: OK, UNDER, OVER, or ALARM when the box reports a blocking alarm.
static const char *zone_of(const Reading &r, uint16_t *color) {
  float kg = to_kg(r);
  if (r.alarm) {
    *color = RED;
    return "ALARM";
  }
  if (kg < TARGET_LOW_KG) {
    *color = YELLOW;
    return "UNDER";
  }
  if (kg > TARGET_HIGH_KG) {
    *color = RED;
    return "OVER";
  }
  *color = GREEN;
  return "OK";
}

#if !WSOFT_BOARD_CYD
// Top right of the card: the trade band, then why it is not for trade (or the sequence), and the audit counter.
static void draw_approval() {
  TradeCheck check = approval_check();
  bool trade = check == TRADE_OK;
  fill_rect(440, 100, 760, 144, trade ? GREEN : RED);
  draw_text(MID, 600, 105, trade ? "TRADE CHECKS ON" : "NOT FOR TRADE", WHITE, CENTER);

  char line[24];
  uint16_t color;
  const char *why = trade_line(check, line, sizeof line, &color);
  draw_text(MID, 440, 152, why, color);
  snprintf(line, sizeof line, "AUDIT %lu", (unsigned long)approval_audit_counter());
  draw_text(MID, 760, 152, line, MUTED, RIGHT);
}

static void render(const Reading &r) {
  memcpy(frame, background, SCREEN_W * SCREEN_H * sizeof(uint16_t));

  HubState hub = hub_state();
  draw_text(MID, 560, 22, hub_state_text(hub), hub == HUB_ONLINE ? HEADER_OK : HEADER_BAD, RIGHT);

  if (!r.linked) {
    draw_text(MID, 772, 22, "NO SIGNAL", HEADER_BAD, RIGHT);
    draw_text(BIG, 690, 238, "----", MUTED, RIGHT);
    draw_text(MID, GAUGE_CX, 314, "WAITING", MUTED, CENTER);
    draw_text(MID, 68, 394, "----", MUTED);
    draw_text(MID, 430, 394, "NO DJB REPLY", RED);
  } else {
    char value[16], gross[24];
    format_weight(r.value, r.decimals, value, sizeof value);
    snprintf(gross, sizeof gross, "%s %s", value, weight_unit_text(r.unit));

    float kg = to_kg(r);
    uint16_t zone_color;
    const char *zone = zone_of(r, &zone_color);

    draw_text(MID, 772, 22, r.alarm ? "DJB ALARM" : "RS485 LIVE", r.alarm ? HEADER_BAD : HEADER_OK, RIGHT);
    draw_needle(kg / GAUGE_FULL_KG);
    draw_text(MID, GAUGE_CX, 314, zone, zone_color, CENTER);
    draw_text(BIG, 690, 238, value, INK, RIGHT);
    draw_text(MID, 700, 292, weight_unit_text(r.unit), MUTED);
    draw_text(MID, 68, 394, gross, INK);
    draw_text(MID, 430, 394, zone, zone_color);
  }
  draw_approval();

  lcd->drawBitmap(0, 0, SCREEN_W, SCREEN_H, reinterpret_cast<const uint8_t *>(frame), 1000);
}
#endif  // !WSOFT_BOARD_CYD

// ---------- Modbus RTU ----------

static uint16_t crc16(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}

// The M1X keypad can change its address (menu 21) and baud (menu 22), so search until it answers.
static const uint32_t SCAN_BAUDS[] = {DJB_BAUD, 19200, 115200};
static const uint8_t SCAN_MAX_SLAVE = 10;
static uint8_t slave = DJB_SLAVE;
static int baud_index = 0;
static size_t last_rx_bytes = 0;
static uint8_t last_rx[8];

static void next_scan_target() {
  if (++slave > SCAN_MAX_SLAVE) {
    slave = 1;
    baud_index = (baud_index + 1) % 3;
    Serial1.updateBaudRate(SCAN_BAUDS[baud_index]);
  }
}

static bool read_holding(uint16_t start, uint8_t count, uint16_t *words) {
  uint8_t request[8] = {slave, 0x03, (uint8_t)(start >> 8), (uint8_t)start, 0, count};
  uint16_t crc = crc16(request, 6);
  request[6] = crc & 0xFF;
  request[7] = crc >> 8;

  while (Serial1.available()) Serial1.read();
  Serial1.write(request, sizeof request);
  Serial1.flush();

  const size_t expected = 5 + count * 2;
  uint8_t reply[32];
  size_t got = 0;
  uint32_t deadline = millis() + 200;
  while (got < expected && (int32_t)(deadline - millis()) > 0) {
    if (Serial1.available()) reply[got++] = Serial1.read();
  }
  last_rx_bytes = got;
  memcpy(last_rx, reply, got < sizeof last_rx ? got : sizeof last_rx);
  if (got != expected || reply[0] != slave || reply[1] != 0x03 || reply[2] != count * 2) return false;
  if (crc16(reply, expected - 2) != (uint16_t)(reply[expected - 2] | (reply[expected - 1] << 8))) return false;
  for (int i = 0; i < count; i++) words[i] = (reply[3 + i * 2] << 8) | reply[4 + i * 2];
  return true;
}

static bool read_long(uint16_t start, int32_t *value) {
  uint16_t words[2];
  if (!read_holding(start, 2, words)) return false;
  *value = (int32_t)(((uint32_t)words[0] << 16) | words[1]);
  return true;
}

static Reading reading;

// One queued register transaction. The HTTP handler waits; only this task touches Serial1.
static const int JOB_IDLE = 0;
static const int JOB_QUEUED = 1;
static const int JOB_RUNNING = 2;
static const int JOB_DONE = 3;

static portMUX_TYPE job_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile int job_state = JOB_IDLE;
static WsoftDjbPlan job_plan;
static WsoftDjbResult job_result;
// Set after a write so the next poll re-reads unit and decimals instead of waiting 20 polls.
static bool format_stale = false;

static void fill_ids(const WsoftDjbPlan *plan, WsoftDjbResult *out) {
  out->slave = slave;
  out->doc = plan->doc;
  out->pdu = plan->pdu;
  out->command = plan->command;
  out->value = plan->value;
}

static void run_job(const WsoftDjbPlan *plan, WsoftDjbResult *out) {
  memset(out, 0, sizeof *out);
  fill_ids(plan, out);
  if (rs232_mode) {
    strlcpy(out->error, "rs232_mode", sizeof out->error);
    return;
  }
  if (!reading.linked) {
    strlcpy(out->error, "not_linked", sizeof out->error);
    return;
  }
  // An AD write is a calibration and the card parameters are the scale's setup:
  // writes need Approval off (switched off from any place, logged and counted).
  if (plan->write && approval_on()) {
    strlcpy(out->error, "approval_on", sizeof out->error);
    return;
  }

  uint8_t request[16];
  size_t request_len = wsoft_djb_build(slave, plan, request, sizeof request);
  if (request_len == 0) {
    strlcpy(out->error, "build", sizeof out->error);
    return;
  }
  wsoft_djb_hex(request, request_len, out->request_hex, sizeof out->request_hex);

  while (Serial1.available()) Serial1.read();
  Serial1.write(request, request_len);
  Serial1.flush();
  out->sent = true;

  size_t expected = plan->write ? 8 : 9;
  uint8_t reply[32];
  size_t got = 0;
  uint32_t deadline = millis() + 200;
  while (got < expected && (int32_t)(deadline - millis()) > 0) {
    if (Serial1.available()) {
      if (got < sizeof reply) reply[got] = (uint8_t)Serial1.read();
      else Serial1.read();
      got++;
      if (got >= 2 && (reply[1] & 0x80)) expected = 5;
    } else {
      delay(1);
    }
  }
  if (got > sizeof reply) got = sizeof reply;
  wsoft_djb_hex(reply, got, out->reply_hex, sizeof out->reply_hex);

  int32_t value = plan->value;
  uint8_t exception = 0;
  int status = wsoft_djb_parse(reply, got, slave, plan->command, plan->pdu, &value, &exception);
  if (plan->write) format_stale = true;
  if (status == 0) {
    if (plan->write) scale_card_written();
    out->ok = true;
    out->value = value;
    if (plan->alarm) {
      out->alarm_valid = true;
      out->alarm_raw = (uint32_t)value;
      wsoft_djb_alarm_bits(value, &out->alarm);
    }
    return;
  }
  const char *error = "mismatch";
  if (status == 1) error = got == 0 ? "no_reply" : "short";
  else if (status == 2) error = "crc";
  else if (status == 3) {
    error = "exception";
    out->exception = exception;
  }
  strlcpy(out->error, error, sizeof out->error);
}

static void service_job() {
  if (hub_ota_busy()) return;
  WsoftDjbPlan plan;
  portENTER_CRITICAL(&job_lock);
  if (job_state != JOB_QUEUED) {
    portEXIT_CRITICAL(&job_lock);
    return;
  }
  plan = job_plan;
  job_state = JOB_RUNNING;
  portEXIT_CRITICAL(&job_lock);

  WsoftDjbResult result;
  run_job(&plan, &result);
  // A following poll needs a short gap on the bus after this frame.
  delay(5);

  portENTER_CRITICAL(&job_lock);
  job_result = result;
  job_state = JOB_DONE;
  portEXIT_CRITICAL(&job_lock);
}

bool wsoft_screen_djb(const WsoftDjbPlan *plan, WsoftDjbResult *out, uint32_t timeout_ms) {
  if (out == nullptr) return false;
  memset(out, 0, sizeof *out);
  if (plan == nullptr || !plan->ok) {
    strlcpy(out->error, "bad_plan", sizeof out->error);
    return false;
  }
  portENTER_CRITICAL(&job_lock);
  // DONE with nobody waiting is a result whose caller timed out; it must not block for good.
  if (job_state != JOB_IDLE && job_state != JOB_DONE) {
    portEXIT_CRITICAL(&job_lock);
    strlcpy(out->error, "busy", sizeof out->error);
    return false;
  }
  job_plan = *plan;
  job_state = JOB_QUEUED;
  portEXIT_CRITICAL(&job_lock);

  uint32_t start = millis();
  for (;;) {
    portENTER_CRITICAL(&job_lock);
    int state = job_state;
    if (state == JOB_DONE) {
      *out = job_result;
      job_state = JOB_IDLE;
      portEXIT_CRITICAL(&job_lock);
      return true;
    }
    portEXIT_CRITICAL(&job_lock);

    uint32_t elapsed = millis() - start;
    if (state == JOB_QUEUED && elapsed > timeout_ms) {
      portENTER_CRITICAL(&job_lock);
      if (job_state == JOB_QUEUED) {
        job_state = JOB_IDLE;
        portEXIT_CRITICAL(&job_lock);
        strlcpy(out->error, "timeout", sizeof out->error);
        return false;
      }
      portEXIT_CRITICAL(&job_lock);
    } else if (elapsed > timeout_ms + 1000) {
      strlcpy(out->error, "timeout", sizeof out->error);
      return false;
    }
    delay(5);
  }
}

// ---------- main ----------

#if WSOFT_BOARD_CYD
// ---------- CYD panel (320 x 240) ----------

static TFT_eSPI tft;
static const int CYD_W = 320;

static void cyd_static();

static bool start_display() {
  tft.init();
  // CYD: rotation 3 is the unmirrored landscape (rotation 1 mirrors). Inversion is set
  // explicitly either way, because CYD batches ship with different panel controllers.
  tft.setRotation(WSOFT_CYD_ROTATION);
  tft.invertDisplay(WSOFT_CYD_INVERT ? true : false);
  cyd_static();
  return tft.width() == CYD_W;
}

// The same design as the Waveshare 4.3" screen (screen_rle.h), scaled to 320 x 240:
// navy header, light page with a white card, title + target line, gauge on the left,
// trade band + NET weight on the right, GROSS and ZONE boxes along the bottom.
// The static parts draw once; each live part redraws only when its content changes,
// so the panel does not flicker. Free fonts ignore the text background, so each live
// part clears its own box first.
static const uint16_t PAGE = 0xEF9E;     // light grey page behind the card
static const uint16_t BORDER = 0xD6BC;   // card and box outline
static const uint16_t BOX = 0xEF9F;      // pale blue GROSS / ZONE boxes
static const int GX = 90, GY = 132, GR = 40, GT = 9;  // gauge centre, radius, thickness

static void cyd_free(const char *text, int x, int y, const GFXfont *font, uint16_t fg, uint8_t datum) {
  tft.setFreeFont(font);
  tft.setTextDatum(datum);
  tft.setTextColor(fg);
  tft.drawString(text, x, y);
}

static void cyd_text(const char *text, int x, int y, int font, uint16_t fg, uint16_t bg, uint8_t datum) {
  tft.setTextDatum(datum);
  tft.setTextColor(fg, bg);
  tft.drawString(text, x, y, font);
}

// True (and remembers the new key) when a part's content changed since the last draw.
static bool cyd_changed(char *last, size_t size, const char *key) {
  if (strncmp(last, key, size) == 0) return false;
  strlcpy(last, key, size);
  return true;
}

static void cyd_gauge_arcs() {
  // TFT_eSPI arcs: 0 degrees is 6 o'clock, clockwise. 90..270 is the top half.
  tft.drawArc(GX, GY, GR, GR - GT, 90, 150, YELLOW, WHITE, true);
  tft.drawArc(GX, GY, GR, GR - GT, 150, 210, GREEN, WHITE, true);
  tft.drawArc(GX, GY, GR, GR - GT, 210, 270, RED, WHITE, true);
}

static void cyd_static() {
  tft.fillScreen(PAGE);
  tft.fillRect(0, 0, CYD_W, 30, INK);
  cyd_free("LIVE WEIGHT", 10, 15, &FreeSansBold9pt7b, WHITE, ML_DATUM);

  tft.fillRoundRect(6, 36, CYD_W - 12, 198, 8, WHITE);
  tft.drawRoundRect(6, 36, CYD_W - 12, 198, 8, BORDER);
  cyd_free("Live Weight", 16, 51, &FreeSansBold9pt7b, INK, ML_DATUM);
  char target[32];
  snprintf(target, sizeof target, "Target %.3f-%.3f kg", TARGET_LOW_KG, TARGET_HIGH_KG);
  cyd_text(target, 16, 64, 2, MUTED, WHITE, TL_DATUM);
  cyd_gauge_arcs();

  tft.fillRoundRect(14, 160, 142, 66, 6, BOX);
  tft.drawRoundRect(14, 160, 142, 66, 6, BORDER);
  cyd_text("GROSS", 22, 166, 2, MUTED, BOX, TL_DATUM);
  // The right box is the SAVE button (the zone word stays under the gauge); drawn by cyd_save_box.
}

// SAVE button, bottom right. Tapping it saves one weighing for the Hub.
static const int SAVE_X = 164, SAVE_Y = 160, SAVE_W = 142, SAVE_H = 66;

// Redraws the SAVE button when its text changes: the last result for 3 s, then how many
// weighings still wait for the Hub.
static void cyd_save_box() {
  static char last[48] = "";
  WeighingsInfo info;
  weighings_info(&info);
  bool recent = info.last_result != WSOFT_SAVE_NONE && millis() - info.last_result_ms < 3000;
  char line[32];
  uint16_t fill = GREEN;
  if (recent && info.last_result == WSOFT_SAVE_SAVED && info.last.valid) {
    snprintf(line, sizeof line, "SAVED #%lu", (unsigned long)info.last.seq);
  } else if (recent) {
    strlcpy(line, wsoft_save_result_text(info.last_result), sizeof line);
    fill = info.last_result == WSOFT_SAVE_ALREADY ? MUTED : RED;
  } else if (info.pending > 0) {
    snprintf(line, sizeof line, "%u to send", (unsigned)info.pending);
  } else {
    strlcpy(line, info.auto_save ? "AUTO SAVE ON" : "TAP TO SAVE", sizeof line);
  }
  char key[48];
  snprintf(key, sizeof key, "%s|%u", line, fill);
  if (!cyd_changed(last, sizeof last, key)) return;
  tft.fillRoundRect(SAVE_X, SAVE_Y, SAVE_W, SAVE_H, 6, fill);
  cyd_free("SAVE", SAVE_X + SAVE_W / 2, SAVE_Y + 24, &FreeSansBold12pt7b, WHITE, MC_DATUM);
  cyd_text(line, SAVE_X + SAVE_W / 2, SAVE_Y + 46, 2, WHITE, fill, TC_DATUM);
}

static SPIClass touch_spi(HSPI);
static bool touch_ready = false;
static const uint16_t TOUCH_Z_MIN = 400;  // pressure below this is no touch

struct TouchPoint {
  int x, y, z;
};

static void cyd_touch_begin() {
  pinMode(WSOFT_TOUCH_CS, OUTPUT);
  digitalWrite(WSOFT_TOUCH_CS, HIGH);
  pinMode(WSOFT_TOUCH_IRQ, INPUT);
  touch_spi.begin(WSOFT_TOUCH_CLK, WSOFT_TOUCH_MISO, WSOFT_TOUCH_MOSI, -1);
  touch_ready = true;
  Serial0.println("TOUCH ready");
}

// One XPT2046 read: pressure, then X and Y (average of two readings after a dropped first one).
static bool cyd_touch_read(TouchPoint *p) {
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
  int x = (x1 + x2) / 2, y = (y1 + y2) / 2;
  switch (WSOFT_TOUCH_ROTATION) {
    case 0: p->x = 4095 - y; p->y = x; break;
    case 1: p->x = x; p->y = y; break;
    case 2: p->x = y; p->y = 4095 - x; break;
    default: p->x = 4095 - x; p->y = 4095 - y; break;
  }
  p->z = z;
  return true;
}

// Runs every loop pass (every 5 ms), so a short tap is not missed. One save per press.
static void cyd_touch_poll() {
  static bool was_down = false;
  static uint32_t last_tap = 0;
  if (!touch_ready) return;
  TouchPoint p;
  bool down = cyd_touch_read(&p);
  if (down && !was_down && millis() - last_tap > 600) {
    int x = constrain(map(p.x, WSOFT_TOUCH_X_MIN, WSOFT_TOUCH_X_MAX, 0, CYD_W - 1), 0, CYD_W - 1);
    int y = constrain(map(p.y, WSOFT_TOUCH_Y_MIN, WSOFT_TOUCH_Y_MAX, 0, 239), 0, 239);
    bool on_save = x >= SAVE_X - 10 && x <= SAVE_X + SAVE_W + 10 && y >= SAVE_Y - 10;
    Serial0.printf("TOUCH raw=%d,%d,%d screen=%d,%d %s\n", p.x, p.y, p.z, x, y, on_save ? "SAVE" : "-");
    if (on_save) weighings_request_save();
    last_tap = millis();
  }
  was_down = down;
}

static void cyd_box_value(int x, const char *text, uint16_t color) {
  tft.fillRect(x + 4, 186, 134, 36, BOX);
  cyd_free(text, x + 8, 206, &FreeSansBold12pt7b, color, ML_DATUM);
}

static void render(const Reading &r) {
  static char last_head[40], last_band[40], last_audit[20], last_weight[40], last_gauge[40], last_boxes[64];
  char key[64];

  // Header, right side: Hub state, then the link state with a colour dot.
  HubState hub = hub_state();
  const char *link = !r.linked ? "NO SIGNAL" : r.alarm ? "DJB ALARM" : rs232_mode ? "RS232 LIVE" : "RS485 LIVE";
  bool link_ok = r.linked && !r.alarm;
  snprintf(key, sizeof key, "%s|%s", link, hub_state_text(hub));
  if (cyd_changed(last_head, sizeof last_head, key)) {
    tft.fillRect(130, 0, CYD_W - 130, 30, INK);
    cyd_text(link, CYD_W - 8, 8, 2, link_ok ? HEADER_OK : HEADER_BAD, INK, TR_DATUM);
    int link_left = CYD_W - 8 - tft.textWidth(link, 2);
    tft.fillCircle(link_left - 8, 15, 4, link_ok ? HEADER_OK : HEADER_BAD);
    cyd_text(hub_state_text(hub), link_left - 20, 8, 2, hub == HUB_ONLINE ? HEADER_OK : HEADER_BAD, INK, TR_DATUM);
  }

  // Top right of the card: the trade band, and why it is not for trade (or the sequence).
  TradeCheck check = approval_check();
  bool trade = check == TRADE_OK;
  char line[24];
  uint16_t why_color;
  const char *why = trade_line(check, line, sizeof line, &why_color);
  snprintf(key, sizeof key, "%d|%s|%u", trade ? 1 : 0, why, why_color);
  if (cyd_changed(last_band, sizeof last_band, key)) {
    uint16_t band = trade ? GREEN : RED;
    tft.fillRect(172, 42, 134, 20, band);
    cyd_text(trade ? "TRADE CHECKS ON" : "NOT FOR TRADE", 239, 45, 2, WHITE, band, TC_DATUM);
    tft.fillRect(172, 64, 134, 16, WHITE);
    cyd_text(why, 172, 64, 2, why_color, WHITE, TL_DATUM);
  }

  // NET label row carries the audit counter on the right.
  char audit[20];
  snprintf(audit, sizeof audit, "AUDIT %lu", (unsigned long)approval_audit_counter());
  if (cyd_changed(last_audit, sizeof last_audit, audit)) {
    tft.fillRect(172, 84, 134, 16, WHITE);
    cyd_text("NET", 172, 84, 2, MUTED, WHITE, TL_DATUM);
    cyd_text(audit, 306, 84, 2, MUTED, WHITE, TR_DATUM);
  }

  char value[16] = "----", gross[24] = "----";
  const char *unit = "kg";
  uint16_t zone_color = RED;
  const char *zone = rs232_mode ? "NO SCALE LINE" : "NO DJB REPLY";
  if (r.linked) {
    format_weight(r.value, r.decimals, value, sizeof value);
    unit = weight_unit_text(r.unit);
    snprintf(gross, sizeof gross, "%s %s", value, unit);
    zone = zone_of(r, &zone_color);
  }

  // NET weight: as big as it fits right-aligned before the unit.
  snprintf(key, sizeof key, "%s|%s|%d", value, unit, r.linked ? 1 : 0);
  if (cyd_changed(last_weight, sizeof last_weight, key)) {
    tft.fillRect(170, 102, 138, 50, WHITE);
    const GFXfont *big = &FreeSansBold24pt7b;
    tft.setFreeFont(big);
    if (tft.textWidth(value) > 104) big = &FreeSansBold18pt7b;
    cyd_free(value, 276, 128, big, r.linked ? INK : MUTED, MR_DATUM);
    cyd_text(unit, 280, 126, 2, MUTED, WHITE, TL_DATUM);
  }

  // Gauge needle and the word under it.
  int angle = 0;
  if (r.linked) angle = (int)lroundf(constrain(to_kg(r) / GAUGE_FULL_KG, 0.0f, 1.0f) * 180.0f);
  snprintf(key, sizeof key, "%d|%d|%s", r.linked ? 1 : 0, angle, r.linked ? zone : "WAITING");
  if (cyd_changed(last_gauge, sizeof last_gauge, key)) {
    tft.fillRect(GX - GR, GY - GR + GT, 2 * GR, GR - GT + 24, WHITE);
    cyd_gauge_arcs();
    if (r.linked) {
      float rad = (180.0f - angle) * PI / 180.0f;
      int len = GR - GT - 4;
      tft.drawWideLine(GX, GY, GX + cosf(rad) * len, GY - sinf(rad) * len, 4, INK, WHITE);
      tft.fillCircle(GX, GY, 5, INK);
    }
    cyd_text(r.linked ? zone : "WAITING", GX, GY + 6, 2, r.linked ? zone_color : MUTED, WHITE, TC_DATUM);
  }

  // GROSS box (the SAVE button beside it redraws itself every pass).
  snprintf(key, sizeof key, "%s|%d", gross, r.linked ? 1 : 0);
  if (cyd_changed(last_boxes, sizeof last_boxes, key)) cyd_box_value(14, gross, r.linked ? INK : MUTED);
}
#else
static bool start_display() {
  board = new Board();
  board->init();
  auto *expander = board->getIO_Expander();
  // After a cold power-on the CH422G can miss the first few I2C writes.
  bool expander_ok = false;
  for (int attempt = 1; expander != nullptr && attempt <= 20 && !expander_ok; attempt++) {
    expander_ok = expander->begin();
    if (!expander_ok) {
      Serial0.printf("expander begin failed, attempt %d\n", attempt);
      delay(200);
    }
  }
  if (!expander_ok) {
    log_line("expander gave up");
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
  if (!bus->configRGB_BounceBufferSize(SCREEN_W * 10) || !lcd->begin()) {
    log_line("lcd begin failed");
    return false;
  }

  const size_t bytes = SCREEN_W * SCREEN_H * sizeof(uint16_t);
  background = static_cast<uint16_t *>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  frame = static_cast<uint16_t *>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (background == nullptr || frame == nullptr) {
    log_line("frame alloc failed");
    lcd->colorBarTest();
    return false;
  }
  expand_background(background);
  return true;
}
#endif  // WSOFT_BOARD_CYD

static bool display_ready = false;
static int fails = FAILS_BEFORE_NO_SIGNAL;
static uint32_t polls = 0;

// ---------- snapshot for the web services (Hardware.Base, M3) ----------

static portMUX_TYPE snapshot_lock = portMUX_INITIALIZER_UNLOCKED;
static WSoftSnapshot snapshot;

static void publish_snapshot(const Reading &r, bool fresh, TradeCheck check, HubState hub) {
  WSoftSnapshot next;
  next.valid = true;
  next.display_ready = display_ready;
  next.linked = r.linked;
  next.fresh = fresh;
  next.alarm = r.alarm;
  next.value = r.value;
  next.decimals = r.decimals;
  next.unit = r.unit;
  next.approval_on = approval_on();
  next.stable = approval_stable();
  next.trade_check = check;
  next.audit_counter = approval_audit_counter();
  next.last_trade_seq = approval_last_trade_weight().seq;
  next.trade_queue = (uint32_t)approval_pending();
  next.hub_online = hub == HUB_ONLINE;
  strlcpy(next.hub_text, hub_state_text(hub), sizeof next.hub_text);
  next.polls = polls;
  next.updated_ms = millis();
  next.rs232 = rs232_mode;
  portENTER_CRITICAL(&snapshot_lock);
  snapshot = next;
  portEXIT_CRITICAL(&snapshot_lock);
}

void wsoft_screen_snapshot(WSoftSnapshot *out) {
  portENTER_CRITICAL(&snapshot_lock);
  *out = snapshot;
  portEXIT_CRITICAL(&snapshot_lock);
}

void wsoft_screen_setup(bool rs232) {
  rs232_mode = rs232;
#if WSOFT_BOARD_CYD
  // UART0 is the CH340 USB port on the CYD (default pins GPIO1 / GPIO3).
  Serial0.begin(115200);
#else
  // UART0 is the CH343 port on this board: TX GPIO43, RX GPIO44.
  Serial0.begin(115200, SERIAL_8N1, 44, 43);
#endif
  log_line(rs232_mode ? "weigh screen boot (RS232 scale)" : "weigh screen boot (RS485 junction box)");
  if (!rs232_mode) Serial1.begin(DJB_BAUD, SERIAL_8N1, RS485_RX, RS485_TX);

  display_ready = start_display();
  log_line(display_ready ? "display ready" : "display failed");
  scale_begin(!rs232_mode);
  pin_begin();
  approval_begin();
  weighings_begin(rs232_mode);
  hub_begin();
#if WSOFT_BOARD_CYD
  cyd_touch_begin();
#endif
  if (display_ready) render(reading);
  portENTER_CRITICAL(&snapshot_lock);
  snapshot.display_ready = display_ready;
  portEXIT_CRITICAL(&snapshot_lock);
}

// One RS485 poll of the DJB: unit and decimals every 20 polls, then weight and alarm.
static bool poll_djb(Reading *out) {
  Reading &next = *out;
  if (polls++ % 20 == 0 || format_stale) {
    format_stale = false;
    int32_t decimals, unit;
    if (read_long(REG_DECIMAL, &decimals) && decimals >= 0 && decimals <= 4) next.decimals = decimals;
    if (read_long(REG_WEIGHT_UNIT, &unit) && unit >= 0 && unit <= 3) next.unit = unit;
  }

  uint16_t words[4];
  bool fresh = read_holding(REG_LC1_WEIGHT, 4, words);
  if (fresh) {
    fails = 0;
    next.linked = true;
    next.value = (int32_t)(((uint32_t)words[0] << 16) | words[1]);
    next.alarm = ((((uint32_t)words[2] << 16) | words[3]) & ALARM_BLOCKING_MASK) != 0;
  } else if (++fails >= FAILS_BEFORE_NO_SIGNAL) {
    next.linked = false;
    Serial0.printf("DJB no reply slave=%u baud=%lu rx_bytes=%u", slave, (unsigned long)SCAN_BAUDS[baud_index],
                   (unsigned)last_rx_bytes);
    for (size_t i = 0; i < last_rx_bytes && i < sizeof last_rx; i++) Serial0.printf(" %02X", last_rx[i]);
    Serial0.println();
    next_scan_target();
  }
  return fresh;
}

// One RS232 pass: the newest weight the Serial reader extracted, stamped when its line
// arrived. Fresh and linked while that reading is inside the live window, so a 1-2 Hz
// scale stays fresh between lines and can settle. The scale sends no DJB alarm, unit or
// decimals: unit is kg, decimals come from the text.
static bool poll_rs232(Reading *out) {
  char text[24];
  uint32_t stamp = 0;
  bool have = line_reader != nullptr && line_reader(text, sizeof text, &stamp);
  int32_t value;
  int decimals;
  bool parsed = have && wsoft_parse_weight_text(text, &value, &decimals);
  bool fresh = parsed && millis() - stamp < RS232_STALE_MS;
  out->alarm = false;
  out->unit = 1;
  out->linked = fresh;
  if (fresh) {
    out->value = value;
    out->decimals = decimals;
  }
  return fresh;
}

void wsoft_screen_loop() {
  static uint32_t last_poll = 0;
  static HubState shown_hub = HUB_SETUP;
  hub_handle_serial(Serial0);
  hub_ota_poll();
  service_job();
  switches_poll();
#if WSOFT_BOARD_CYD
  cyd_touch_poll();
#endif
  if (hub_ota_busy() || !display_ready || millis() - last_poll < POLL_MS) return;
  last_poll = millis();

  Reading next = reading;
  bool fresh;
  if (rs232_mode) {
    fresh = poll_rs232(&next);
    polls++;
  } else {
    fresh = poll_djb(&next);
  }

  TradeCheck check = approval_update(next.linked, fresh, next.alarm, next.value, next.decimals, next.unit);
  // A weighing for the Hub (Save tap or Auto save), saved whether trade checks are on or not.
  weighings_update(next.linked, fresh, next.alarm, next.value, next.decimals, next.unit, check == TRADE_OK);
#if WSOFT_BOARD_CYD
  cyd_save_box();
#endif

  if (next.linked && polls % 4 == 1) {
    if (!rs232_mode) Serial0.printf("DJB slave=%u baud=%lu ", slave, (unsigned long)SCAN_BAUDS[baud_index]);
    char value[16];
    format_weight(next.value, next.decimals, value, sizeof value);
    Serial0.printf("%s %s weight=%s %s raw=%ld alarm=%d stable=%d %s\n", rs232_mode ? "RS232" : "DJB", next.linked ? "LIVE" : "NO_SIGNAL", value,
                   weight_unit_text(next.unit), (long)next.value, next.alarm ? 1 : 0, approval_stable() ? 1 : 0,
                   check == TRADE_OK ? "TRADE_CHECKS_ON" : "NOT_FOR_TRADE");
  }

  char weight[24];
  format_weight(next.value, next.decimals, weight, sizeof weight);
  strlcat(weight, " ", sizeof weight);
  strlcat(weight, weight_unit_text(next.unit), sizeof weight);
  hub_set_health(next.linked, next.linked ? weight : "", next.alarm);

  HubState hub = hub_state();
  publish_snapshot(next, fresh, check, hub);
  static TradeCheck shown_check = TRADE_OFF;
  static uint32_t shown_audit = 0, shown_seq = 0;
  uint32_t audit = approval_audit_counter(), seq = approval_last_trade_weight().seq;
  bool approval_changed = check != shown_check || audit != shown_audit || seq != shown_seq;
  shown_check = check;
  shown_audit = audit;
  shown_seq = seq;
  if (approval_changed || next.linked != reading.linked || next.value != reading.value || next.alarm != reading.alarm ||
      next.decimals != reading.decimals || next.unit != reading.unit || hub != shown_hub || polls == 1) {
    reading = next;
    shown_hub = hub;
    render(reading);
  }
}
