#include "wsoft_log.h"
#include "hilo.h"

#include <wsoft_products.h>

#include <Preferences.h>
#include <freertos/semphr.h>
#include <string.h>

static const uint32_t PULL_MS = 60000;
static const uint32_t RETRY_MS = 15000;
// After the operator picks a product the Hub hears it within a couple of seconds.
static const uint32_t AFTER_PICK_MS = 2000;
static const uint32_t STORED_VERSION = 1;
// Board products (wsoft_products) have their own key and version in the same namespace.
static const uint32_t LOCAL_VERSION = 1;

// Shared between tasks: read and written under the lock.
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static WsoftHiloList list;
static WsoftLocalProducts local;
static WsoftHiloPick pick;
static uint32_t next_pull_ms = 0;
static uint32_t last_ok_ms = 0;

// Flash writes from the Hub and screen tasks take turns.
static SemaphoreHandle_t flash_mutex = nullptr;

static void save(bool with_list, bool with_local = false) {
  // Copies first: the flash write is slow and must not hold the spinlock.
  static WsoftHiloList list_copy;
  static WsoftLocalProducts local_copy;
  WsoftHiloPick pick_copy;
  xSemaphoreTake(flash_mutex, portMAX_DELAY);
  portENTER_CRITICAL(&lock);
  if (with_list) list_copy = list;
  if (with_local) local_copy = local;
  pick_copy = pick;
  portEXIT_CRITICAL(&lock);
  Preferences store;
  store.begin("hilo", false);
  store.putUInt("ver", STORED_VERSION);
  store.putString("cur", pick_copy.current);
  store.putString("set", pick_copy.applied_set_at);
  if (with_list) store.putBytes("list", &list_copy, sizeof list_copy);
  if (with_local) {
    store.putUInt("lver", LOCAL_VERSION);
    store.putBytes("local", &local_copy, sizeof local_copy);
  }
  store.end();
  xSemaphoreGive(flash_mutex);
}

void hilo_begin() {
  flash_mutex = xSemaphoreCreateMutex();
  static WsoftHiloList stored;
  static WsoftLocalProducts stored_local;
  WsoftHiloPick stored_pick;
  Preferences store;
  store.begin("hilo", true);
  bool known = store.getUInt("ver", 0) == STORED_VERSION;
  if (known && store.getBytesLength("list") == sizeof stored &&
      store.getBytes("list", &stored, sizeof stored) == sizeof stored && stored.count <= WSOFT_HILO_MAX) {
    store.getString("set", stored_pick.applied_set_at, sizeof stored_pick.applied_set_at);
  } else {
    stored = WsoftHiloList();
  }
  // The pick is read even with no Hub list (it may name a board product); the check below
  // drops it when no list holds it.
  if (known) store.getString("cur", stored_pick.current, sizeof stored_pick.current);
  // Board products are read even when the Hub list is missing (a board with no Hub).
  if (!(store.getUInt("lver", 0) == LOCAL_VERSION && store.getBytesLength("local") == sizeof stored_local &&
        store.getBytes("local", &stored_local, sizeof stored_local) == sizeof stored_local &&
        wsoft_products_valid(stored_local))) {
    stored_local = WsoftLocalProducts();
  }
  store.end();
  // A pick no list holds is not used.
  if (wsoft_products_find(stored, stored_local, stored_pick.current) == nullptr) stored_pick.current[0] = 0;
  portENTER_CRITICAL(&lock);
  list = stored;
  local = stored_local;
  pick = stored_pick;
  portEXIT_CRITICAL(&lock);
  WSOFT_LOG.printf("HILO products=%u board=%u current=%s\n", (unsigned)stored.count, (unsigned)stored_local.count,
                 stored_pick.current[0] ? stored_pick.current : "none");
}

bool hilo_current(WsoftHiloProduct *out) {
  portENTER_CRITICAL(&lock);
  const WsoftHiloProduct *p = wsoft_products_find(list, local, pick.current);
  if (p != nullptr) *out = *p;
  portEXIT_CRITICAL(&lock);
  return p != nullptr;
}

void hilo_select_next() {
  char code[WSOFT_HILO_CODE_MAX + 1];
  portENTER_CRITICAL(&lock);
  strlcpy(pick.current, wsoft_products_next(list, local, pick.current), sizeof pick.current);
  strlcpy(code, pick.current, sizeof code);
  next_pull_ms = millis() + AFTER_PICK_MS;
  portEXIT_CRITICAL(&lock);
  save(false);
  WSOFT_LOG.printf("HILO operator picked %s\n", code[0] ? code : "no product");
}

