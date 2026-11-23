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

#include <wsoft_rs485_tuning.h>
#include <wsoft_scale_setup.h>

// Screen task, at boot. rs485 = the setup is kept on this board: the weight comes from our
// weighing card, or from a load cell this board reads itself (HX711, 22 Nov 2026). The name
// is from before the load cell; false only for an RS232 scale and a Writer.
// card = the weight comes from the RS485 weighing card (not a load cell): the RS485 tuning, the
// board's ZERO and auto zero apply only then.
void scale_begin(bool rs485, bool card = false);
bool scale_rs485();
bool scale_rs485_card();

static const uint32_t SCALE_LOCK_WEB_MS = 500;
static const uint32_t SCALE_LOCK_FOREVER = 0xFFFFFFFFu;
// False when the lock was not free within timeout_ms (then do not call scale_unlock).
bool scale_lock(uint32_t timeout_ms);
void scale_unlock();

bool scale_setup_complete();

// Screen task: the values just read from the card. With a load cell the board is its own
// "card": the division, decimals and unit of its calibration (also set by the web task right
// after a setup change, so the next save is checked against the new e).
void scale_set_card(int32_t division, int decimal, int unit);
// Screen task: a card register write went through (setup audit + 1).
void scale_card_written();

// Screen task: the card answered but has no format registers (division 0: a DJB-W1X has no
// decimals, e or unit). The card becomes the format kept on this board, when one is saved;
// returns false (and leaves the card unread) when none is. kept gets the board's format.
bool scale_card_no_format(WsoftCardFormat *kept);
// Screen task: the card answered with its own format (an M1X).
void scale_card_has_format();
// Web task (admin): saves the decimals, e (in the card's steps) and unit on this board, for a card
// with no format registers. NUMBER when not valid (wsoft_board_format_valid), LOCKED with
// Approval ON or on RS232, BUSY when the setup lock is not free. A change adds 1 to the audit.
WsoftSetupError scale_save_board_format(int decimal, int32_t division, int unit, const char *who);

struct ScaleInfo {
  bool rs485;
  WsoftScaleSetup setup;
  WsoftCardFormat card;
  // The card has no format registers: card holds the format kept on this board (if any).
  bool card_no_format;
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

// ---------- RS485 card tuning and the board's ZERO (wsoft_rs485_tuning.h, 23 Nov 2026) ----------

// The stable range, stable time, auto zero range and time typed on the Scale setup, kept on this
// board (NVS "scale": srng / stim / ztr / ztt) whether or not the card also keeps them.
WsoftRs485Tuning scale_tuning();
// Web task (admin): checks and saves the tuning. LOCKED with Approval ON or when the weight is
// not from the RS485 card, NUMBER when out of range, BUSY when the setup lock is not free. A
// change adds 1 to the setup audit and clears the auto zero.
WsoftSetupError scale_save_tuning(const WsoftRs485Tuning &tuning, const char *who);
// Any task: the stable rule for the trade engine and the weighing saver. RS485 card: the stable
// range (digits) and time (ms) of the tuning. Other sources: 0 and 0 (the old rule).
void scale_stability(int32_t *range, uint32_t *ms);

// Screen task, RS485 card only: the weight to show for this card reading. Takes off the board's
// ZERO and, on a card with no zero tracking of its own (DJB-W1X), the board's auto zero; on a
// card with no format registers it rounds to the e kept on this board. Keeps raw for ZERO.
int32_t scale_card_shown(bool linked, bool fresh, int32_t raw, int decimals, int unit);
// Screen task, RS485 card: ZERO (the screen button, or the web page through the screen task).
// The card's weight now becomes the board's zero (kept over a restart). Nothing is written to
// the card. who goes to the USB log.
WsoftZeroError scale_card_zero(bool linked, bool stable, const char *who);
struct ScaleZeroInfo {
  bool card;           // the weight is from the RS485 card
  WsoftBoardZero zero; // the board's ZERO
  int32_t track;       // what auto zero has taken off since (digits)
  bool board_tracks;   // auto zero runs on the board (a card with no zero tracking of its own)
  int32_t raw;         // the card's last weight before the board's zero
};
void scale_zero_info(ScaleZeroInfo *out);
