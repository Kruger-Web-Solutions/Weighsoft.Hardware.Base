#include <Arduino.h>

#include "wsoft_log.h"

#include "approval.h"
#include "approval_base.h"
#include "hilo.h"
#include "hub.h"
#include "pin.h"
#include "relay.h"
#include "scale.h"
#include "switches.h"
#include "weighings.h"
#include "wsoft_screen.h"
#include "wsoft_ui.h"

// The weigh loop, the same on every board: read the scale (the RS485 junction box, an RS232
// line or a weigh board in Writer mode), run approval, weighings, the Hi-Lo relays and the
// Hub, publish the snapshot, and hand the screen one view per poll. What is drawn and tapped
// is in wsoft_ui.cpp; only the scale UART pins differ here.
#ifndef WSOFT_BOARD_HEADLESS
#define WSOFT_BOARD_HEADLESS 0
#endif
#ifndef WSOFT_BOARD_CYD
#define WSOFT_BOARD_CYD 0
#endif
#if WSOFT_BOARD_HEADLESS || WSOFT_BOARD_CYD
// Headless S3: a TTL converter or TTL-to-RS485 module on the env's pins. CYD: an external
// 3.3 V TTL-to-RS485 module with automatic direction on CN1 (GPIO22 / GPIO27).
static const int RS485_RX = WSOFT_RS485_RX;
static const int RS485_TX = WSOFT_RS485_TX;
#else
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
static const uint16_t REG_DIVISION = 1306;     // 41307: e in display steps (1, 2, 5, 10 ... 500)
// Which alarm bits block, and which are allowed: wsoft_djb_alarm_check (wsoft_djb.h).

static const uint32_t POLL_MS = 250;
static const int FAILS_BEFORE_NO_SIGNAL = 3;
// RS232: a scale line older than this is no longer live (same 3 s stale rule as the Pi).
static const uint32_t RS232_STALE_MS = 3000;

// line_mode: the weight comes from the line reader (an RS232 scale, or a weigh board in
// Writer mode), never from Serial1. writer_mode: this board follows a weigh board and saves
// no weighings (WSOFT_SAVE_WRITER).
static bool line_mode = false;
static bool writer_mode = false;
static WsoftLineReader line_reader = nullptr;

void wsoft_screen_set_line_reader(WsoftLineReader reader) {
  line_reader = reader;
}

struct Reading {
  bool linked = false;
  bool alarm = false;          // a blocking alarm bit that is not on the allowed list
  bool alarm_allowed = false;  // an allowed alarm bit is set: flagged, never blocking
  bool alarm_word_valid = false;
  uint32_t alarm_word = 0;     // raw DJB alarm state of the last fresh read
  int32_t value = 0;
  int decimals = 3;
  int unit = 1;
};

static void log_line(const char *message) {
  WSOFT_LOG.println(message);
}