void hilo_follow(const char *code) {
  if (code == nullptr) return;
  char now[WSOFT_HILO_CODE_MAX + 1];
  portENTER_CRITICAL(&lock);
  const char *want = wsoft_products_find(list, local, code) != nullptr ? code : "";
  bool same = strcmp(pick.current, want) == 0;
  if (!same) {
    strlcpy(pick.current, want, sizeof pick.current);
    next_pull_ms = millis() + AFTER_PICK_MS;
  }
  strlcpy(now, pick.current, sizeof now);
  portEXIT_CRITICAL(&lock);
  if (same) return;
  save(false);
  WSOFT_LOG.printf("HILO following the weigh board: %s\n", now[0] ? now : "no product");
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
  bool pick_changed = wsoft_products_apply(list, local, &pick);
  WsoftHiloPick after = pick;
  next_pull_ms = millis() + PULL_MS;
  last_ok_ms = millis();
  portEXIT_CRITICAL(&lock);
  // Flash is written only when something changed (a pull a minute must not wear it out).
  if (list_changed || pick_changed) save(list_changed);
  if (pick_changed && strcmp(before.current, after.current) != 0) {
    WSOFT_LOG.printf("HILO product %s (Hub default or product removed)\n", after.current[0] ? after.current : "none");
  }
  if (list_changed) WSOFT_LOG.printf("HILO list now %u products\n", (unsigned)fresh.count);
}

void hilo_failed() {
  portENTER_CRITICAL(&lock);
  next_pull_ms = millis() + RETRY_MS;
  portEXIT_CRITICAL(&lock);
}

void hilo_info(HiloInfo *out) {
  portENTER_CRITICAL(&lock);
  out->count = wsoft_products_count(list, local);
  const WsoftHiloProduct *p = wsoft_products_find(list, local, pick.current);
  out->picked = p != nullptr;
  out->current = p != nullptr ? *p : WsoftHiloProduct();
  out->last_ok_ms = last_ok_ms;
  portEXIT_CRITICAL(&lock);
}

size_t hilo_rows(HiloRow *out, size_t max, char *current, size_t current_size) {
  size_t n = 0;
  portENTER_CRITICAL(&lock);
  for (size_t i = 0; i < list.count && n < max; i++, n++) {
    out[n].product = list.products[i];
    out[n].from_hub = true;
    out[n].hidden = false;
  }
  for (size_t i = 0; i < local.count && n < max; i++, n++) {
    out[n].product = local.products[i];
    out[n].from_hub = false;
    out[n].hidden = wsoft_products_hidden(list, local.products[i]);
  }
  if (current != nullptr && current_size > 0) strlcpy(current, pick.current, current_size);
  portEXIT_CRITICAL(&lock);
  return n;
}

WsoftProductResult hilo_board_put(const char *old_code, const WsoftHiloProduct &product) {
  portENTER_CRITICAL(&lock);
  WsoftProductResult result = wsoft_products_put(&local, list, old_code, product);
  bool renamed_pick = result == WSOFT_PRODUCT_OK && old_code != nullptr && old_code[0] != 0 &&
                      strcmp(pick.current, old_code) == 0 && strcmp(old_code, product.code) != 0;
  if (renamed_pick) {
    strlcpy(pick.current, product.code, sizeof pick.current);
    next_pull_ms = millis() + AFTER_PICK_MS;
  }
  portEXIT_CRITICAL(&lock);
  if (result != WSOFT_PRODUCT_OK) return result;
  save(false, true);
  WSOFT_LOG.printf("HILO board product %s %s\n", old_code != nullptr && old_code[0] ? "edited" : "added", product.code);
  return result;
}

WsoftProductResult hilo_board_remove(const char *code) {
  portENTER_CRITICAL(&lock);
  WsoftProductResult result = wsoft_products_remove(&local, code);
  bool was_picked = result == WSOFT_PRODUCT_OK && wsoft_products_find(list, local, pick.current) == nullptr &&
                    pick.current[0] != 0;
  if (was_picked) {
    pick.current[0] = 0;
    next_pull_ms = millis() + AFTER_PICK_MS;
  }
  portEXIT_CRITICAL(&lock);
  if (result != WSOFT_PRODUCT_OK) return result;
  save(false, true);
  WSOFT_LOG.printf("HILO board product deleted %s%s\n", code, was_picked ? " (was in use: now no product)" : "");
  return result;
}

bool hilo_pick(const char *code) {
  if (code == nullptr) return false;
  portENTER_CRITICAL(&lock);
  bool ok = code[0] == 0 || wsoft_products_find(list, local, code) != nullptr;
  bool changed = ok && strcmp(pick.current, code) != 0;
  if (changed) {
    strlcpy(pick.current, code, sizeof pick.current);
    next_pull_ms = millis() + AFTER_PICK_MS;
  }
  portEXIT_CRITICAL(&lock);
  if (changed) {
    save(false);
    WSOFT_LOG.printf("HILO picked on the web page: %s\n", code[0] ? code : "no product");
  }
  return ok;
}
