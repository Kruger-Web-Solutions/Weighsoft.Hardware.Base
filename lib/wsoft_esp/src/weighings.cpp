#include "wsoft_log.h"
#include "weighings.h"

#include <LittleFS.h>
#include <Preferences.h>
#include <time.h>

static Preferences prefs;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

// Shared between tasks: read and written under the lock.
static TradeQueue queue;
static bool for_trade_by_seq[TradeQueue::CAPACITY];  // seq % CAPACITY; seqs in the queue are consecutive
static int32_t tare_by_seq[TradeQueue::CAPACITY];   // same index; 0 = no tare
static WsoftHiloStamp hilo_by_seq[TradeQueue::CAPACITY];  // same index; on = false when no product
static uint32_t delivered_seq = 0;
static bool auto_save = false;
static uint32_t save_requests = 0;   // bumped by a tap
static uint32_t save_answered = 0;   // last request the screen task answered
static WsoftSaveResult last_result = WSOFT_SAVE_NONE;
static uint32_t last_result_ms = 0;
static TradeWeight last_saved;
static WsoftSaveResult waiting = WSOFT_SAVE_NONE;  // under lock
static uint32_t result_count = 0;                   // under lock: Save results since boot
static void (*remote_save)() = nullptr;             // Writer role: Save taps go to the weigh board
static WsoftSaveResult remote_waiting = WSOFT_SAVE_NO_SIGNAL;  // under lock

// Screen task only.
static WsoftWeighingSaver saver;
static uint32_t applied_delivered_seq = 0;

// Saved before the tare was added. Still read, so queued weighings survive the update.
struct StoredWeighingV1 {
  uint32_t seq;
  int32_t value;
  int32_t decimals;
  int32_t unit;
  int64_t at;
  int32_t for_trade;
};

// Saved before Hi-Lo was added. Still read, like V1.
struct StoredWeighingV2 {
  uint32_t seq;
  int32_t value;
  int32_t decimals;
  int32_t unit;
  int64_t at;
  int32_t for_trade;
  int32_t tare;    // in the weighing's decimals and unit, 0 = none
  int32_t spare;   // keeps this size apart from StoredWeighingV1's
  int32_t spare2;
};

// Now: V2 and the Hi-Lo result (Hub ADR 0008).
struct StoredWeighing {
  uint32_t seq;
  int32_t value;
  int32_t decimals;
  int32_t unit;
  int64_t at;
  int32_t for_trade;
  int32_t tare;
  int32_t hilo_on;     // 1 = a product was picked
  int32_t hilo_state;  // WsoftHiloState
  int32_t hilo_auto;   // 1 = auto save
  int32_t target_g;
  int32_t low_g;
  int32_t high_g;
  char code[WSOFT_HILO_CODE_MAX + 1];
  char overridden;  // 1 = outside the limits, saved by the operator's second tap (was spare: 0)
  char spare[6];
};
static_assert(sizeof(StoredWeighing) != sizeof(StoredWeighingV1) && sizeof(StoredWeighing) != sizeof(StoredWeighingV2) &&
                  sizeof(StoredWeighingV1) != sizeof(StoredWeighingV2),
              "the layouts must differ in size");

static WsoftHiloStamp stamp_of(const StoredWeighing &s) {
  WsoftHiloStamp h;
  h.on = s.hilo_on == 1 && s.hilo_state >= WSOFT_HILO_LOW && s.hilo_state <= WSOFT_HILO_HIGH;
  if (!h.on) return h;
  memcpy(h.code, s.code, sizeof h.code);
  h.code[WSOFT_HILO_CODE_MAX] = 0;
  h.target_g = s.target_g;
  h.low_g = s.low_g;
  h.high_g = s.high_g;
  h.state = (WsoftHiloState)s.hilo_state;
  h.auto_saved = s.hilo_auto == 1;
  h.overridden = s.overridden == 1;
  return h;
}

static void slot_key(uint32_t index, char *key, size_t size) {
  snprintf(key, size, "w%lu", (unsigned long)(index % TradeQueue::CAPACITY));
}

static bool source_rs232 = false;

const char *weighings_source() {
  return source_rs232 ? "rs232" : "rs485";
}

// Auto save used to live in the W-Soft settings file. Read it raw, once, at the first boot
// after the update (the settings no longer have the field). The framework mounts the file
// system before the screen task starts; if it is not there, Auto save starts off.
static bool old_settings_auto_save() {
  File file = LittleFS.open("/config/wsoftSettings.json", "r");
  if (!file) return false;
  String text = file.readString();
  file.close();
  text.replace(" ", "");
  return text.indexOf("\"auto_save\":true") >= 0;
}

