#include "packers.h"

#include "wsoft_log.h"

#include <Preferences.h>
#include <ctype.h>
#include <freertos/semphr.h>
#ifdef ESP32
#include <nvs.h>
#endif
#include <string.h>

static bool eq_ci(const char *a, const char *b) {
  if (a == nullptr || b == nullptr) return false;
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    a++;
    b++;
  }
  return *a == *b;
}

static const uint32_t PULL_MS = 60000;
static const uint32_t RETRY_MS = 15000;
static const uint32_t AFTER_CHANGE_MS = 2000;
static const uint32_t STORED_VERSION = 1;

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t flash_mutex = nullptr;
static SemaphoreHandle_t mgr_mutex = nullptr;

// Big packer buffers live on the heap, not in static DRAM: the yellow board (ESP32, no PSRAM)
// ran out of static DRAM by about 105 KB once packers landed (28 Nov 2026). Allocated once,
// never freed, so they behave exactly like the statics they replace.
static WsoftPackerRoster &roster = *new WsoftPackerRoster();  // require_packer_bags defaults to false
static WsoftPackerStrip strip;
static WsoftPackerPinGuard pin_guard;
static WsoftBagEventOutbox &outbox = *new WsoftBagEventOutbox();

static uint32_t next_pull_ms = 0;
static uint32_t next_events_ms = 0;
static uint32_t last_ok_ms = 0;

static void save_state(bool with_roster, bool with_outbox, bool with_pin_fails) {
  if (flash_mutex == nullptr) return;
  static WsoftPackerRoster &roster_copy = *new WsoftPackerRoster();
  static WsoftBagEventOutbox &outbox_copy = *new WsoftBagEventOutbox();
  uint32_t fails_copy = 0;

  xSemaphoreTake(flash_mutex, portMAX_DELAY);
  portENTER_CRITICAL(&lock);
  if (with_roster) roster_copy = roster;
  if (with_outbox) outbox_copy = outbox;
  if (with_pin_fails) fails_copy = pin_guard.fails();
  portEXIT_CRITICAL(&lock);

  Preferences store;
  if (store.begin("packers", false)) {
    store.putUInt("ver", STORED_VERSION);
    if (with_roster) store.putBytes("roster", &roster_copy, sizeof roster_copy);
    if (with_outbox) store.putBytes("outbox", &outbox_copy, sizeof outbox_copy);
    if (with_pin_fails) store.putUInt("fails", fails_copy);
    store.end();
  }
  xSemaphoreGive(flash_mutex);
}

static bool roster_equal(const WsoftPackerRoster &a, const WsoftPackerRoster &b) {
  if (a.require_packer_bags != b.require_packer_bags ||
      strcmp(a.pack_unit_label, b.pack_unit_label) != 0 ||
      a.count != b.count) {
    return false;
  }
  for (size_t i = 0; i < a.count; i++) {
    const WsoftPackerEntry &x = a.packers[i];
    const WsoftPackerEntry &y = b.packers[i];
    if (strcmp(x.packer_id, y.packer_id) != 0 ||
        strcmp(x.code, y.code) != 0 ||
        strcmp(x.name, y.name) != 0 ||
        x.floor_manager != y.floor_manager ||
        x.has_verifier != y.has_verifier ||
        memcmp(x.pin_salt, y.pin_salt, sizeof x.pin_salt) != 0 ||
        memcmp(x.pin_sha256, y.pin_sha256, sizeof x.pin_sha256) != 0 ||
        x.has_batch != y.has_batch ||
        strcmp(x.batch_id, y.batch_id) != 0 ||
        x.issued != y.issued ||
        x.remaining != y.remaining ||
        x.signed_in_here != y.signed_in_here) {
      return false;
    }
  }
  return true;
}

