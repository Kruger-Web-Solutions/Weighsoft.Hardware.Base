#include "approval.h"

#include <Preferences.h>
#include <time.h>

static const int LOG_SLOTS = 8;
// Queued trade weights sent per heartbeat; the rest follow in the next ones.
static const size_t SEND_PER_HEARTBEAT = 8;

static Preferences prefs;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

// Shared with the Hub task, so read and written under the lock.
static bool on = false;
static uint32_t audit_counter = 0;
static TradeCheck check = TRADE_OFF;
static bool stable = false;
static bool trade_current = false;
static TradeWeight last_trade;
static TradeQueue queue;
static uint32_t delivered_seq = 0;  // set by the Hub task, applied by the main loop

// Main loop only.
static TradeEngine engine;
static uint32_t applied_delivered_seq = 0;

// Fixed layout for a queued trade weight in flash.
struct StoredTrade {
  uint32_t seq;
  int32_t value;
  int32_t decimals;
  int32_t unit;
  int64_t at;
};

static void format_time(int64_t t, char *out, size_t size) {
  if (t < TRADE_CLOCK_VALID_AFTER) {
    strlcpy(out, "no-clock", size);
    return;
  }
  time_t seconds = (time_t)t;
  struct tm utc;
  gmtime_r(&seconds, &utc);
  strftime(out, size, "%Y-%m-%dT%H:%M:%SZ", &utc);
}

static void slot_key(uint32_t index, char *key, size_t size) {
  snprintf(key, size, "q%lu", (unsigned long)(index % TradeQueue::CAPACITY));
}

static void print_status(Stream &port) {
  portENTER_CRITICAL(&lock);
  size_t pending = queue.size();
  portEXIT_CRITICAL(&lock);
  port.printf("APPROVAL %s audit=%lu check=%s next_seq=%lu queued=%u\n", on ? "ON" : "OFF",
              (unsigned long)audit_counter, trade_check_text(check), (unsigned long)engine.next_seq(),
              (unsigned)pending);
}

void approval_begin() {
  prefs.begin("approval", true);
  on = prefs.getBool("on", false);
  audit_counter = prefs.getUInt("audit", 0);
  engine = TradeEngine(prefs.getUInt("seq", 1));
  uint32_t head = prefs.getUInt("qhead", 0);
  size_t count = prefs.getUInt("qcount", 0);
  static TradeWeight slots[TradeQueue::CAPACITY];
  for (size_t i = 0; i < count && i < TradeQueue::CAPACITY; i++) {
    char key[8];
    slot_key(head + i, key, sizeof key);
    StoredTrade s;
    if (prefs.getBytes(key, &s, sizeof s) != sizeof s) continue;
    TradeWeight &w = slots[(head + i) % TradeQueue::CAPACITY];
    w.valid = true;
    w.seq = s.seq;
    w.value = s.value;
    w.decimals = s.decimals;
    w.unit = s.unit;
    w.at = s.at;
  }
  prefs.end();
  queue.restore(head, count, slots);
  check = on ? TRADE_MOTION : TRADE_OFF;
  print_status(Serial0);
}

// ---------- technician switch (USB only) ----------

static void write_change(bool to, const String &technician, const String &reason, Stream &port) {
  char when[32];
  format_time(time(nullptr), when, sizeof when);

  prefs.begin("approval", false);
  uint32_t counter = prefs.getUInt("audit", 0) + 1;
  uint32_t slot = prefs.getUInt("logn", 0);
  char entry[160];
  snprintf(entry, sizeof entry, "%lu %s %s->%s by=%s why=%s", (unsigned long)counter, when, on ? "ON" : "OFF",
           to ? "ON" : "OFF", technician.c_str(), reason.c_str());
  char key[8];
  snprintf(key, sizeof key, "log%lu", (unsigned long)(slot % LOG_SLOTS));
  prefs.putString(key, entry);
  prefs.putUInt("logn", slot + 1);
  prefs.putUInt("audit", counter);
  prefs.putBool("on", to);
  prefs.end();

  // Queued trade weights stay queued: they were issued while approval was on and still go to the Hub.
  engine.reset();
  portENTER_CRITICAL(&lock);
  on = to;
  audit_counter = counter;
  check = to ? TRADE_MOTION : TRADE_OFF;
  last_trade = TradeWeight();
  trade_current = false;
  portEXIT_CRITICAL(&lock);
  port.printf("APPROVAL_OK %s\n", entry);
}

