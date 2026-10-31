#include "save_queue.h"

#include <Preferences.h>
#include <esp_random.h>
#include <time.h>

#include "approval.h"

static const char *NVS_NAMESPACE = "wsave";

static Preferences prefs;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

// Shared with the web and Hub tasks, so read and written under the lock.
static bool auto_on = false;
static bool ignore_alarm = false;  // bench test mode, never with approval ON
static uint32_t auto_stable_s = 3;
static SaveCheck check = SAVE_NO_SIGNAL;
static SaveQueue queue;
static SavedWeighing last_saved;
static uint32_t last_saved_ms = 0;
static SavedWeighing recent[SAVE_RECENT_MAX];
static size_t recent_count = 0;
static size_t recent_next = 0;
static uint32_t delivered_seq = 0;  // set by the Hub task, applied by the screen task
static int hub_status = 0;
static uint32_t request_id = 0;   // SAVE presses (screen and web)
static uint32_t answered_id = 0;  // the last press the screen task has answered
static SaveCheck answer_check = SAVE_NO_SIGNAL;
static uint32_t answer_seq = 0;

// Screen task only.
static SaveEngine engine;
static uint32_t next_seq = 1;
static uint32_t applied_delivered_seq = 0;

// Written once in save_begin(), before the Hub task starts.
static char id_prefix[40] = "wsoft";

// Fixed layout for a queued weighing in flash.
struct StoredSave {
  uint32_t seq;
  int32_t value;
  int32_t decimals;
  int32_t unit;
  int64_t at;
  uint32_t flags;  // bit 0 trade, bit 1 auto-saved
};

static const uint32_t FLAG_TRADE = 1;
static const uint32_t FLAG_AUTO = 2;
static const uint32_t FLAG_DJB_ALARM = 4;

static void slot_key(uint32_t index, char *key, size_t size) {
  snprintf(key, size, "q%lu", (unsigned long)(index % SaveQueue::CAPACITY));
}

// Caller holds the lock.
static void remember_recent(const SavedWeighing &w) {
  recent[recent_next] = w;
  recent_next = (recent_next + 1) % SAVE_RECENT_MAX;
  if (recent_count < SAVE_RECENT_MAX) recent_count++;
}

void save_begin() {
  prefs.begin(NVS_NAMESPACE, false);
  uint32_t series = prefs.getUInt("series", 0);
  if (series == 0) {
    series = esp_random();
    if (series == 0) series = 1;
    prefs.putUInt("series", series);
  }
  next_seq = prefs.getUInt("seq", 1);
  uint32_t head = prefs.getUInt("qhead", 0);
  size_t count = prefs.getUInt("qcount", 0);
  if (count > SaveQueue::CAPACITY) count = SaveQueue::CAPACITY;
  static SavedWeighing slots[SaveQueue::CAPACITY];
  for (size_t i = 0; i < count; i++) {
    char key[8];
    slot_key(head + i, key, sizeof key);
    StoredSave s;
    if (prefs.getBytes(key, &s, sizeof s) != sizeof s) continue;
    SavedWeighing &w = slots[(head + i) % SaveQueue::CAPACITY];
    w.valid = true;
    w.seq = s.seq;
    w.value = s.value;
    w.decimals = s.decimals;
    w.unit = s.unit;
    w.at = s.at;
    w.trade = (s.flags & FLAG_TRADE) != 0;
    w.auto_saved = (s.flags & FLAG_AUTO) != 0;
    w.djb_alarm = (s.flags & FLAG_DJB_ALARM) != 0;
    if (w.seq >= next_seq) next_seq = w.seq + 1;
  }
  prefs.end();

  uint64_t mac = ESP.getEfuseMac();
  snprintf(id_prefix, sizeof id_prefix, "wsoft-%04lx%08lx-%08lx", (unsigned long)((mac >> 32) & 0xFFFF),
           (unsigned long)(mac & 0xFFFFFFFF), (unsigned long)series);

  portENTER_CRITICAL(&lock);
  queue.restore(head, count, slots);
  // The web list starts with what still waits for the Hub.
  for (size_t i = 0; i < queue.size(); i++) remember_recent(queue.at(i));
  portEXIT_CRITICAL(&lock);
  Serial0.printf("SAVE ready id=%s next_seq=%lu queued=%u\n", id_prefix, (unsigned long)next_seq, (unsigned)count);
}

void save_set_auto(bool on, uint32_t stable_s, bool ignore_djb_alarm) {
  portENTER_CRITICAL(&lock);
  auto_on = on;
  ignore_alarm = ignore_djb_alarm;
  auto_stable_s = stable_s;
  portEXIT_CRITICAL(&lock);
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
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putUInt("qhead", head);
  prefs.putUInt("qcount", count);
  prefs.end();
}

// The entry goes to flash before the counters that make it visible, so a power cut never leaves
// a counted slot holding an old weighing.
static void store(const SavedWeighing &w) {
  portENTER_CRITICAL(&lock);
  queue.push(w);
  uint32_t head = queue.head();
  size_t count = queue.size();
  portEXIT_CRITICAL(&lock);

  StoredSave s = {w.seq, w.value, w.decimals, w.unit, w.at,
                  (w.trade ? FLAG_TRADE : 0) | (w.auto_saved ? FLAG_AUTO : 0) | (w.djb_alarm ? FLAG_DJB_ALARM : 0)};
  char key[8];
  slot_key(head + count - 1, key, sizeof key);
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putBytes(key, &s, sizeof s);
  prefs.putUInt("qcount", count);
  prefs.putUInt("qhead", head);
  prefs.putUInt("seq", next_seq);
  prefs.end();
}

