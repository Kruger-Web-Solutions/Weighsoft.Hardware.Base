#include "packers.h"

#include "wsoft_log.h"

#include <Preferences.h>
#include <freertos/semphr.h>
#include <string.h>

static const uint32_t PULL_MS = 60000;
static const uint32_t RETRY_MS = 15000;
static const uint32_t AFTER_CHANGE_MS = 2000;
static const uint32_t STORED_VERSION = 1;

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t flash_mutex = nullptr;

static WsoftPackerRoster roster;  // require_packer_bags defaults to false
static WsoftPackerStrip strip;
static WsoftPackerPinGuard pin_guard;
static WsoftBagEventOutbox outbox;

static uint32_t next_pull_ms = 0;
static uint32_t next_events_ms = 0;
static uint32_t last_ok_ms = 0;

static void save_state(bool with_roster, bool with_outbox, bool with_pin_fails) {
  if (flash_mutex == nullptr) return;
  static WsoftPackerRoster roster_copy;
  static WsoftBagEventOutbox outbox_copy;
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
  static WsoftPackerRoster stored_roster;
  static WsoftBagEventOutbox stored_outbox;
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

WsoftPackerPinResult packers_sign_in_offline(const char *code, const char *pin, WsoftPackerEntry *out_entry) {
  uint32_t now = millis();
  WsoftPackerPinResult result = WSOFT_PACKER_PIN_NOT_SET;
  uint32_t prev_fails = 0, next_fails = 0;

  portENTER_CRITICAL(&lock);
  prev_fails = pin_guard.fails();
  const WsoftPackerEntry *entry = wsoft_packer_find(roster, code);
  if (entry != nullptr) {
    result = wsoft_packer_pin_try(&pin_guard, entry->has_verifier, entry->pin_salt, entry->pin_sha256, pin, now);
    if (result == WSOFT_PACKER_PIN_OK) {
      if (!entry->floor_manager) strip.sign_in(entry->code, now);
      if (out_entry) *out_entry = *entry;
      next_pull_ms = now + AFTER_CHANGE_MS;
    }
  } else {
    result = WSOFT_PACKER_PIN_NOT_SET;
  }
  next_fails = pin_guard.fails();
  portEXIT_CRITICAL(&lock);

  if (next_fails != prev_fails) save_state(false, false, true);
  return result;
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

bool packers_select(const char *code) {
  uint32_t now = millis();
  portENTER_CRITICAL(&lock);
  bool ok = strip.select_code(code, now);
  portEXIT_CRITICAL(&lock);
  return ok;
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
  static WsoftBagEvent copy[WSOFT_BAG_OUTBOX_MAX];
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