static void print_log(Stream &port) {
  prefs.begin("approval", true);
  uint32_t count = prefs.getUInt("logn", 0);
  uint32_t first = count > LOG_SLOTS ? count - LOG_SLOTS : 0;
  for (uint32_t i = first; i < count; i++) {
    char key[8];
    snprintf(key, sizeof key, "log%lu", (unsigned long)(i % LOG_SLOTS));
    port.printf("APPROVAL_LOG %s\n", prefs.getString(key, "").c_str());
  }
  prefs.end();
  if (count == 0) port.println("APPROVAL_LOG empty");
}

void approval_handle_command(const String &command, Stream &port) {
  if (command == "APPROVAL STATUS") {
    print_status(port);
    return;
  }
  if (command == "APPROVAL LOG") {
    print_log(port);
    return;
  }
  bool to_on = command.startsWith("APPROVAL ON ");
  bool to_off = command.startsWith("APPROVAL OFF ");
  if (!to_on && !to_off) {
    port.println("APPROVAL_ERR use APPROVAL STATUS, APPROVAL LOG, APPROVAL ON|OFF <technician> <reason>");
    return;
  }
  String rest = command.substring(to_on ? 12 : 13);
  rest.trim();
  int space = rest.indexOf(' ');
  String technician = space < 0 ? rest : rest.substring(0, space);
  String reason = space < 0 ? "" : rest.substring(space + 1);
  reason.trim();
  if (technician.isEmpty() || reason.isEmpty()) {
    port.println("APPROVAL_ERR give the technician's name and a reason");
    return;
  }
  if (reason.length() > 80) reason = reason.substring(0, 80);
  if (to_on == on) {
    port.printf("APPROVAL_ERR already %s, nothing changed\n", on ? "ON" : "OFF");
    return;
  }
  write_change(to_on, technician, reason, port);
}

// ---------- each reading ----------

// Removes what the Hub accepted from flash, then from memory.
static void apply_delivered() {
  portENTER_CRITICAL(&lock);
  uint32_t acked = delivered_seq;
  portEXIT_CRITICAL(&lock);
  if (acked == applied_delivered_seq) return;
  applied_delivered_seq = acked;

  portENTER_CRITICAL(&lock);
  queue.ack_through(acked);
  uint32_t head = queue.head();
  size_t count = queue.size();
  portEXIT_CRITICAL(&lock);
  prefs.begin("approval", false);
  prefs.putUInt("qhead", head);
  prefs.putUInt("qcount", count);
  prefs.end();
}

// The entry goes to flash before the counters that make it visible, so a power cut never leaves
// a counted slot holding an old weight.
static void store_issued(const TradeWeight &w) {
  portENTER_CRITICAL(&lock);
  queue.push(w);
  uint32_t head = queue.head();
  size_t count = queue.size();
  portEXIT_CRITICAL(&lock);

  StoredTrade s = {w.seq, w.value, w.decimals, w.unit, w.at};
  char key[8];
  slot_key(head + count - 1, key, sizeof key);
  prefs.begin("approval", false);
  prefs.putBytes(key, &s, sizeof s);
  prefs.putUInt("qcount", count);
  prefs.putUInt("qhead", head);
  prefs.putUInt("seq", engine.next_seq());
  prefs.end();
}

TradeCheck approval_update(bool linked, bool fresh, bool alarm, int32_t value, int decimals, int unit) {
  apply_delivered();

  portENTER_CRITICAL(&lock);
  bool queue_full = queue.full();
  portEXIT_CRITICAL(&lock);

  TradeReading reading = {on, linked, fresh, alarm, value, decimals, unit, millis(), (int64_t)time(nullptr)};
  TradeWeight issued;
  TradeCheck next = engine.update(reading, queue_full, &issued);
  if (issued.valid) store_issued(issued);

  portENTER_CRITICAL(&lock);
  check = next;
  stable = engine.stable();
  trade_current = engine.current();
  last_trade = engine.last();
  portEXIT_CRITICAL(&lock);

  if (issued.valid) {
    char weight[16], when[32];
    format_weight(issued.value, issued.decimals, weight, sizeof weight);
    format_time(issued.at, when, sizeof when);
    Serial0.printf("TRADE seq=%lu weight=%s %s at=%s audit=%lu\n", (unsigned long)issued.seq, weight,
                   weight_unit_text(issued.unit), when, (unsigned long)audit_counter);
  }
  return next;
}