static bool display_ready = false;
static int fails = FAILS_BEFORE_NO_SIGNAL;
static uint32_t polls = 0;

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
  if (line_mode) {
    strlcpy(out->error, "line_mode", sizeof out->error);
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

// ---------- snapshot for the web services (Hardware.Base, M3) ----------

static portMUX_TYPE snapshot_lock = portMUX_INITIALIZER_UNLOCKED;
static WSoftSnapshot snapshot;
// Every change in the DJB alarm word since boot (Rhyno, 14 Nov 2026: tell me if it changes).
static WsoftDjbAlarmWatch alarm_watch;

static void publish_snapshot(const Reading &r, bool fresh, TradeCheck check, HubState hub, bool tared, int32_t net) {
  WSoftSnapshot next;
  next.tared = tared;
  next.net_value = net;
  next.valid = true;
  next.display_ready = display_ready;
  next.linked = r.linked;
  next.fresh = fresh;
  next.alarm = r.alarm;
  next.alarm_allowed = r.alarm_allowed;
  next.alarm_word_valid = r.alarm_word_valid;
  next.alarm_word = r.alarm_word;
  next.alarm_changes = alarm_watch.changes();
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
  next.rs232 = line_mode && !writer_mode;
  next.writer = writer_mode;
  WeighingsInfo weigh;
  weighings_info(&weigh);
  next.save_waiting = weigh.waiting;
  portENTER_CRITICAL(&snapshot_lock);
  snapshot = next;
  portEXIT_CRITICAL(&snapshot_lock);
}

void wsoft_screen_snapshot(WSoftSnapshot *out) {
  portENTER_CRITICAL(&snapshot_lock);
  *out = snapshot;
  portEXIT_CRITICAL(&snapshot_lock);
}

void wsoft_screen_setup(WsoftScreenSource source) {
  writer_mode = source == WSOFT_SCREEN_WRITER;
  line_mode = source != WSOFT_SCREEN_RS485;
#if defined(WSOFT_LOG_USB) && WSOFT_LOG_USB
  // The S3's own USB port. Never wait on it: with no PC reading, prints are dropped.
  WSOFT_LOG.begin(115200);
  WSOFT_LOG.setTxTimeoutMs(0);
#elif WSOFT_BOARD_CYD
  // UART0 is the CH340 USB port on the CYD (default pins GPIO1 / GPIO3).
  WSOFT_LOG.begin(115200);
#else
  // UART0 is the CH343 port on this board: TX GPIO43, RX GPIO44.
  WSOFT_LOG.begin(115200, SERIAL_8N1, 44, 43);
#endif
  log_line(writer_mode ? "weigh screen boot (Writer: weight from a weigh board over Wi-Fi)"
             : line_mode ? "weigh screen boot (RS232 scale)"
                         : "weigh screen boot (RS485 junction box)");
  if (!line_mode) Serial1.begin(DJB_BAUD, SERIAL_8N1, RS485_RX, RS485_TX);

  display_ready = ui_begin();
  log_line(display_ready ? "display ready" : WSOFT_BOARD_HEADLESS ? "no display (headless board)" : "display failed");
  scale_begin(!line_mode);
  pin_begin();
  approval_begin();
  weighings_begin(line_mode && !writer_mode);
  hilo_begin();
  hub_begin();
  portENTER_CRITICAL(&snapshot_lock);
  snapshot.display_ready = display_ready;
  portEXIT_CRITICAL(&snapshot_lock);
}

// One RS485 poll of the DJB: unit and decimals every 20 polls, then weight and alarm.
static bool poll_djb(Reading *out) {
  Reading &next = *out;
  // e (division) is read once the card answers, and again after a card write.
  static int32_t division = 0;
  if (polls++ % 20 == 0 || format_stale) {
    bool read_division = format_stale || division <= 0;
    format_stale = false;
    int32_t decimals, unit, value;
    bool format_ok = read_long(REG_DECIMAL, &decimals) && decimals >= 0 && decimals <= 4;
    if (format_ok) next.decimals = decimals;
    bool unit_ok = read_long(REG_WEIGHT_UNIT, &unit) && unit >= 0 && unit <= 3;
    if (unit_ok) next.unit = unit;
    if (read_division && format_ok && unit_ok && read_long(REG_DIVISION, &value) && value > 0) division = value;
    if (division > 0 && format_ok && unit_ok) scale_set_card(division, next.decimals, next.unit);
  }

  uint16_t words[4];
  bool fresh = read_holding(REG_LC1_WEIGHT, 4, words);
  if (fresh) {
    fails = 0;
    next.linked = true;
    next.value = (int32_t)(((uint32_t)words[0] << 16) | words[1]);
    uint32_t alarm_word = ((uint32_t)words[2] << 16) | words[3];
    WsoftDjbAlarmCheck alarm_check = wsoft_djb_alarm_check(alarm_word);
    next.alarm = alarm_check.blocking;
    next.alarm_allowed = alarm_check.allowed;
    next.alarm_word = alarm_word;
    next.alarm_word_valid = true;
  } else if (++fails >= FAILS_BEFORE_NO_SIGNAL) {
    next.linked = false;
    WSOFT_LOG.printf("DJB no reply slave=%u baud=%lu rx_bytes=%u", slave, (unsigned long)SCAN_BAUDS[baud_index],
                   (unsigned)last_rx_bytes);
    for (size_t i = 0; i < last_rx_bytes && i < sizeof last_rx; i++) WSOFT_LOG.printf(" %02X", last_rx[i]);
    WSOFT_LOG.println();
    next_scan_target();
  }
  return fresh;
}

// One line pass (RS232 scale or Writer): the newest weight the line reader holds, stamped
// when it arrived. Fresh and linked while that reading is inside the live window, so a 1-2 Hz
// scale stays fresh between lines and can settle. No DJB alarm; decimals come from the text.
// The unit is kg unless the reader says otherwise (a Writer gets it from the weigh board).
static bool poll_line(Reading *out) {
  char text[24];
  uint32_t stamp = 0;
  int unit = 1;
  bool have = line_reader != nullptr && line_reader(text, sizeof text, &stamp, &unit);
  int32_t value;
  int decimals;
  bool parsed = have && wsoft_parse_weight_text(text, &value, &decimals);
  bool fresh = parsed && millis() - stamp < RS232_STALE_MS;
  out->alarm = false;
  out->alarm_allowed = false;
  out->alarm_word_valid = false;
  out->unit = unit;
  out->linked = fresh;
  if (fresh) {
    out->value = value;
    out->decimals = decimals;
  }
  return fresh;
}

void wsoft_screen_loop() {
  static uint32_t last_poll = 0;
  hub_handle_serial(WSOFT_LOG);
  hub_ota_poll();
  service_job();
  switches_poll();
  ui_loop();
  // The headless board has no panel and polls anyway.
  if (hub_ota_busy() || (!display_ready && !WSOFT_BOARD_HEADLESS) || millis() - last_poll < POLL_MS) return;
  last_poll = millis();

  Reading next = reading;
  bool fresh;
  if (line_mode) {
    fresh = poll_line(&next);
    polls++;
  } else {
    fresh = poll_djb(&next);
    uint32_t before = alarm_watch.last();
    bool first = !alarm_watch.seen();
    if (alarm_watch.update(fresh && next.alarm_word_valid, next.alarm_word)) {
      WSOFT_LOG.printf("DJB alarm CHANGED %08lX -> %08lX (change %lu since boot)%s\n", (unsigned long)before,
                       (unsigned long)next.alarm_word, (unsigned long)alarm_watch.changes(),
                       next.alarm ? " BLOCKING" : next.alarm_allowed ? " allowed, monitoring" : "");
    } else if (first && alarm_watch.seen()) {
      WSOFT_LOG.printf("DJB alarm %08lX at start%s\n", (unsigned long)next.alarm_word,
                       next.alarm ? " BLOCKING" : next.alarm_allowed ? ": EEPROM allowed, monitoring" : "");
    }
  }

  // RS485 setup: blanking, below Min and the stable samples hold a reading back (NONE on RS232).
  int32_t net = next.value;
  bool tared = scale_net(next.value, next.decimals, next.unit, &net);
  WsoftScaleHold hold = scale_hold(next.linked, fresh, next.value, net, next.decimals, next.unit);
  TradeCheck check = approval_update_held(next.linked, fresh, next.alarm, next.value, next.decimals, next.unit, hold);
  WsoftSaveResult save_hold = WSOFT_SAVE_NONE;
  if (hold == WSOFT_HOLD_BLANK) save_hold = WSOFT_SAVE_BLANK;
  else if (hold == WSOFT_HOLD_NOT_STABLE) save_hold = WSOFT_SAVE_NOT_STABLE;
  else if (hold == WSOFT_HOLD_BELOW_MIN && approval_on()) save_hold = WSOFT_SAVE_BELOW_MIN;
  // Writer mode: the weigh board saves; this board never does (no weighing counted twice).
  if (writer_mode) save_hold = WSOFT_SAVE_WRITER;
  // A weighing for the Hub (Save tap or Auto save), saved whether trade checks are on or not.
  WsoftHiloProduct product;
  bool picked = hilo_current(&product);
  weighings_update(next.linked, fresh, next.alarm, next.value, next.decimals, next.unit, check == TRADE_OK,
                   tared ? scale_tare().value : 0, save_hold, picked ? &product : nullptr);
  // The Hi-Lo relay zone, judged on the net weight like the word on the screen and the saved
  // weighing (it was the gross: with a tare on, relays and screen disagreed). Posted every
  // poll; the relay service (if this board has one) drives the pins, and the screen colours
  // its weight panel with the same zone.
  WsoftRelayInput relay_in;
  relay_in.linked = next.linked;
  relay_in.alarm = next.alarm;
  relay_in.blanked = hold == WSOFT_HOLD_BLANK;
  relay_in.picked = picked;
  relay_in.value = net;
  relay_in.state = picked ? wsoft_hilo_check(product, net, next.decimals, next.unit) : WSOFT_HILO_NONE;
  relay_post(relay_in, fresh);

  if (next.linked && polls % 4 == 1) {
    if (!line_mode) WSOFT_LOG.printf("DJB slave=%u baud=%lu ", slave, (unsigned long)SCAN_BAUDS[baud_index]);
    char value[16];
    format_weight(next.value, next.decimals, value, sizeof value);
    WSOFT_LOG.printf("%s %s weight=%s %s raw=%ld alarm=%d stable=%d %s\n", line_mode ? "RS232" : "DJB",
                     next.linked ? "LIVE" : "NO_SIGNAL", value, weight_unit_text(next.unit), (long)next.value,
                     next.alarm ? 1 : 0, approval_stable() ? 1 : 0, check == TRADE_OK ? "TRADE_CHECKS_ON" : "NOT_FOR_TRADE");
  }

  char weight[24];
  format_weight(next.value, next.decimals, weight, sizeof weight);
  strlcat(weight, " ", sizeof weight);
  strlcat(weight, weight_unit_text(next.unit), sizeof weight);
  hub_set_health(next.linked, next.linked ? weight : "", next.alarm);
  publish_snapshot(next, fresh, check, hub_state(), tared, net);
  reading = next;

  // The screen paints only the rows whose words changed.
  WsoftUiView view;
  view.linked = next.linked;
  view.alarm = next.alarm;
  view.gross = next.value;
  view.net = net;
  view.tared = tared;
  view.decimals = next.decimals;
  view.unit = next.unit;
  view.hold = hold;
  view.check = check;
  view.source = writer_mode ? "WRITER" : line_mode ? "RS232" : "RS485";
  view.writer = writer_mode;
  ui_show(view);
}