void packers_begin() {
  flash_mutex = xSemaphoreCreateMutex();
  mgr_mutex = xSemaphoreCreateMutex();
  static WsoftPackerRoster &stored_roster = *new WsoftPackerRoster();
  static WsoftBagEventOutbox &stored_outbox = *new WsoftBagEventOutbox();
  // In place: a temporary roster (about 10 KB) overflowed the loop task's stack at boot.
  wsoft_packer_roster_reset(&stored_roster);
  stored_outbox.clear();
  uint32_t stored_fails = 0;

  Preferences store;
  if (store.begin("packers", true)) {
    if (store.getUInt("ver", 0) == STORED_VERSION) {
      if (store.getBytesLength("roster") == sizeof stored_roster) {
        if (store.getBytes("roster", &stored_roster, sizeof stored_roster) != sizeof stored_roster ||
            stored_roster.count > WSOFT_PACKERS_MAX) {
          wsoft_packer_roster_reset(&stored_roster);
        }
      }
      if (store.getBytesLength("outbox") == sizeof stored_outbox) {
        if (store.getBytes("outbox", &stored_outbox, sizeof stored_outbox) != sizeof stored_outbox ||
            stored_outbox.size() > WSOFT_BAG_OUTBOX_MAX) {
          stored_outbox.clear();
        }
      }
      stored_fails = store.getUInt("fails", 0);
    }
    store.end();
  }

  portENTER_CRITICAL(&lock);
  roster = stored_roster;
  outbox = stored_outbox;
  pin_guard = WsoftPackerPinGuard(stored_fails, millis());
  strip.sync_roster(roster, millis());
  portEXIT_CRITICAL(&lock);

  WSOFT_LOG.printf("PACKERS required=%d count=%u unit=%s outbox=%u\n",
                   stored_roster.require_packer_bags ? 1 : 0, (unsigned)stored_roster.count,
                   stored_roster.pack_unit_label, (unsigned)stored_outbox.size());
#ifdef ESP32
  nvs_stats_t nvs_st;
  if (nvs_get_stats(nullptr, &nvs_st) == ESP_OK) {
    WSOFT_LOG.printf("NVS used=%u free=%u total=%u namespaces=%u\n",
                     (unsigned)nvs_st.used_entries, (unsigned)nvs_st.free_entries,
                     (unsigned)nvs_st.total_entries, (unsigned)nvs_st.namespace_count);
  }
#endif
}

bool packers_required() {
  portENTER_CRITICAL(&lock);
  bool req = roster.require_packer_bags;
  portEXIT_CRITICAL(&lock);
  return req;
}

WsoftSaveResult packers_save_hold() {
  portENTER_CRITICAL(&lock);
  WsoftSaveResult hold = wsoft_packer_save_hold(roster, strip);
  portEXIT_CRITICAL(&lock);
  return hold;
}

bool packers_active_meta(char *code, size_t code_size, char *name, size_t name_size, char *batch_id,
                         size_t batch_size) {
  portENTER_CRITICAL(&lock);
  bool ok = false;
  if (roster.require_packer_bags && strip.active()[0] != 0) {
    const WsoftPackerEntry *p = wsoft_packer_find(roster, strip.active());
    if (p != nullptr) {
      if (code && code_size > 0) strlcpy(code, p->code, code_size);
      if (name && name_size > 0) strlcpy(name, p->name, name_size);
      if (batch_id && batch_size > 0) strlcpy(batch_id, p->batch_id, batch_size);
      ok = true;
    }
  }
  portEXIT_CRITICAL(&lock);
  return ok;
}

void packers_on_saved(const char *packer_code, const WsoftHiloStamp *hilo) {
  bool changed = false;
  portENTER_CRITICAL(&lock);
  if (roster.require_packer_bags && packer_code != nullptr && packer_code[0] != 0) {
    changed = wsoft_packer_record_save(&roster, packer_code, hilo);
    strip.touch_active(millis());
  }
  portEXIT_CRITICAL(&lock);
  if (changed) save_state(true, false, false);
}

void packers_on_void(const char *packer_code, const char *batch_id) {
  bool changed = false;
  portENTER_CRITICAL(&lock);
  if (roster.require_packer_bags && packer_code != nullptr && packer_code[0] != 0) {
    changed = wsoft_packer_record_void(&roster, packer_code, batch_id);
  }
  portEXIT_CRITICAL(&lock);
  if (changed) save_state(true, false, false);
}