// ---------- read side ----------

bool approval_on() {
  portENTER_CRITICAL(&lock);
  bool value = on;
  portEXIT_CRITICAL(&lock);
  return value;
}

bool approval_stable() {
  portENTER_CRITICAL(&lock);
  bool value = stable;
  portEXIT_CRITICAL(&lock);
  return value;
}

uint32_t approval_audit_counter() {
  portENTER_CRITICAL(&lock);
  uint32_t value = audit_counter;
  portEXIT_CRITICAL(&lock);
  return value;
}

TradeCheck approval_check() {
  portENTER_CRITICAL(&lock);
  TradeCheck value = check;
  portEXIT_CRITICAL(&lock);
  return value;
}

TradeWeight approval_last_trade_weight() {
  portENTER_CRITICAL(&lock);
  TradeWeight value = last_trade;
  portEXIT_CRITICAL(&lock);
  return value;
}

size_t approval_pending() {
  portENTER_CRITICAL(&lock);
  size_t value = queue.size();
  portEXIT_CRITICAL(&lock);
  return value;
}

void approval_delivered(uint32_t seq) {
  portENTER_CRITICAL(&lock);
  if (seq > delivered_seq) delivered_seq = seq;
  portEXIT_CRITICAL(&lock);
}

static void append_trade(String &out, const TradeWeight &trade) {
  char weight[16], when[32];
  format_weight(trade.value, trade.decimals, weight, sizeof weight);
  format_time(trade.at, when, sizeof when);
  out += "{\"seq\":";
  out += trade.seq;
  out += ",\"value\":\"";
  out += weight;
  out += "\",\"unit\":\"";
  out += weight_unit_text(trade.unit);
  out += "\",\"at\":\"";
  out += when;
  out += "\"";
}

String approval_health_fields(uint32_t *included_through) {
  TradeWeight waiting[SEND_PER_HEARTBEAT];
  portENTER_CRITICAL(&lock);
  bool is_on = on;
  bool is_stable = stable;
  uint32_t counter = audit_counter;
  TradeCheck now_check = check;
  TradeWeight trade = last_trade;
  bool current = trade_current;
  size_t pending = queue.size();
  size_t sending = pending < SEND_PER_HEARTBEAT ? pending : SEND_PER_HEARTBEAT;
  for (size_t i = 0; i < sending; i++) waiting[i] = queue.at(i);
  portEXIT_CRITICAL(&lock);

  String out = ",\"approval\":\"";
  out += is_on ? "on" : "off";
  out += "\",\"trade_status\":\"";
  out += now_check == TRADE_OK ? "trade_checks_on" : "not_for_trade";
  out += "\",\"trade_check\":\"";
  out += trade_check_text(now_check);
  out += "\",\"audit_counter\":";
  out += counter;
  out += ",\"djb_stable\":";
  out += is_stable ? "true" : "false";
  if (now_check != TRADE_OK) out += ",\"notice\":\"Not for trade\"";
  if (is_on && trade.valid) {
    out += ",\"trade_weight\":";
    append_trade(out, trade);
    out += ",\"current\":";
    out += current ? "true" : "false";
    out += "}";
  }
  // Every queued trade weight, oldest first, until the Hub accepts it. The Hub may see one twice
  // if its 202 is lost; seq is the key to de-duplicate on.
  out += ",\"trade_queue\":";
  out += (unsigned)pending;
  if (sending > 0) {
    out += ",\"trade_weights\":[";
    for (size_t i = 0; i < sending; i++) {
      if (i) out += ",";
      append_trade(out, waiting[i]);
      out += "}";
    }
    out += "]";
  }
  if (included_through) *included_through = sending > 0 ? waiting[sending - 1].seq : 0;
  return out;
}
