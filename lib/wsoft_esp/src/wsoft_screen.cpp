#include <Arduino.h>
#include <esp_display_panel.hpp>
#include <esp_expander_ch422g.hpp>
#include <esp_heap_caps.h>
#include <math.h>

#include "approval.h"
#include "fonts.h"
#include "hub.h"
#include "screen_rle.h"
#include "wsoft_screen.h"

using namespace esp_panel::board;
using namespace esp_panel::drivers;

// The Waveshare wiki lists TXD GPIO15 / RXD GPIO16, but on this board (bench, 2026-10-24) the
// transceiver output reaches GPIO15, so RX is 15 and TX is 16. Direction switching is automatic.
static const int RS485_RX = 15;
static const int RS485_TX = 16;

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

// Demo target band, matching the static "Target 1.000 - 2.000 kg" line.
static const float TARGET_LOW_KG = 1.0f;
static const float TARGET_HIGH_KG = 2.0f;
static const float GAUGE_FULL_KG = 3.0f;
static const int GAUGE_CX = 250, GAUGE_CY = 292, GAUGE_R = 92;

static const uint16_t INK = 0x1149;
static const uint16_t MUTED = 0x6390;
static const uint16_t GREEN = 0x1469;
static const uint16_t YELLOW = 0xD524;
static const uint16_t RED = 0xC1C6;
static const uint16_t HEADER_OK = 0x7EF2;
static const uint16_t HEADER_BAD = 0xFB2C;
static const uint16_t WHITE = 0xFFFF;

static Board *board = nullptr;
static LCD *lcd = nullptr;
static uint16_t *background = nullptr;
static uint16_t *frame = nullptr;

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

static float to_kg(const Reading &r) {
  float value = r.value / powf(10.0f, r.decimals);
  if (r.unit == 2) return value * 1000.0f;
  if (r.unit == 3) return value / 1000.0f;
  return value;
}

// Top right of the card: the trade band, then why it is not for trade (or the sequence), and the audit counter.
static void draw_approval() {
  TradeCheck check = approval_check();
  bool trade = check == TRADE_OK;
  fill_rect(440, 100, 760, 144, trade ? GREEN : RED);
  draw_text(MID, 600, 105, trade ? "TRADE CHECKS ON" : "NOT FOR TRADE", WHITE, CENTER);

  char line[24];
  if (trade) {
    snprintf(line, sizeof line, "SEQ %lu", (unsigned long)approval_last_trade_weight().seq);
    draw_text(MID, 440, 152, line, GREEN);
  } else if (check == TRADE_OFF) {
    draw_text(MID, 440, 152, "APPROVAL OFF", MUTED);
  } else {
    const char *why = "CHECK";
    switch (check) {
      case TRADE_NO_SIGNAL: why = "NO SIGNAL"; break;
      case TRADE_STALE: why = "NO REPLY"; break;
      case TRADE_QUEUE_FULL: why = "QUEUE FULL"; break;
      case TRADE_ALARM: why = "ALARM"; break;
      case TRADE_MOTION: why = "MOTION"; break;
      case TRADE_EMPTY: why = "EMPTY"; break;
      case TRADE_NO_CLOCK: why = "NO CLOCK"; break;
      default: break;
    }
    draw_text(MID, 440, 152, why, YELLOW);
  }
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
    const char *zone = "OK";
    uint16_t zone_color = GREEN;
    if (r.alarm) {
      zone = "ALARM";
      zone_color = RED;
    } else if (kg < TARGET_LOW_KG) {
      zone = "UNDER";
      zone_color = YELLOW;
    } else if (kg > TARGET_HIGH_KG) {
      zone = "OVER";
      zone_color = RED;
    }

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
  if (!reading.linked) {
    strlcpy(out->error, "not_linked", sizeof out->error);
    return;
  }
  // Approval has no web write path: an AD write is a calibration and the scale
  // parameters are metrological. Writes need approval off (USB APPROVAL OFF).
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
  portENTER_CRITICAL(&snapshot_lock);
  snapshot = next;
  portEXIT_CRITICAL(&snapshot_lock);
}

void wsoft_screen_snapshot(WSoftSnapshot *out) {
  portENTER_CRITICAL(&snapshot_lock);
  *out = snapshot;
  portEXIT_CRITICAL(&snapshot_lock);
}

void wsoft_screen_setup() {
  // UART0 is the CH343 port on this board: TX GPIO43, RX GPIO44.
  Serial0.begin(115200, SERIAL_8N1, 44, 43);
  log_line("weigh screen boot");
  Serial1.begin(DJB_BAUD, SERIAL_8N1, RS485_RX, RS485_TX);

  display_ready = start_display();
  log_line(display_ready ? "display ready" : "display failed");
  approval_begin();
  hub_begin();
  if (display_ready) render(reading);
  portENTER_CRITICAL(&snapshot_lock);
  snapshot.display_ready = display_ready;
  portEXIT_CRITICAL(&snapshot_lock);
}

void wsoft_screen_loop() {
  static uint32_t last_poll = 0;
  static HubState shown_hub = HUB_SETUP;
  hub_handle_serial(Serial0);
  hub_ota_poll();
  service_job();
  if (hub_ota_busy() || !display_ready || millis() - last_poll < POLL_MS) return;
  last_poll = millis();

  Reading next = reading;
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

  TradeCheck check = approval_update(next.linked, fresh, next.alarm, next.value, next.decimals, next.unit);

  if (next.linked && polls % 4 == 1) {
    Serial0.printf("DJB slave=%u baud=%lu ", slave, (unsigned long)SCAN_BAUDS[baud_index]);
    char value[16];
    format_weight(next.value, next.decimals, value, sizeof value);
    Serial0.printf("DJB %s weight=%s %s raw=%ld alarm=%d stable=%d %s\n", next.linked ? "LIVE" : "NO_SIGNAL", value,
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