WsoftPackerPinResult packers_sign_in_offline(const char *code, const char *pin, uint32_t now_ms,
                                             uint32_t *locked_s_out, WsoftPackerEntry *out_entry) {
  WsoftPackerPinResult result = WSOFT_PACKER_PIN_NOT_SET;
  uint32_t prev_fails = 0, next_fails = 0, locked_s = 0;

  bool found = false;
  bool has_verifier = false;
  bool locked_now = false;
  uint8_t salt[WSOFT_PACKER_PIN_SALT_SIZE] = {};
  uint8_t hash[32] = {};

  portENTER_CRITICAL(&lock);
  prev_fails = pin_guard.fails();
  locked_now = pin_guard.locked_s(now_ms) > 0;
  const WsoftPackerEntry *entry = wsoft_packer_find(roster, code);
  if (entry != nullptr) {
    found = true;
    has_verifier = entry->has_verifier;
    if (has_verifier && !locked_now) {
      memcpy(salt, entry->pin_salt, sizeof salt);
      memcpy(hash, entry->pin_sha256, sizeof hash);
    }
  }
  portEXIT_CRITICAL(&lock);

  bool pin_ok = (found && has_verifier && !locked_now) && wsoft_packer_pin_matches(salt, hash, pin);

  portENTER_CRITICAL(&lock);
  if (!found || !has_verifier) {
    result = WSOFT_PACKER_PIN_NOT_SET;
  } else if (pin_guard.locked_s(now_ms) > 0) {
    result = WSOFT_PACKER_PIN_LOCKED;
  } else if (pin_ok) {
    const WsoftPackerEntry *fresh = wsoft_packer_find(roster, code);
    if (fresh != nullptr && fresh->has_verifier && memcmp(fresh->pin_salt, salt, sizeof salt) == 0 &&
        memcmp(fresh->pin_sha256, hash, sizeof hash) == 0) {
      pin_guard.right();
      if (!fresh->floor_manager) strip.sign_in(fresh->code, now_ms);
      if (out_entry) *out_entry = *fresh;
      next_pull_ms = now_ms + AFTER_CHANGE_MS;
      result = WSOFT_PACKER_PIN_OK;
    } else {
      result = WSOFT_PACKER_PIN_NOT_SET;
    }
  } else {
    pin_guard.wrong(now_ms);
    result = pin_guard.locked_s(now_ms) > 0 ? WSOFT_PACKER_PIN_LOCKED : WSOFT_PACKER_PIN_WRONG;
  }
  next_fails = pin_guard.fails();
  locked_s = pin_guard.locked_s(now_ms);
  portEXIT_CRITICAL(&lock);

  memset(salt, 0, sizeof salt);
  memset(hash, 0, sizeof hash);

  if (locked_s_out) *locked_s_out = locked_s;
  if (next_fails != prev_fails) save_state(false, false, true);
  return result;
}

WsoftPackerPinResult packers_sign_in_offline(const char *code, const char *pin, WsoftPackerEntry *out_entry) {
  return packers_sign_in_offline(code, pin, millis(), nullptr, out_entry);
}

