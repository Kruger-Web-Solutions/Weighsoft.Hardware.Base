#include "wsoft_log.h"
#include "pin.h"

#include <Preferences.h>
#include <esp_random.h>
#include <freertos/semphr.h>

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
// Held across a whole save or a whole try (hash and NVS write), so a try on the screen can
// never write its old guard or fail count over a PIN the web page just saved.
static SemaphoreHandle_t busy = nullptr;

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
  if (busy == nullptr) busy = xSemaphoreCreateMutex();
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
  WSOFT_LOG.printf("PIN %s\n", have ? "set" : "not set (an admin sets it on the web page)");
}

PinSaveResult pin_save(const char *pin) {
  if (!wsoft_pin_format_ok(pin)) return PIN_SAVE_FORMAT;
  // Not started yet, or a try is running: do not block the web server for long.
  if (busy == nullptr || xSemaphoreTake(busy, pdMS_TO_TICKS(PIN_SAVE_WAIT_MS)) != pdTRUE) return PIN_SAVE_BUSY;
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
  xSemaphoreGive(busy);
  memset(h, 0, sizeof h);
  WSOFT_LOG.println("PIN changed from the web page");
  return PIN_SAVE_OK;
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

int pin_tries_left() {
  portENTER_CRITICAL(&lock);
  int value = guard.tries_left();
  portEXIT_CRITICAL(&lock);
  return value;
}

WsoftPinResult pin_try(const char *entry) {
  if (busy == nullptr) return WSOFT_PIN_NOT_SET;  // pin_begin has not run
  xSemaphoreTake(busy, portMAX_DELAY);  // a save holds it for one NVS write
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
  xSemaphoreGive(busy);
  if (result == WSOFT_PIN_WRONG) {
    WSOFT_LOG.printf("PIN wrong try %lu of %lu\n", (unsigned long)g.fails(), (unsigned long)WSOFT_PIN_TRIES);
  } else if (result == WSOFT_PIN_LOCKED && fails_before != g.fails()) {
    WSOFT_LOG.printf("PIN locked for %lu s after %lu wrong tries\n", (unsigned long)(WSOFT_PIN_LOCK_MS / 1000),
                   (unsigned long)WSOFT_PIN_TRIES);
  }
  return result;
}
