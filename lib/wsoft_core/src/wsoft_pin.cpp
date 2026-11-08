#include "wsoft_pin.h"

#include <string.h>

#include "wsoft_sha256.h"

bool wsoft_pin_format_ok(const char *pin) {
  if (pin == nullptr) return false;
  size_t length = strlen(pin);
  if (length < WSOFT_PIN_MIN || length > WSOFT_PIN_MAX) return false;
  for (size_t i = 0; i < length; i++) {
    if (pin[i] < '0' || pin[i] > '9') return false;
  }
  return true;
}

void wsoft_pin_hash(const uint8_t salt[WSOFT_PIN_SALT_SIZE], const char *pin, uint8_t out[32]) {
  WsoftSha256 ctx;
  wsoft_sha256_begin(&ctx);
  wsoft_sha256_add(&ctx, salt, WSOFT_PIN_SALT_SIZE);
  wsoft_sha256_add(&ctx, (const uint8_t *)pin, strlen(pin));
  wsoft_sha256_end(&ctx, out);
}

bool wsoft_pin_matches(const uint8_t salt[WSOFT_PIN_SALT_SIZE], const uint8_t hash[32], const char *pin) {
  if (!wsoft_pin_format_ok(pin)) return false;
  uint8_t given[32];
  wsoft_pin_hash(salt, pin, given);
  bool same = wsoft_same_bytes(given, hash, 32);
  memset(given, 0, sizeof given);
  return same;
}

const char *wsoft_pin_result_code(WsoftPinResult result) {
  switch (result) {
    case WSOFT_PIN_OK: return "";
    case WSOFT_PIN_WRONG: return "PIN_WRONG";
    case WSOFT_PIN_LOCKED: return "PIN_LOCKED";
    case WSOFT_PIN_NOT_SET: return "PIN_NOT_SET";
  }
  return "PIN_WRONG";
}

WsoftPinGuard::WsoftPinGuard(uint32_t fails, uint32_t now_ms) {
  if (fails >= WSOFT_PIN_TRIES) {
    locked_ = true;
    lock_from_ms_ = now_ms;
    fails_ = WSOFT_PIN_TRIES;
  } else {
    fails_ = fails;
  }
}

uint32_t WsoftPinGuard::locked_s(uint32_t now_ms) const {
  if (!locked_) return 0;
  uint32_t passed = now_ms - lock_from_ms_;  // unsigned: right across a millis() wrap
  if (passed >= WSOFT_PIN_LOCK_MS) return 0;
  return (WSOFT_PIN_LOCK_MS - passed + 999) / 1000;
}

void WsoftPinGuard::wrong(uint32_t now_ms) {
  if (locked_ && locked_s(now_ms) == 0) {
    locked_ = false;
    fails_ = 0;
  }
  if (locked_) return;
  fails_++;
  if (fails_ >= WSOFT_PIN_TRIES) {
    locked_ = true;
    lock_from_ms_ = now_ms;
  }
}

void WsoftPinGuard::right() {
  fails_ = 0;
  locked_ = false;
}

WsoftPinResult wsoft_pin_try(WsoftPinGuard *guard, bool pin_set, const uint8_t salt[WSOFT_PIN_SALT_SIZE],
                             const uint8_t hash[32], const char *entry, uint32_t now_ms) {
  if (!pin_set) return WSOFT_PIN_NOT_SET;
  if (guard->locked_s(now_ms) > 0) return WSOFT_PIN_LOCKED;
  if (wsoft_pin_matches(salt, hash, entry)) {
    guard->right();
    return WSOFT_PIN_OK;
  }
  guard->wrong(now_ms);
  return guard->locked_s(now_ms) > 0 ? WSOFT_PIN_LOCKED : WSOFT_PIN_WRONG;
}

void WsoftPinPad::press(char digit) {
  size_t length = strlen(entry_);
  if (digit < '0' || digit > '9' || length >= WSOFT_PIN_MAX) return;
  entry_[length] = digit;
  entry_[length + 1] = 0;
}

void WsoftPinPad::del() {
  size_t length = strlen(entry_);
  if (length > 0) entry_[length - 1] = 0;
}

void WsoftPinPad::clear() {
  memset(entry_, 0, sizeof entry_);
}

const char *WsoftPinPad::masked() {
  size_t length = strlen(entry_);
  memset(masked_, '*', length);
  masked_[length] = 0;
  return masked_;
}
