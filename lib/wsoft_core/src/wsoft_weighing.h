#pragma once

// Turns one settled load into the JSON body the Hub takes at POST /api/v1/ingest/weighings
// (server/src/ingest/weighings.ts in Weighsoft.Indicator.Hub). Pure C++ for host tests.
//
// A weighing is saved when a person taps Save, or by itself when Auto save is on, and only
// once the weight has settled above zero for TRADE_STABLE_MS (same "stable" as the trade
// checks). One weighing per load: after a save (and after boot) the scale must show 0 (a live
// reading of 0 or less) before the next weighing, by tap or by auto save (Rhyno, 8 Nov 2026).
// Weighings are saved whether trade checks are on or not; the payload says which.
// With a tare on (RS485 setup), gross, tare and net go out with basis NT; else GS.

#include <stddef.h>
#include <stdint.h>

#include "trade_logic.h"
#include "wsoft_hilo.h"

// "1520", "1.52", "0.0152": the weight in kg as a JSON number the Hub accepts
// (at most 5 digits each side of the point). Units: 1 kg, 2 t, 3 g.
// False for an unknown unit or a value outside the Hub's range.
bool wsoft_weight_kg_text(int32_t value, int decimals, int unit, char *out, size_t size);

struct WsoftWeighingMeta {
  const char *device_tag;     // e.g. the MAC without colons; part of weighing_id
  const char *source;         // "rs232" or "rs485"
  const char *station_model;  // e.g. "wsoft-cyd28"
  bool for_trade;             // trade checks were on when this load settled
  // A tare (button or preset) was on: tare_value is in the weighing's decimals and unit and
  // must be above 0. Then gross, tare and net (gross - tare) go out with basis "NT".
  bool tared;
  int32_t tare_value;
  // The Hi-Lo result for this load (Hub ADR 0008), or nullptr. When on, the weighing gets a
  // top-level "hi_lo" block judged on the net weight.
  const WsoftHiloStamp *hilo;
};

// weighing_id = "<device_tag>-<unix time>-<seq>", unique even if the counter is reset.
// False when the weight cannot be sent (see wsoft_weight_kg_text), the time is unset, or
// out is too small.
bool wsoft_weighing_json(const TradeWeight &w, const WsoftWeighingMeta &meta, char *out, size_t size);

// How long a first Save tap outside the Hi-Lo limits waits for the second (override) tap.
static const uint32_t WSOFT_OVERRIDE_MS = 5000;

enum WsoftSaveResult {
  WSOFT_SAVE_NONE,        // nothing asked for, or auto save waiting for a settled new load
  WSOFT_SAVE_SAVED,       // *issued holds the new weighing
  WSOFT_SAVE_ALREADY,     // Save tapped, but this load is already saved
  WSOFT_SAVE_NEED_ZERO,   // Save tapped, but the scale has not shown 0 since the last save or boot
  WSOFT_SAVE_NOT_STABLE,  // Save tapped while the weight is moving (or the scale alarms)
  WSOFT_SAVE_EMPTY,       // Save tapped with nothing on the scale
  WSOFT_SAVE_NO_SIGNAL,   // Save tapped with no live reading
  WSOFT_SAVE_NO_CLOCK,    // Save tapped before the clock was set (the Hub needs a time)
  WSOFT_SAVE_FULL,        // the board already holds the maximum unsent weighings
  WSOFT_SAVE_BLANK,       // the scale shows ---- (outside the RS485 setup's blanking points)
  WSOFT_SAVE_BELOW_MIN,   // below the RS485 setup's Min while Approval is ON
  // Save tapped while a Hi-Lo product is picked and the net weight is outside its limits. The
  // tap arms an override: a second tap within WSOFT_OVERRIDE_MS on the same weight saves it,
  // marked as an operator override (Rhyno, 8 Nov 2026). Auto save never saves such a weight.
  WSOFT_SAVE_OUT_OF_LIMITS,
};

class WsoftWeighingSaver {
 public:
  explicit WsoftWeighingSaver(uint32_t next_seq = 1) : next_seq_(next_seq) {}

  // Call once per reading. r.on is ignored. save_requested = a person tapped Save since the
  // last call. When the result is SAVED, *issued holds the weighing and next_seq() moved on.
  // hold = why the RS485 scale setup holds this reading back (BLANK, BELOW_MIN or NOT_STABLE),
  // or NONE. BLANK wins over everything but NO_SIGNAL; the others only when nothing else blocks.
  // hilo = the Hi-Lo check of this reading's net weight (NONE when no product is picked).
  // LOW or HIGH: auto save waits (the load may still be topped up or taken off), and a tap gives
  // OUT_OF_LIMITS the first time and saves on a second tap (see WSOFT_SAVE_OUT_OF_LIMITS).
  WsoftSaveResult update(const TradeReading &r, bool auto_save, bool save_requested, bool queue_full,
                         TradeWeight *issued, WsoftSaveResult hold = WSOFT_SAVE_NONE,
                         WsoftHiloState hilo = WSOFT_HILO_NONE);

  bool stable() const { return stable_; }
  uint32_t next_seq() const { return next_seq_; }
  const TradeWeight &last() const { return last_; }
  // The last SAVED weighing was outside the Hi-Lo limits and saved by the operator's second tap.
  bool last_overridden() const { return last_overridden_; }

 private:
  uint32_t next_seq_;
  bool started_ = false;
  bool after_stale_ = false;
  int32_t motion_value_ = 0;
  uint32_t unchanged_since_ = 0;
  bool stable_ = false;
  bool seen_empty_ = false;  // a live 0 seen since the last save; false at boot
  bool armed_ = false;        // a first tap outside the limits is waiting for the second
  uint32_t armed_ms_ = 0;
  int32_t armed_value_ = 0;
  bool last_overridden_ = false;
  TradeWeight last_;
};

// Short words for the screen and the web page (at most 16 chars: they fit the SAVE button).
const char *wsoft_save_result_text(WsoftSaveResult result);
