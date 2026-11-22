#pragma once

// Hardware.Base only (WSOFT_ON_BASE): the screen PIN for the Approval switch (design D2).
// The rules are wsoft_pin (wsoft_core, host-tested). NVS namespace "pin": a random 16-byte
// salt, sha256(salt || PIN), and the wrong-try count (so a restart does not undo a lock).
// The PIN itself is never stored, shown, logged or sent anywhere.

#include <Arduino.h>

#include <wsoft_pin.h>

// Screen task, at boot.
void pin_begin();
enum PinSaveResult { PIN_SAVE_OK, PIN_SAVE_FORMAT, PIN_SAVE_BUSY };
// Web task (admin): sets or changes the PIN. FORMAT: not 4 to 6 digits. BUSY: a try on the
// screen held the PIN for more than PIN_SAVE_WAIT_MS. Nothing is saved on an error.
static const uint32_t PIN_SAVE_WAIT_MS = 500;
PinSaveResult pin_save(const char *pin);
bool pin_is_set();
// Seconds left on the lock after 5 wrong tries, 0 when not locked.
uint32_t pin_locked_s();
// Wrong tries left before the lock.
int pin_tries_left();
// Screen task: one try on the number pad. Wrong tries are logged without the digits.
WsoftPinResult pin_try(const char *entry);