SaveCheck save_update(bool linked, bool fresh, bool alarm, int32_t value, int decimals, int unit) {
  apply_delivered();

  portENTER_CRITICAL(&lock);
  uint32_t wanted = request_id;
  bool manual = wanted != answered_id;
  bool on = auto_on;
  uint32_t stable_ms = auto_stable_s * 1000;
  bool full = queue.full();
  bool skip_alarm = ignore_alarm;
  portEXIT_CRITICAL(&lock);

  // Bench test mode lets a save through a DJB alarm, but never while approval is ON.
  bool alarm_ignored = alarm && skip_alarm && !approval_on();
  bool effective_alarm = alarm && !alarm_ignored;
  SaveInput in = {linked, fresh, effective_alarm, value, decimals, unit, millis(), (int64_t)time(nullptr),
                  on, stable_ms, manual, full};
  SaveDecision d = engine.update(in);

  SavedWeighing w;
  if (d.save) {
    w.valid = true;
    w.seq = next_seq++;
    w.value = value;
    w.decimals = decimals;
    w.unit = unit;
    w.at = in.now_unix;
    w.trade = approval_on();
    w.auto_saved = d.automatic;
    w.djb_alarm = alarm_ignored;
    store(w);
  }

  // Just saved: this load is done until the scale is back to empty.
  SaveCheck now = d.save ? SAVE_DONE : d.check;
  portENTER_CRITICAL(&lock);
  check = now;
  if (manual) {
    answered_id = wanted;
    answer_check = d.save ? SAVE_OK : d.check;
    answer_seq = d.save ? w.seq : 0;
  }
  if (d.save) {
    last_saved = w;
    last_saved_ms = millis();
    remember_recent(w);
  }
  portEXIT_CRITICAL(&lock);

  if (d.save) {
    char weight[16];
    format_weight(w.value, w.decimals, weight, sizeof weight);
    Serial0.printf("SAVE seq=%lu weight=%s %s %s trade=%d\n", (unsigned long)w.seq, weight,
                   weight_unit_text(w.unit), w.auto_saved ? "auto" : "button", w.trade ? 1 : 0);
  } else if (manual) {
    Serial0.printf("SAVE refused: %s\n", save_check_text(d.check));
  }
  return now;
}

void save_press() {
  portENTER_CRITICAL(&lock);
  request_id++;
  portEXIT_CRITICAL(&lock);
}

bool save_request(uint32_t timeout_ms, SaveCheck *out_check, uint32_t *out_seq) {
  portENTER_CRITICAL(&lock);
  uint32_t mine = ++request_id;
  portEXIT_CRITICAL(&lock);

  uint32_t start = millis();
  for (;;) {
    portENTER_CRITICAL(&lock);
    // Ids only grow; a later press answered in the same poll answers this one too.
    bool answered = (int32_t)(answered_id - mine) >= 0;
    SaveCheck c = answer_check;
    uint32_t seq = answer_seq;
    portEXIT_CRITICAL(&lock);
    if (answered) {
      if (out_check) *out_check = c;
      if (out_seq) *out_seq = seq;
      return true;
    }
    if (millis() - start > timeout_ms) return false;
    delay(5);
  }
}

void save_status(SaveStatus *out) {
  portENTER_CRITICAL(&lock);
  out->check = check;
  out->can_save = check == SAVE_OK;
  out->auto_on = auto_on;
  out->auto_stable_s = auto_stable_s;
  out->last = last_saved;
  out->last_saved_ms = last_saved_ms;
  out->queue = (uint32_t)queue.size();
  out->hub_status = hub_status;
  portEXIT_CRITICAL(&lock);
}

size_t save_recent(SavedWeighing *out, size_t max, uint32_t *queued_from) {
  portENTER_CRITICAL(&lock);
  size_t n = recent_count < max ? recent_count : max;
  for (size_t i = 0; i < n; i++) {
    out[i] = recent[(recent_next + SAVE_RECENT_MAX - 1 - i) % SAVE_RECENT_MAX];
  }
  if (queued_from) *queued_from = queue.size() > 0 ? queue.at(0).seq : 0;
  portEXIT_CRITICAL(&lock);
  return n;
}

// ---------- Hub task ----------

size_t save_pending() {
  size_t n = 0;
  portENTER_CRITICAL(&lock);
  for (size_t i = 0; i < queue.size(); i++) {
    if (queue.at(i).seq > delivered_seq) n++;
  }
  portEXIT_CRITICAL(&lock);
  return n;
}

bool save_oldest(SavedWeighing *out) {
  // Skips what the Hub already has but the screen task has not yet removed from flash (it does that
  // at its next poll, and never while the screen is paused).
  bool any = false;
  portENTER_CRITICAL(&lock);
  for (size_t i = 0; i < queue.size() && !any; i++) {
    if (queue.at(i).seq > delivered_seq) {
      *out = queue.at(i);
      any = true;
    }
  }
  portEXIT_CRITICAL(&lock);
  return any;
}

const char *save_id_prefix() {
  return id_prefix;
}

void save_delivered(uint32_t seq) {
  portENTER_CRITICAL(&lock);
  if (seq > delivered_seq) delivered_seq = seq;
  portEXIT_CRITICAL(&lock);
}

void save_hub_result(int http_status) {
  portENTER_CRITICAL(&lock);
  hub_status = http_status;
  portEXIT_CRITICAL(&lock);
}
