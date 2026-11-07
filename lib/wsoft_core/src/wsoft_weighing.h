#pragma once

// Turns one settled load into the JSON body the Hub takes at POST /api/v1/ingest/weighings
// (server/src/ingest/weighings.ts in Weighsoft.Indicator.Hub). Pure C++ for host tests.
//
// A weighing is saved when a person taps Save, or by itself when Auto save is on, and only
// once the weight has settled above zero for TRADE_STABLE_MS (same "stable" as the trade
// checks). One weighing per load: a load is new after the scale emptied or the weight changed.
// Weighings are saved whether trade checks are on or not; the payload says which.

#include <stddef.h>
#include <stdint.h>

#include "trade_logic.h"

// "1520", "1.52", "0.0152": the weight in kg as a JSON number the Hub accepts
// (at most 5 digits each side of the point). Units: 1 kg, 2 t, 3 g.
// False for an unknown unit or a value outside the Hub's range.
bool wsoft_weight_kg_text(int32_t value, int decimals, int unit, char *out, size_t size);

struct WsoftWeighingMeta {
  const char *device_tag;     // e.g. the MAC without colons; part of weighing_id
  const char *source;         // "rs232" or "rs485"
  const char *station_model;  // e.g. "wsoft-cyd28"
  bool for_trade;             // trade checks were on when this load settled
};

// weighing_id = "<device_tag>-<unix time>-<seq>", unique even if the counter is reset.
// False when the weight cannot be sent (see wsoft_weight_kg_text), the time is unset, or
// out is too small.
bool wsoft_weighing_json(const TradeWeight &w, const WsoftWeighingMeta &meta, char *out, size_t size);

enum WsoftSaveResult {
  WSOFT_SAVE_NONE,        // nothing asked for, or auto save waiting for a settled new load
  WSOFT_SAVE_SAVED,       // *issued holds the new weighing
  WSOFT_SAVE_ALREADY,     // Save tapped, but this load is already saved
  WSOFT_SAVE_NOT_STABLE,  // Save tapped while the weight is moving (or the scale alarms)
  WSOFT_SAVE_EMPTY,       // Save tapped with nothing on the scale
  WSOFT_SAVE_NO_SIGNAL,   // Save tapped with no live reading
  WSOFT_SAVE_NO_CLOCK,    // Save tapped before the clock was set (the Hub needs a time)
  WSOFT_SAVE_FULL,        // the board already holds the maximum unsent weighings
};

class WsoftWeighingSaver {
 public:
  explicit WsoftWeighingSaver(uint32_t next_seq = 1) : next_seq_(next_seq) {}

  // Call once per reading. r.on is ignored. save_requested = a person tapped Save since the
  // last call. When the result is SAVED, *issued holds the weighing and next_seq() moved on.
  WsoftSaveResult update(const TradeReading &r, bool auto_save, bool save_requested, bool queue_full,
                         TradeWeight *issued);

  bool stable() const { return stable_; }
  uint32_t next_seq() const { return next_seq_; }
  const TradeWeight &last() const { return last_; }

 private:
  uint32_t next_seq_;
  bool started_ = false;
  bool after_stale_ = false;
  int32_t motion_value_ = 0;
  uint32_t unchanged_since_ = 0;
  bool stable_ = false;
  bool seen_empty_ = true;
  TradeWeight last_;
};

// Short words for the screen and the web page (at most 16 chars: they fit the SAVE button).
const char *wsoft_save_result_text(WsoftSaveResult result);
