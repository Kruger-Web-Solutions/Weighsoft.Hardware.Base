#pragma once

// Hardware.Base only (WSOFT_ON_BASE): the RS485 scale setup kept on the board (NVS namespace
// "scale"), the card's e / decimals / unit as last read, and the setup audit counter.
// The rules are wsoft_scale_setup (wsoft_core, host-tested). RS232: nothing here applies,
// the indicator owns its setup.
//
// Tasks: the web task saves the setup; the screen task reads the card and counts card writes.
// scale_lock() is a mutex held across "check, then write" so a setup save and an Approval
// switch can never cross. The web task waits at most SCALE_LOCK_WEB_MS for it (then BUSY),
// so it never blocks the web server for long; the screen task waits until it is free.

#include <Arduino.h>

#include <wsoft_scale_setup.h>

// Screen task, at boot. rs485 = the weight comes from our weighing card.
void scale_begin(bool rs485);
bool scale_rs485();

static const uint32_t SCALE_LOCK_WEB_MS = 500;
static const uint32_t SCALE_LOCK_FOREVER = 0xFFFFFFFFu;
// False when the lock was not free within timeout_ms (then do not call scale_unlock).
bool scale_lock(uint32_t timeout_ms);
void scale_unlock();

bool scale_setup_complete();

// Screen task: the values just read from the card.
void scale_set_card(int32_t division, int decimal, int unit);
// Screen task: a card register write went through (setup audit + 1).
void scale_card_written();

struct ScaleInfo {
  bool rs485;
  WsoftScaleSetup setup;
  WsoftCardFormat card;
  uint32_t audit;
};
void scale_info(ScaleInfo *out);

// Screen task, once per reading: updates the stable-samples count and says why this
// reading is held back (blank, below Min, not stable yet). Always NONE on RS232.
// net = the weight less the tare (scale_net), so below Min looks at the load.
WsoftScaleHold scale_hold(bool linked, bool fresh, int32_t value, int32_t net, int decimals, int unit);

// Runtime tare (RS485 only; never stored, cleared at a restart or a setup save).
// Screen task: TARE takes the stable gross, or clears a tare that is on.
WsoftTareError scale_tare_toggle(bool stable, int32_t value, int decimals, int unit);
// Screen task: the preset tare from the setup.
WsoftTareError scale_tare_preset(int decimals, int unit);
// Any task: the tare now. Cleared by the screen task when the reading's format changes.
WsoftTare scale_tare();
// Screen task: net for this reading; clears a tare whose format no longer matches.
// Returns true when a tare is on.
bool scale_net(int32_t gross, int decimals, int unit, int32_t *net);

// Web task (admin): checks and saves an A5 body. LOCKED while Approval is ON or on RS232;
// BUSY when the setup lock is not free within SCALE_LOCK_WEB_MS.
// who is the signed-in name, for the USB log line.
WsoftSetupError scale_save(const WsoftScaleSetupInput &in, const char *who);