WsoftPackerPinResult packers_verify_manager_pin(const char *pin, uint32_t now_ms, uint32_t *locked_s_out,
                                                char *actor_code, size_t actor_code_size, char *actor_name,
                                                size_t actor_name_size) {
  if (actor_code && actor_code_size > 0) actor_code[0] = 0;
  if (actor_name && actor_name_size > 0) actor_name[0] = 0;

  struct MgrCandidate {
    char code[WSOFT_PACKER_CODE_MAX + 1];
    char name[WSOFT_PACKER_NAME_MAX + 1];
    uint8_t salt[WSOFT_PACKER_PIN_SALT_SIZE];
    uint8_t hash[32];
  };
  static MgrCandidate *candidates = new MgrCandidate[WSOFT_PACKERS_MAX];  // heap: see roster
  if (mgr_mutex == nullptr) return WSOFT_PACKER_PIN_NOT_SET;

  WsoftPackerPinResult result = WSOFT_PACKER_PIN_NOT_SET;
  uint32_t prev_fails = 0, next_fails = 0, locked_s = 0;
  bool locked_now = false;
  size_t count = 0;

  xSemaphoreTake(mgr_mutex, portMAX_DELAY);
  portENTER_CRITICAL(&lock);
  prev_fails = pin_guard.fails();
  locked_now = pin_guard.locked_s(now_ms) > 0;
  if (!locked_now) {
    for (size_t i = 0; i < roster.count && i < WSOFT_PACKERS_MAX; i++) {
      const WsoftPackerEntry &p = roster.packers[i];
      if (!p.floor_manager || !p.has_verifier) continue;
      MgrCandidate &c = candidates[count++];
      strlcpy(c.code, p.code, sizeof c.code);
      strlcpy(c.name, p.name[0] ? p.name : p.code, sizeof c.name);
      memcpy(c.salt, p.pin_salt, sizeof c.salt);
      memcpy(c.hash, p.pin_sha256, sizeof c.hash);
    }
  }
  portEXIT_CRITICAL(&lock);

  int matched_idx = -1;
  if (!locked_now) {
    for (size_t i = 0; i < count; i++) {
      if (wsoft_packer_pin_matches(candidates[i].salt, candidates[i].hash, pin)) {
        matched_idx = (int)i;
        break;
      }
    }
  }

  portENTER_CRITICAL(&lock);
  if (pin_guard.locked_s(now_ms) > 0) {
    result = WSOFT_PACKER_PIN_LOCKED;
  } else if (matched_idx >= 0) {
    const MgrCandidate &cand = candidates[matched_idx];
    const WsoftPackerEntry *cur = wsoft_packer_find(roster, cand.code);
    if (cur != nullptr && cur->floor_manager && cur->has_verifier &&
        memcmp(cur->pin_salt, cand.salt, sizeof cand.salt) == 0 &&
        memcmp(cur->pin_sha256, cand.hash, sizeof cand.hash) == 0) {
      pin_guard.right();
      if (actor_code && actor_code_size > 0) strlcpy(actor_code, cur->code, actor_code_size);
      if (actor_name && actor_name_size > 0) strlcpy(actor_name, cur->name[0] ? cur->name : cur->code, actor_name_size);
      result = WSOFT_PACKER_PIN_OK;
    } else {
      result = WSOFT_PACKER_PIN_WRONG;
    }
  } else if (count == 0) {
    result = WSOFT_PACKER_PIN_NOT_SET;
  } else {
    pin_guard.wrong(now_ms);
    result = pin_guard.locked_s(now_ms) > 0 ? WSOFT_PACKER_PIN_LOCKED : WSOFT_PACKER_PIN_WRONG;
  }
  next_fails = pin_guard.fails();
  locked_s = pin_guard.locked_s(now_ms);
  portEXIT_CRITICAL(&lock);

  memset(candidates, 0, sizeof candidates[0] * count);
  xSemaphoreGive(mgr_mutex);

  if (locked_s_out) *locked_s_out = locked_s;
  if (next_fails != prev_fails) save_state(false, false, true);
  return result;
}

bool packers_single_manager_code(char *code_out, size_t code_size) {
  if (code_out && code_size > 0) code_out[0] = 0;
  size_t mgr_count = 0;
  char found[WSOFT_PACKER_CODE_MAX + 1] = "";
  portENTER_CRITICAL(&lock);
  for (size_t i = 0; i < roster.count && i < WSOFT_PACKERS_MAX; i++) {
    if (!roster.packers[i].floor_manager) continue;
    mgr_count++;
    if (mgr_count == 1) strlcpy(found, roster.packers[i].code, sizeof found);
  }
  portEXIT_CRITICAL(&lock);
  if (mgr_count != 1) return false;
  if (code_out && code_size > 0) strlcpy(code_out, found, code_size);
  return true;
}

void packers_apply_auth(const WsoftPackerAuthReply &reply) {
  uint32_t now = millis();
  bool roster_changed = false;
  portENTER_CRITICAL(&lock);
  if (reply.ok) {
    if (strcmp(reply.action, "sign_out") == 0) {
      strip.sign_out(reply.code);
      next_pull_ms = now + AFTER_CHANGE_MS;
    } else if (strcmp(reply.action, "sign_in") == 0) {
      WsoftPackerEntry *entry = wsoft_packer_find_mut(&roster, reply.code);
      if (entry != nullptr) {
        strlcpy(entry->packer_id, reply.packer_id, sizeof entry->packer_id);
        strlcpy(entry->name, reply.name, sizeof entry->name);
        entry->floor_manager = reply.floor_manager;
        entry->has_batch = reply.has_batch;
        strlcpy(entry->batch_id, reply.batch_id, sizeof entry->batch_id);
        entry->issued = reply.issued;
        entry->remaining = reply.remaining;
        entry->signed_in_here = !reply.floor_manager;
        roster_changed = true;
      }
      if (!reply.floor_manager) strip.sign_in(reply.code, now);
      next_pull_ms = now + AFTER_CHANGE_MS;
    }
  }
  portEXIT_CRITICAL(&lock);
  if (roster_changed) save_state(true, false, false);
}

