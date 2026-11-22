#pragma once

// The screen PIN that guards the Approval switch on the board's own screen (design D2).
// An admin sets it in the browser; only a salted SHA-256 is kept (random 16-byte salt).
// The PIN itself is never stored, shown, logged or sent anywhere.
// 5 wrong tries lock the pad for 300 s. Pure C++ for host tests (test/test_wsoft_pin).

#include <stddef.h>
#include <stdint.h>

static const size_t WSOFT_PIN_MIN = 4;
static const size_t WSOFT_PIN_MAX = 6;
static const size_t WSOFT_PIN_SALT_SIZE = 16;
static const uint32_t WSOFT_PIN_TRIES = 5;
static const uint32_t WSOFT_PIN_LOCK_MS = 300000;

enum WsoftPinResult { WSOFT_PIN_OK = 0, WSOFT_PIN_WRONG, WSOFT_PIN_LOCKED, WSOFT_PIN_NOT_SET };

// 4 to 6 digits, nothing else.
bool wsoft_pin_format_ok(const char *pin);
// sha256(salt || pin).
void wsoft_pin_hash(const uint8_t salt[WSOFT_PIN_SALT_SIZE], const char *pin, uint8_t out[32]);
// Constant-time compare against the stored hash. False for a bad format.
bool wsoft_pin_matches(const uint8_t salt[WSOFT_PIN_SALT_SIZE], const uint8_t hash[32], const char *pin);
// "PIN_WRONG", "PIN_LOCKED", "PIN_NOT_SET" ("" for OK).
const char *wsoft_pin_result_code(WsoftPinResult result);

// Counts wrong tries. The board keeps fails() in flash, so a restart does not reset the lock.
class WsoftPinGuard {
 public:
  // fails = the count kept in flash; WSOFT_PIN_TRIES or more locks from now_ms.
  explicit WsoftPinGuard(uint32_t fails = 0, uint32_t now_ms = 0);

  // Seconds left on the lock (rounded up), 0 when not locked.
  uint32_t locked_s(uint32_t now_ms) const;
  int tries_left() const { return (int)(WSOFT_PIN_TRIES - fails_); }
  uint32_t fails() const { return fails_; }

  void wrong(uint32_t now_ms);
  void right();

 private:
  uint32_t fails_ = 0;
  bool locked_ = false;
  uint32_t lock_from_ms_ = 0;
};

// One try on the pad. A wrong or badly formed PIN counts; the 5th wrong one returns LOCKED.
// While locked, nothing is checked and nothing counts.
WsoftPinResult wsoft_pin_try(WsoftPinGuard *guard, bool pin_set, const uint8_t salt[WSOFT_PIN_SALT_SIZE],
                             const uint8_t hash[32], const char *entry, uint32_t now_ms);

// The digits typed on the screen's number pad (at most 6).
class WsoftPinPad {
 public:
  void press(char digit);
  void del();
  void clear();
  const char *entry() const { return entry_; }
  // One '*' per digit typed, for the screen.
  const char *masked();

 private:
  char entry_[WSOFT_PIN_MAX + 1] = "";
  char masked_[WSOFT_PIN_MAX + 1] = "";
};