void weighings_begin(bool rs232) {
  source_rs232 = rs232;
  prefs.begin("weigh", true);
  uint32_t next_seq = prefs.getUInt("seq", 1);
  // putBool stores one byte: 2 here means the key is not there yet.
  uint8_t stored_auto = prefs.getUChar("auto", 2);
  bool automatic = stored_auto == 1;
  uint32_t head = prefs.getUInt("qhead", 0);
  size_t count = prefs.getUInt("qcount", 0);
  static TradeWeight slots[TradeQueue::CAPACITY];
  for (size_t i = 0; i < count && i < TradeQueue::CAPACITY; i++) {
    char key[8];
    slot_key(head + i, key, sizeof key);
    StoredWeighing s = {};
    size_t length = prefs.getBytesLength(key);
    if (length == sizeof(StoredWeighingV1)) {
      StoredWeighingV1 old;
      if (prefs.getBytes(key, &old, sizeof old) != sizeof old) continue;
      s.seq = old.seq;
      s.value = old.value;
      s.decimals = old.decimals;
      s.unit = old.unit;
      s.at = old.at;
      s.for_trade = old.for_trade;
    } else if (length == sizeof(StoredWeighingV2)) {
      StoredWeighingV2 old;
      if (prefs.getBytes(key, &old, sizeof old) != sizeof old) continue;
      s.seq = old.seq;
      s.value = old.value;
      s.decimals = old.decimals;
      s.unit = old.unit;
      s.at = old.at;
      s.for_trade = old.for_trade;
      s.tare = old.tare;
    } else if (length != sizeof s || prefs.getBytes(key, &s, sizeof s) != sizeof s) {
      continue;
    }
    TradeWeight &w = slots[(head + i) % TradeQueue::CAPACITY];
    w.valid = true;
    w.seq = s.seq;
    w.value = s.value;
    w.decimals = s.decimals;
    w.unit = s.unit;
    w.at = s.at;
    for_trade_by_seq[s.seq % TradeQueue::CAPACITY] = s.for_trade != 0;
    tare_by_seq[s.seq % TradeQueue::CAPACITY] = s.tare;
    hilo_by_seq[s.seq % TradeQueue::CAPACITY] = stamp_of(s);
  }
  prefs.end();
  if (stored_auto == 2) {
    automatic = old_settings_auto_save();
    prefs.begin("weigh", false);
    prefs.putBool("auto", automatic);  // from now on NVS holds it
    prefs.end();
    WSOFT_LOG.printf("WEIGH auto_save carried over from the old settings: %s\n", automatic ? "on" : "off");
  }
  saver = WsoftWeighingSaver(next_seq);
  portENTER_CRITICAL(&lock);
  queue.restore(head, count, slots);
  auto_save = automatic;
  portEXIT_CRITICAL(&lock);
  WSOFT_LOG.printf("WEIGH next_seq=%lu queued=%u auto_save=%s\n", (unsigned long)next_seq, (unsigned)queue.size(),
                 automatic ? "on" : "off");
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
static void store_saved(const TradeWeight &w, bool for_trade, int32_t tare_value, const WsoftHiloStamp &hilo) {
  portENTER_CRITICAL(&lock);
  queue.push(w);
  for_trade_by_seq[w.seq % TradeQueue::CAPACITY] = for_trade;
  tare_by_seq[w.seq % TradeQueue::CAPACITY] = tare_value;
  hilo_by_seq[w.seq % TradeQueue::CAPACITY] = hilo;
  uint32_t head = queue.head();
  size_t count = queue.size();
  portEXIT_CRITICAL(&lock);

  StoredWeighing s;
  memset(&s, 0, sizeof s);
  s.seq = w.seq;
  s.value = w.value;
  s.decimals = w.decimals;
  s.unit = w.unit;
  s.at = w.at;
  s.for_trade = for_trade ? 1 : 0;
  s.tare = tare_value;
  if (hilo.on) {
    s.hilo_on = 1;
    s.hilo_state = hilo.state;
    s.hilo_auto = hilo.auto_saved ? 1 : 0;
    s.target_g = hilo.target_g;
    s.low_g = hilo.low_g;
    s.high_g = hilo.high_g;
    strlcpy(s.code, hilo.code, sizeof s.code);
    s.overridden = hilo.overridden ? 1 : 0;
  }
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
                                 bool for_trade, int32_t tare_value, WsoftSaveResult hold,
                                 const WsoftHiloProduct *product) {
  apply_delivered();

  portENTER_CRITICAL(&lock);
  bool full = queue.full();
  bool automatic = auto_save;
  uint32_t requests = save_requests;
  bool tapped = requests != save_answered;
  portEXIT_CRITICAL(&lock);

  TradeReading r = {true, linked, fresh, alarm, value, decimals, unit, millis(), (int64_t)time(nullptr)};
  // Hi-Lo is judged on the net weight (gross - tare), in the reading's own decimals and unit.
  // Outside the limits auto save waits and a tap needs a second tap (WSOFT_SAVE_OUT_OF_LIMITS).
  WsoftHiloState state = WSOFT_HILO_NONE;
  if (product != nullptr && linked) state = wsoft_hilo_check(*product, value - tare_value, decimals, unit);
  TradeWeight saved;
  WsoftSaveResult result = saver.update(r, automatic, tapped, full, &saved, hold, state);
  WsoftHiloStamp hilo;
  if (result == WSOFT_SAVE_SAVED && product != nullptr) {
    hilo.state = state;
    hilo.on = hilo.state != WSOFT_HILO_NONE;
    hilo.overridden = saver.last_overridden();
    strlcpy(hilo.code, product->code, sizeof hilo.code);
    hilo.target_g = product->target_g;
    hilo.low_g = product->low_g;
    hilo.high_g = product->high_g;
    hilo.auto_saved = !tapped;
  }
  if (result == WSOFT_SAVE_SAVED) store_saved(saved, for_trade, tare_value, hilo);

  portENTER_CRITICAL(&lock);
  waiting = remote_save != nullptr ? remote_waiting : saver.waiting();
  if (tapped) save_answered = requests;
  if (result != WSOFT_SAVE_NONE) {
    result_count++;
    last_result = result;
    last_result_ms = millis();
  }
  if (result == WSOFT_SAVE_SAVED) last_saved = saved;
  portEXIT_CRITICAL(&lock);

  if (result == WSOFT_SAVE_SAVED) {
    char weight[16];
    format_weight(saved.value, saved.decimals, weight, sizeof weight);
    char tare_text[16];
    format_weight(tare_value, saved.decimals, tare_text, sizeof tare_text);
    WSOFT_LOG.printf("WEIGH saved seq=%lu weight=%s %s tare=%s %s for_trade=%d hilo=%s %s%s\n", (unsigned long)saved.seq,
                   weight, weight_unit_text(saved.unit), tare_text, tapped ? "tap" : "auto", for_trade ? 1 : 0,
                   hilo.on ? hilo.code : "-", hilo.on ? wsoft_hilo_state_wire(hilo.state) : "",
                   hilo.overridden ? " OVERRIDE" : "");
  } else if (tapped) {
    WSOFT_LOG.printf("WEIGH save refused: %s\n", wsoft_save_result_text(result));
  }
  return result;
}

void weighings_request_save() {
  if (remote_save != nullptr) {
    remote_save();
    return;
  }
  portENTER_CRITICAL(&lock);
  save_requests++;
  portEXIT_CRITICAL(&lock);
}

void weighings_set_remote_save(void (*fn)()) { remote_save = fn; }

void weighings_remote_status(WsoftSaveResult status) {
  portENTER_CRITICAL(&lock);
  remote_waiting = status;
  portEXIT_CRITICAL(&lock);
}

void weighings_remote_result(WsoftSaveResult result) {
  if (result == WSOFT_SAVE_NONE) return;
  portENTER_CRITICAL(&lock);
  last_result = result;
  last_result_ms = millis();
  portEXIT_CRITICAL(&lock);
  WSOFT_LOG.printf("WEIGH weigh board answered the Save: %s\n", wsoft_save_result_text(result));
}

uint32_t weighings_result_count() {
  portENTER_CRITICAL(&lock);
  uint32_t n = result_count;
  portEXIT_CRITICAL(&lock);
  return n;
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
  prefs.begin("weigh", false);
  prefs.putBool("auto", on);
  prefs.end();
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
  out->waiting = waiting;
  portEXIT_CRITICAL(&lock);
}

bool weighings_oldest(TradeWeight *w, bool *for_trade, int32_t *tare_value, WsoftHiloStamp *hilo) {
  portENTER_CRITICAL(&lock);
  // Skip what was acked but not yet removed by the screen task.
  bool found = false;
  for (size_t i = 0; i < queue.size(); i++) {
    const TradeWeight &item = queue.at(i);
    if (item.seq > delivered_seq) {
      *w = item;
      *for_trade = for_trade_by_seq[item.seq % TradeQueue::CAPACITY];
      *tare_value = tare_by_seq[item.seq % TradeQueue::CAPACITY];
      if (hilo) *hilo = hilo_by_seq[item.seq % TradeQueue::CAPACITY];
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