bool packers_select(const char *code, uint32_t now_ms) {
  portENTER_CRITICAL(&lock);
  bool ok = strip.select_code(code, now_ms);
  portEXIT_CRITICAL(&lock);
  return ok;
}

bool packers_select(const char *code) {
  return packers_select(code, millis());
}

bool packers_sign_out(const char *code) {
  portENTER_CRITICAL(&lock);
  bool ok = strip.sign_out(code);
  if (ok) next_pull_ms = millis() + AFTER_CHANGE_MS;
  portEXIT_CRITICAL(&lock);
  return ok;
}

void packers_tick(uint32_t now_ms) {
  portENTER_CRITICAL(&lock);
  if (roster.require_packer_bags) {
    strip.expire_idle(now_ms, WSOFT_PACKER_IDLE_DEFAULT_MS);
  }
  portEXIT_CRITICAL(&lock);
}

WsoftBagEventError packers_floor_issue(const char *event_id, const char *actor_code, const char *packer_code,
                                       int32_t quantity, const char *reason_note, const char *occurred_at) {
  portENTER_CRITICAL(&lock);
  WsoftBagEventError err =
      wsoft_packer_floor_issue(&roster, &outbox, event_id, actor_code, packer_code, quantity, reason_note, occurred_at);
  if (err == WSOFT_BAG_OK) next_events_ms = millis();
  portEXIT_CRITICAL(&lock);
  if (err == WSOFT_BAG_OK) save_state(true, true, false);
  return err;
}

WsoftBagEventError packers_floor_handin(const char *event_id, const char *actor_code, const char *packer_code,
                                        int32_t lost_qty, int32_t returned_qty, const char *reason_code,
                                        const char *reason_note, bool close_batch, const char *occurred_at) {
  portENTER_CRITICAL(&lock);
  WsoftBagEventError err = wsoft_packer_floor_handin(&roster, &outbox, event_id, actor_code, packer_code, lost_qty,
                                                     returned_qty, reason_code, reason_note, close_batch, occurred_at);
  if (err == WSOFT_BAG_OK) next_events_ms = millis();
  portEXIT_CRITICAL(&lock);
  if (err == WSOFT_BAG_OK) save_state(true, true, false);
  return err;
}

bool packers_pull_due() {
  portENTER_CRITICAL(&lock);
  bool due = (int32_t)(millis() - next_pull_ms) >= 0;
  portEXIT_CRITICAL(&lock);
  return due;
}

bool packers_request(char *out, size_t size) {
  portENTER_CRITICAL(&lock);
  bool ok = wsoft_packers_request(strip, out, size);
  portEXIT_CRITICAL(&lock);
  return ok;
}

void packers_take(const WsoftPackerRoster &fresh) {
  uint32_t now = millis();
  bool changed = false;
  portENTER_CRITICAL(&lock);
  changed = !roster_equal(roster, fresh);
  roster = fresh;
  strip.sync_roster(roster, now);
  next_pull_ms = now + PULL_MS;
  last_ok_ms = now;
  portEXIT_CRITICAL(&lock);
  if (changed) {
    save_state(true, false, false);
    WSOFT_LOG.printf("PACKERS synced required=%d count=%u unit=%s\n",
                     fresh.require_packer_bags ? 1 : 0, (unsigned)fresh.count, fresh.pack_unit_label);
  }
}

void packers_failed() {
  portENTER_CRITICAL(&lock);
  next_pull_ms = millis() + RETRY_MS;
  portEXIT_CRITICAL(&lock);
}

bool packers_bag_events_due() {
  portENTER_CRITICAL(&lock);
  bool due = outbox.size() > 0 && (int32_t)(millis() - next_events_ms) >= 0;
  portEXIT_CRITICAL(&lock);
  return due;
}

bool packers_bag_events_request(char *out, size_t size, size_t *count) {
  static WsoftBagEvent *copy = new WsoftBagEvent[WSOFT_BAG_OUTBOX_MAX];  // heap: see roster
  size_t n = 0;
  portENTER_CRITICAL(&lock);
  n = outbox.size();
  for (size_t i = 0; i < n; i++) copy[i] = outbox.at(i);
  portEXIT_CRITICAL(&lock);
  if (count) *count = n;
  if (n == 0) return false;
  return wsoft_bag_events_request(copy, n, out, size);
}

