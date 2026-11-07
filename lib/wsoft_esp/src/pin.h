#pragma once

// Hardware.Base only (WSOFT_ON_BASE): the screen PIN for the Approval switch (design D2).
// The rules are wsoft_pin (wsoft_core, host-tested). NVS namespace "pin": a random 16-byte
// salt, sha256(salt || PIN), and the wrong-try count (so a restart does not undo a lock).
// The PIN itself is never stored, shown, logged or sent anywhere.

#include <Arduino.h>

#include <wsoft_pin.h>

// Screen task, at boot.
void pin_begin();
// Web task (admin): sets or changes the PIN. False for a bad format (nothing saved).
bool pin_save(const char *pin);
bool pin_is_set();
// Seconds left on the lock after 5 wrong tries, 0 when not locked.
uint32_t pin_locked_s();
// Wrong tries left before the lock.
int pin_tries_left();
// Screen task: one try on the number pad. Wrong tries are logged without the digits.
WsoftPinResult pin_try(const char *entry);
