#include "hilo.h"

#include <Preferences.h>
#include <freertos/semphr.h>
#include <string.h>

static const uint32_t PULL_MS = 60000;
static const uint32_t RETRY_MS = 15000;
// After the operator picks a product the Hub hears it within a couple of seconds.
static const uint32_t AFTER_PICK_MS = 2000;
static const uint32_t STORED_VERSION = 1;

// Shared between tasks: read and written under the lock.
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static WsoftHiloList list;
static WsoftHiloPick pick;
static uint32_t next_pull_ms = 0;
static uint32_t last_ok_ms = 0;

// Flash writes from the Hub and screen tasks take turns.
static SemaphoreHandle_t flash_mutex = nullptr;

static void save(bool with_list) {
  // Copies first: the flash write is slow and must not hold the spinlock.
  static WsoftHiloList list_copy;
  WsoftHiloPick pick_copy;
  xSemaphoreTake(flash_mutex, portMAX_DELAY);
  portENTER_CRITICAL(&lock);
  if (with_list) list_copy = list;
  pick_copy = pick;
  portEXIT_CRITICAL(&lock);
  Preferences store;
  store.begin("hilo", false);
  store.putUInt("ver", STORED_VERSION);
  store.putString("cur", pick_copy.current);
  store.putString("set", pick_copy.applied_set_at);
  if (with_list) store.putBytes("list", &list_copy, sizeof list_copy);
  store.end();
  xSemaphoreGive(flash_mutex);
}

void hilo_begin() {
  flash_mutex = xSemaphoreCreateMutex();
  static WsoftHiloList stored;
  WsoftHiloPick stored_pick;
  Preferences store;
  store.begin("hilo", true);
  bool known = store.getUInt("ver", 0) == STORED_VERSION;
  if (known && store.getBytesLength("list") == sizeof stored &&
      store.getBytes("list", &stored, sizeof stored) == sizeof stored && stored.count <= WSOFT_HILO_MAX) {
    store.getString("cur", stored_pick.current, sizeof stored_pick.current);
    store.getString("set", stored_pick.applied_set_at, sizeof stored_pick.applied_set_at);
  } else {
    stored = WsoftHiloList();
  }
  store.end();
  // A pick the list does not hold is not used.
  if (wsoft_hilo_find(stored, stored_pick.current) == nullptr) stored_pick.current[0] = 0;
  portENTER_CRITICAL(&lock);
  list = stored;
  pick = stored_pick;
  portEXIT_CRITICAL(&lock);
  Serial0.printf("HILO products=%u current=%s\n", (unsigned)stored.count,
                 stored_pick.current[0] ? stored_pick.current : "none");
}

bool hilo_current(WsoftHiloProduct *out) {
  portENTER_CRITICAL(&lock);
  const WsoftHiloProduct *p = wsoft_hilo_find(list, pick.current);
  if (p != nullptr) *out = *p;
  portEXIT_CRITICAL(&lock);
  return p != nullptr;
}

void hilo_select_next() {
  char code[WSOFT_HILO_CODE_MAX + 1];
  portENTER_CRITICAL(&lock);
  strlcpy(pick.current, wsoft_hilo_next(list, pick.current), sizeof pick.current);
  strlcpy(code, pick.current, sizeof code);
  next_pull_ms = millis() + AFTER_PICK_MS;
  portEXIT_CRITICAL(&lock);
  save(false);
  Serial0.printf("HILO operator picked %s\n", code[0] ? code : "no product");
}

void hilo_follow(const char *code) {
  if (code == nullptr) return;
  char now[WSOFT_HILO_CODE_MAX + 1];
  portENTER_CRITICAL(&lock);
  const char *want = wsoft_hilo_find(list, code) != nullptr ? code : "";
  bool same = strcmp(pick.current, want) == 0;
  if (!same) {
    strlcpy(pick.current, want, sizeof pick.current);
    next_pull_ms = millis() + AFTER_PICK_MS;
  }
  strlcpy(now, pick.current, sizeof now);
  portEXIT_CRITICAL(&lock);
  if (same) return;
  save(false);
  Serial0.printf("HILO following the weigh board: %s\n", now[0] ? now : "no product");
}

bool hilo_pull_due() {
  portENTER_CRITICAL(&lock);
  bool due = (int32_t)(millis() - next_pull_ms) >= 0;
  portEXIT_CRITICAL(&lock);
  return due;
}

bool hilo_request(char *out, size_t size) {
  char code[WSOFT_HILO_CODE_MAX + 1];
  portENTER_CRITICAL(&lock);
  strlcpy(code, pick.current, sizeof code);
  portEXIT_CRITICAL(&lock);
  return wsoft_hilo_request(code, out, size);
}

void hilo_take(const WsoftHiloList &fresh) {
  portENTER_CRITICAL(&lock);
  bool list_changed = !wsoft_hilo_same(list, fresh);
  list = fresh;
  WsoftHiloPick before = pick;
  bool pick_changed = wsoft_hilo_apply(list, &pick);
  WsoftHiloPick after = pick;
  next_pull_ms = millis() + PULL_MS;
  last_ok_ms = millis();
  portEXIT_CRITICAL(&lock);
  // Flash is written only when something changed (a pull a minute must not wear it out).
  if (list_changed || pick_changed) save(list_changed);
  if (pick_changed && strcmp(before.current, after.current) != 0) {
    Serial0.printf("HILO product %s (Hub default or product removed)\n", after.current[0] ? after.current : "none");
  }
  if (list_changed) Serial0.printf("HILO list now %u products\n", (unsigned)fresh.count);
}

void hilo_failed() {
  portENTER_CRITICAL(&lock);
  next_pull_ms = millis() + RETRY_MS;
  portEXIT_CRITICAL(&lock);
}

void hilo_info(HiloInfo *out) {
  portENTER_CRITICAL(&lock);
  out->count = list.count;
  const WsoftHiloProduct *p = wsoft_hilo_find(list, pick.current);
  out->picked = p != nullptr;
  out->current = p != nullptr ? *p : WsoftHiloProduct();
  out->last_ok_ms = last_ok_ms;
  portEXIT_CRITICAL(&lock);
}