void packers_bag_events_take(const WsoftBagEventResult *results, size_t count) {
  size_t handled = 0;
  portENTER_CRITICAL(&lock);
  handled = outbox.apply_results(results, count, &roster);
  next_events_ms = millis() + (outbox.size() > 0 ? RETRY_MS : PULL_MS);
  if (handled > 0) next_pull_ms = millis() + AFTER_CHANGE_MS;
  portEXIT_CRITICAL(&lock);
  if (handled > 0) save_state(true, true, false);
}

void packers_bag_events_failed() {
  portENTER_CRITICAL(&lock);
  next_events_ms = millis() + RETRY_MS;
  portEXIT_CRITICAL(&lock);
}

void packers_info(PackersInfo *out) {
  if (out == nullptr) return;
  portENTER_CRITICAL(&lock);
  out->required = roster.require_packer_bags;
  strlcpy(out->pack_unit_label, roster.pack_unit_label, sizeof out->pack_unit_label);
  out->roster_count = roster.count;
  out->strip_count = strip.count();
  strlcpy(out->active_code, strip.active(), sizeof out->active_code);
  const WsoftPackerEntry *p = wsoft_packer_find(roster, strip.active());
  out->active_remaining = p != nullptr ? p->remaining : 0;
  out->outbox_pending = outbox.size();
  out->last_ok_ms = last_ok_ms;
  portEXIT_CRITICAL(&lock);
}

void packers_pack_unit(char *out, size_t size) {
  if (out == nullptr || size == 0) return;
  portENTER_CRITICAL(&lock);
  strlcpy(out, roster.pack_unit_label, size);
  portEXIT_CRITICAL(&lock);
}

void packers_active_code(char *out, size_t size) {
  if (out == nullptr || size == 0) return;
  portENTER_CRITICAL(&lock);
  strlcpy(out, strip.active(), size);
  portEXIT_CRITICAL(&lock);
}

uint32_t packers_guard_locked_s(uint32_t now_ms) {
  portENTER_CRITICAL(&lock);
  uint32_t s = pin_guard.locked_s(now_ms);
  portEXIT_CRITICAL(&lock);
  return s;
}

int packers_guard_tries_left() {
  portENTER_CRITICAL(&lock);
  int left = pin_guard.tries_left();
  portEXIT_CRITICAL(&lock);
  return left;
}

void packers_ui_snapshot(bool *required_out, char *unit_out, size_t unit_size, WsoftUiStripEntry *strip_out,
                         uint8_t *strip_count_out, uint8_t picker_page, uint8_t *picker_page_out,
                         uint8_t *picker_pages_out, WsoftUiPickerEntry *picker_out, uint8_t *picker_count_out) {
  portENTER_CRITICAL(&lock);
  if (required_out) *required_out = roster.require_packer_bags;
  if (unit_out && unit_size > 0) strlcpy(unit_out, roster.pack_unit_label, unit_size);

  uint8_t sc = (uint8_t)(strip.count() > WSOFT_UI_STRIP_MAX ? WSOFT_UI_STRIP_MAX : strip.count());
  if (strip_count_out) *strip_count_out = sc;
  if (strip_out != nullptr) {
    const char *active = strip.active();
    for (uint8_t i = 0; i < sc; i++) {
      const char *code = strip.slot_code(i);
      const WsoftPackerEntry *p = wsoft_packer_find(roster, code);
      strlcpy(strip_out[i].code, code, sizeof strip_out[i].code);
      strlcpy(strip_out[i].name, (p && p->name[0]) ? p->name : code, sizeof strip_out[i].name);
      strip_out[i].remaining = p ? p->remaining : 0;
      strip_out[i].active = (active[0] != 0 && eq_ci(active, code));
    }
  }

  uint8_t total_packers = 0;
  for (size_t i = 0; i < roster.count && i < WSOFT_PACKERS_MAX; i++) {
    if (!roster.packers[i].floor_manager) total_packers++;
  }
  uint8_t pages = total_packers == 0
                      ? 0
                      : (uint8_t)((total_packers + WSOFT_UI_PICKER_MAX - 1) / WSOFT_UI_PICKER_MAX);
  uint8_t pg = (pages > 0 && picker_page < pages) ? picker_page : (pages > 0 ? (uint8_t)(pages - 1) : 0);
  if (picker_pages_out) *picker_pages_out = pages;
  if (picker_page_out) *picker_page_out = pg;

  uint8_t pk = 0;
  if (picker_out != nullptr && pages > 0) {
    uint8_t skip = (uint8_t)(pg * WSOFT_UI_PICKER_MAX);
    uint8_t seen = 0;
    const char *active = strip.active();
    for (size_t i = 0; i < roster.count && i < WSOFT_PACKERS_MAX && pk < WSOFT_UI_PICKER_MAX; i++) {
      const WsoftPackerEntry &p = roster.packers[i];
      if (p.floor_manager) continue;
      if (seen++ < skip) continue;
      strlcpy(picker_out[pk].code, p.code, sizeof picker_out[pk].code);
      strlcpy(picker_out[pk].name, p.name[0] ? p.name : p.code, sizeof picker_out[pk].name);
      picker_out[pk].remaining = p.remaining;
      picker_out[pk].signed_in = strip.contains(p.code);
      picker_out[pk].active = (active[0] != 0 && eq_ci(active, p.code));
      picker_out[pk].has_batch = p.has_batch;
      pk++;
    }
  }
  if (picker_count_out) *picker_count_out = pk;
  portEXIT_CRITICAL(&lock);
}

