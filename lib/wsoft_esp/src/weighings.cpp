#include "weighings.h"

#include <Preferences.h>
#include <time.h>

static Preferences prefs;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

// Shared between tasks: read and written under the lock.
static TradeQueue queue;
static bool for_trade_by_seq[TradeQueue::CAPACITY];  // seq % CAPACITY; seqs in the queue are consecutive
static uint32_t delivered_seq = 0;
static bool auto_save = false;
static uint32_t save_requests = 0;   // bumped by a tap
static uint32_t save_answered = 0;   // last request the screen task answered
static WsoftSaveResult last_result = WSOFT_SAVE_NONE;
static uint32_t last_result_ms = 0;
static TradeWeight last_saved;

// Screen task only.
static WsoftWeighingSaver saver;
static uint32_t applied_delivered_seq = 0;

struct StoredWeighing {
  uint32_t seq;
  int32_t value;
  int32_t decimals;
  int32_t unit;
  int64_t at;
  int32_t for_trade;
};

static void slot_key(uint32_t index, char *key, size_t size) {
  snprintf(key, size, "w%lu", (unsigned long)(index % TradeQueue::CAPACITY));
}

static bool source_rs232 = false;

const char *weighings_source() {
  return source_rs232 ? "rs232" : "rs485";
}

void weighings_begin(bool rs232) {
  source_rs232 = rs232;
  prefs.begin("weigh", true);
  uint32_t next_seq = prefs.getUInt("seq", 1);
  uint32_t head = prefs.getUInt("qhead", 0);
  size_t count = prefs.getUInt("qcount", 0);
  static TradeWeight slots[TradeQueue::CAPACITY];
  for (size_t i = 0; i < count && i < TradeQueue::CAPACITY; i++) {
    char key[8];
    slot_key(head + i, key, sizeof key);
    StoredWeighing s;
    if (prefs.getBytes(key, &s, sizeof s) != sizeof s) continue;
    TradeWeight &w = slots[(head + i) % TradeQueue::CAPACITY];
    w.valid = true;
    w.seq = s.seq;
    w.value = s.value;
    w.decimals = s.decimals;
    w.unit = s.unit;
    w.at = s.at;
    for_trade_by_seq[s.seq % TradeQueue::CAPACITY] = s.for_trade != 0;
  }
  prefs.end();
  saver = WsoftWeighingSaver(next_seq);
  queue.restore(head, count, slots);
  Serial0.printf("WEIGH next_seq=%lu queued=%u\n", (unsigned long)next_seq, (unsigned)queue.size());
}

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
  prefs.begin("weigh", false);
  prefs.putUInt("qhead", head);
  prefs.putUInt("qcount", count);
  prefs.end();
}

// The entry goes to flash before the counters that make it visible.
static void store_saved(const TradeWeight &w, bool for_trade) {
  portENTER_CRITICAL(&lock);
  queue.push(w);
  for_trade_by_seq[w.seq % TradeQueue::CAPACITY] = for_trade;
  uint32_t head = queue.head();
  size_t count = queue.size();
  portEXIT_CRITICAL(&lock);

  StoredWeighing s = {w.seq, w.value, w.decimals, w.unit, w.at, for_trade ? 1 : 0};
  char key[8];
  slot_key(head + count - 1, key, sizeof key);
  prefs.begin("weigh", false);
  prefs.putBytes(key, &s, sizeof s);
  prefs.putUInt("qcount", count);
  prefs.putUInt("qhead", head);
  prefs.putUInt("seq", saver.next_seq());
  prefs.end();
}

WsoftSaveResult weighings_update(bool linked, bool fresh, bool alarm, int32_t value, int decimals, int unit,
                                 bool for_trade) {
  apply_delivered();

  portENTER_CRITICAL(&lock);
  bool full = queue.full();
  bool automatic = auto_save;
  uint32_t requests = save_requests;
  bool tapped = requests != save_answered;
  portEXIT_CRITICAL(&lock);

  TradeReading r = {true, linked, fresh, alarm, value, decimals, unit, millis(), (int64_t)time(nullptr)};
  TradeWeight saved;
  WsoftSaveResult result = saver.update(r, automatic, tapped, full, &saved);
  if (result == WSOFT_SAVE_SAVED) store_saved(saved, for_trade);

  portENTER_CRITICAL(&lock);
  if (tapped) save_answered = requests;
  if (result != WSOFT_SAVE_NONE) {
    last_result = result;
    last_result_ms = millis();
  }
  if (result == WSOFT_SAVE_SAVED) last_saved = saved;
  portEXIT_CRITICAL(&lock);

  if (result == WSOFT_SAVE_SAVED) {
    char weight[16];
    format_weight(saved.value, saved.decimals, weight, sizeof weight);
    Serial0.printf("WEIGH saved seq=%lu weight=%s %s %s for_trade=%d\n", (unsigned long)saved.seq, weight,
                   weight_unit_text(saved.unit), tapped ? "tap" : "auto", for_trade ? 1 : 0);
  } else if (tapped) {
    Serial0.printf("WEIGH save refused: %s\n", wsoft_save_result_text(result));
  }
  return result;
}

void weighings_request_save() {
  portENTER_CRITICAL(&lock);
  save_requests++;
  portEXIT_CRITICAL(&lock);
}

WsoftSaveResult weighings_save(uint32_t timeout_ms, TradeWeight *saved) {
  portENTER_CRITICAL(&lock);
  uint32_t mine = ++save_requests;
  portEXIT_CRITICAL(&lock);
  uint32_t start = millis();
  for (;;) {
    portENTER_CRITICAL(&lock);
    bool answered = (int32_t)(save_answered - mine) >= 0;
    WsoftSaveResult result = last_result;
    TradeWeight w = last_saved;
    portEXIT_CRITICAL(&lock);
    if (answered) {
      if (saved) *saved = w;
      return result;
    }
    if (millis() - start >= timeout_ms) return WSOFT_SAVE_NO_SIGNAL;  // the screen task did not answer
    delay(20);
  }
}

void weighings_set_auto_save(bool on) {
  portENTER_CRITICAL(&lock);
  auto_save = on;
  portEXIT_CRITICAL(&lock);
}

void weighings_info(WeighingsInfo *out) {
  portENTER_CRITICAL(&lock);
  out->auto_save = auto_save;
  out->pending = queue.size();
  out->last = last_saved;
  out->last_result = last_result;
  out->last_result_ms = last_result_ms;
  portEXIT_CRITICAL(&lock);
}

bool weighings_oldest(TradeWeight *w, bool *for_trade) {
  portENTER_CRITICAL(&lock);
  // Skip what was acked but not yet removed by the screen task.
  bool found = false;
  for (size_t i = 0; i < queue.size(); i++) {
    const TradeWeight &item = queue.at(i);
    if (item.seq > delivered_seq) {
      *w = item;
      *for_trade = for_trade_by_seq[item.seq % TradeQueue::CAPACITY];
      found = true;
      break;
    }
  }
  portEXIT_CRITICAL(&lock);
  return found;
}

void weighings_delivered(uint32_t seq) {
  portENTER_CRITICAL(&lock);
  if (seq > delivered_seq) delivered_seq = seq;
  portEXIT_CRITICAL(&lock);
}
