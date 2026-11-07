#include "pin.h"

#include <Preferences.h>
#include <esp_random.h>

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

// Shared between the web task (save) and the screen task (try): under the lock.
static bool pin_set = false;
static uint8_t salt[WSOFT_PIN_SALT_SIZE];
static uint8_t hash[32];
static WsoftPinGuard guard;

static void store_fails(uint32_t fails) {
  Preferences store;
  store.begin("pin", false);
  store.putUInt("fails", fails);
  store.end();
}

void pin_begin() {
  Preferences store;
  store.begin("pin", true);
  uint8_t s[WSOFT_PIN_SALT_SIZE], h[32];
  bool have = store.getBytes("salt", s, sizeof s) == sizeof s && store.getBytes("hash", h, sizeof h) == sizeof h;
  uint32_t fails = store.getUInt("fails", 0);
  store.end();
  portENTER_CRITICAL(&lock);
  pin_set = have;
  if (have) {
    memcpy(salt, s, sizeof salt);
    memcpy(hash, h, sizeof hash);
  }
  guard = WsoftPinGuard(fails, millis());
  portEXIT_CRITICAL(&lock);
  Serial0.printf("PIN %s\n", have ? "set" : "not set (an admin sets it on the web page)");
}

bool pin_save(const char *pin) {
  if (!wsoft_pin_format_ok(pin)) return false;
  uint8_t s[WSOFT_PIN_SALT_SIZE], h[32];
  esp_fill_random(s, sizeof s);
  wsoft_pin_hash(s, pin, h);
  Preferences store;
  store.begin("pin", false);
  store.putBytes("salt", s, sizeof s);
  store.putBytes("hash", h, sizeof h);
  store.putUInt("fails", 0);
  store.end();
  portENTER_CRITICAL(&lock);
  memcpy(salt, s, sizeof salt);
  memcpy(hash, h, sizeof hash);
  pin_set = true;
  guard = WsoftPinGuard();
  portEXIT_CRITICAL(&lock);
  memset(h, 0, sizeof h);
  Serial0.println("PIN changed from the web page");
  return true;
}

bool pin_is_set() {
  portENTER_CRITICAL(&lock);
  bool value = pin_set;
  portEXIT_CRITICAL(&lock);
  return value;
}

uint32_t pin_locked_s() {
  uint32_t now = millis();
  portENTER_CRITICAL(&lock);
  uint32_t value = guard.locked_s(now);
  portEXIT_CRITICAL(&lock);
  return value;
}

WsoftPinResult pin_try(const char *entry) {
  uint8_t s[WSOFT_PIN_SALT_SIZE], h[32];
  portENTER_CRITICAL(&lock);
  bool set = pin_set;
  memcpy(s, salt, sizeof s);
  memcpy(h, hash, sizeof h);
  WsoftPinGuard g = guard;
  portEXIT_CRITICAL(&lock);

  uint32_t fails_before = g.fails();
  // The hash runs outside the spinlock.
  WsoftPinResult result = wsoft_pin_try(&g, set, s, h, entry, millis());
  memset(h, 0, sizeof h);

  portENTER_CRITICAL(&lock);
  guard = g;
  portEXIT_CRITICAL(&lock);
  if (g.fails() != fails_before) store_fails(g.fails());
  if (result == WSOFT_PIN_WRONG) {
    Serial0.printf("PIN wrong try %lu of %lu\n", (unsigned long)g.fails(), (unsigned long)WSOFT_PIN_TRIES);
  } else if (result == WSOFT_PIN_LOCKED && fails_before != g.fails()) {
    Serial0.printf("PIN locked for %lu s after %lu wrong tries\n", (unsigned long)(WSOFT_PIN_LOCK_MS / 1000),
                   (unsigned long)WSOFT_PIN_TRIES);
  }
  return result;
}