bool packers_floor_target(uint8_t index, uint8_t *index_out, uint8_t *total_out, char *code_out, size_t code_size,
                          char *name_out, size_t name_size, int32_t *issued_out, int32_t *remaining_out,
                          bool *has_batch_out) {
  if (code_out && code_size > 0) code_out[0] = 0;
  if (name_out && name_size > 0) name_out[0] = 0;
  if (issued_out) *issued_out = 0;
  if (remaining_out) *remaining_out = 0;
  if (has_batch_out) *has_batch_out = false;
  if (index_out) *index_out = 0;
  if (total_out) *total_out = 0;

  portENTER_CRITICAL(&lock);
  uint8_t total = 0;
  for (size_t i = 0; i < roster.count && i < WSOFT_PACKERS_MAX; i++) {
    if (!roster.packers[i].floor_manager) total++;
  }
  if (total_out) *total_out = total;
  if (total == 0) {
    portEXIT_CRITICAL(&lock);
    return false;
  }
  uint8_t target_idx = index % total;
  if (index_out) *index_out = target_idx;
  uint8_t seen = 0;
  for (size_t i = 0; i < roster.count && i < WSOFT_PACKERS_MAX; i++) {
    const WsoftPackerEntry &p = roster.packers[i];
    if (p.floor_manager) continue;
    if (seen++ == target_idx) {
      if (code_out && code_size > 0) strlcpy(code_out, p.code, code_size);
      if (name_out && name_size > 0) strlcpy(name_out, p.name[0] ? p.name : p.code, name_size);
      if (issued_out) *issued_out = p.issued;
      if (remaining_out) *remaining_out = p.remaining;
      if (has_batch_out) *has_batch_out = p.has_batch;
      break;
    }
  }
  portEXIT_CRITICAL(&lock);
  return true;
}

void packers_print_list(Stream &port) {
  static WsoftPackerRoster &copy = *new WsoftPackerRoster();  // heap: see roster
  char active[WSOFT_PACKER_CODE_MAX + 1] = "";
  portENTER_CRITICAL(&lock);
  copy = roster;
  strlcpy(active, strip.active(), sizeof active);
  portEXIT_CRITICAL(&lock);

  port.printf("PACKERS required=%d unit=%s count=%u active=%s\n",
              copy.require_packer_bags ? 1 : 0, copy.pack_unit_label, (unsigned)copy.count,
              active[0] ? active : "-");
  for (size_t i = 0; i < copy.count && i < WSOFT_PACKERS_MAX; i++) {
    const WsoftPackerEntry &p = copy.packers[i];
    port.printf("PACKER code=%s name=%s floor_manager=%d remaining=%ld\n",
                p.code, p.name, p.floor_manager ? 1 : 0, (long)p.remaining);
  }
}
