#pragma once

// Pure rules for keeping a weighing on the W-Soft (auto-save, touch SAVE, web Save), with no Arduino
// or NVS calls, so they build and run in host tests (test/test_save_logic). save_queue.cpp in
// lib/wsoft_esp wraps them with flash storage, the screen and the Hub.
//
// - Stability is the same rule as TradeEngine: fresh, unchanged readings (same value, decimals and
//   unit) for long enough. A failed or stale poll is never a weight and restarts the clock.
// - A manual SAVE (screen or web) needs SAVE_MANUAL_STABLE_MS (1 s, the same as TRADE_STABLE_MS):
//   the operator is watching the scale. Auto-save waits for the operator-set time (1..30 s, never
//   shorter than the manual 1 s).
// - Once per load: after a save, the next save waits until the scale has been back to empty
//   (a fresh reading <= 0).
// - The queue never overwrites: full means "save refused", never "drop the oldest".
// Approval and the trade path (trade_logic, approval.cpp) are separate and untouched by this.

#include <stddef.h>
#include <stdint.h>

#include "trade_logic.h"

enum SaveCheck {
  SAVE_OK,
  SAVE_NO_SIGNAL,
  SAVE_STALE,
  SAVE_ALARM,
  SAVE_MOTION,
  SAVE_EMPTY,
  SAVE_DONE,  // this load is already saved: take it off (back to empty) for the next one
  SAVE_UNIT,  // the DJB unit or decimals cannot be sent as kg exactly
  SAVE_NO_CLOCK,
  SAVE_QUEUE_FULL,
};

static const uint32_t SAVE_MANUAL_STABLE_MS = TRADE_STABLE_MS;
// A clock before this has not been set yet (same line as the trade rules).
static const int64_t SAVE_CLOCK_VALID_AFTER = TRADE_CLOCK_VALID_AFTER;

struct SaveInput {
  bool linked;
  bool fresh;  // false when this poll failed and the reading was carried over
  bool alarm;
  int32_t value;
  int decimals;
  int unit;  // DJB unit code: 1 kg, 2 t, 3 g
  uint32_t now_ms;
  int64_t now_unix;
  bool auto_on;
  uint32_t auto_stable_ms;  // clamped to at least SAVE_MANUAL_STABLE_MS
  bool manual;              // SAVE was pressed (screen or web) since the last poll
  bool queue_full;
};

struct SaveDecision {
  SaveCheck check;  // SAVE_OK means a manual SAVE would be accepted now
  bool save;        // keep this reading now (the caller must queue it)
  bool automatic;   // the save came from the auto-save timer, not a button
};

class SaveEngine {
 public:
  SaveDecision update(const SaveInput &in);

  bool stable() const { return stable_; }
  // True after a save until the scale has been back to empty.
  bool saved_this_load() const { return saved_this_load_; }

 private:
  bool started_ = false;
  bool after_stale_ = false;
  int32_t motion_value_ = 0;
  int motion_decimals_ = 0;
  int motion_unit_ = 0;
  uint32_t unchanged_since_ = 0;
  bool stable_ = false;
  bool saved_this_load_ = false;
};

const char *save_check_text(SaveCheck check);

// One kept weighing, as queued in flash and sent to the Hub.
struct SavedWeighing {
  bool valid = false;
  uint32_t seq = 0;
  int32_t value = 0;
  int decimals = 0;
  int unit = 0;
  int64_t at = 0;  // unix seconds (UTC)
  bool trade = false;
  bool auto_saved = false;
  bool djb_alarm = false;  // saved in bench test mode while the DJB reported an alarm
};

// Saved weighings waiting for the Hub, oldest first. Full means refuse new saves, never overwrite.
class SaveQueue {
 public:
  static const size_t CAPACITY = 32;

  size_t size() const { return count_; }
  bool full() const { return count_ == CAPACITY; }
  // Absolute index of the oldest entry; storage slot = index % CAPACITY.
  uint32_t head() const { return head_; }
  const SavedWeighing &at(size_t i) const { return items_[(head_ + i) % CAPACITY]; }

  bool push(const SavedWeighing &w);
  // Drops every entry with seq <= acked_seq from the front; returns how many.
  size_t ack_through(uint32_t acked_seq);
  // Rebuilds the queue from storage.
  void restore(uint32_t head, size_t count, const SavedWeighing *slots);

 private:
  SavedWeighing items_[CAPACITY];
  uint32_t head_ = 0;
  size_t count_ = 0;
};

// Hub limit (Weighsoft.Indicator.Hub server/src/ingest/kg.ts): up to 5 digits before and 5 after
// the decimal point.
static const int SAVE_KG_MAX_DECIMALS = 5;

// Exact decimal kg text for a DJB reading ("12.3", "1500", "0.00125"), trailing zeros removed.
// Returns false when the unit has no kg meaning (0 or unknown), decimals are outside 0..6, the
// value is negative, or the result does not fit the Hub's 5.5 digits. Never rounds.
bool save_kg_text(int32_t value, int decimals, int unit, char *out, size_t size);

// "2026-10-31T12:00:00Z". Returns false before the clock is valid.
bool save_time_text(int64_t at, char *out, size_t size);

// "<id_prefix>-<seq>". The prefix is unique per box and per flash life ("wsoft-<mac>-<series>").
void save_weighing_id(const char *id_prefix, uint32_t seq, char *out, size_t size);

// Body for the Hub's signed POST /api/v1/ingest/weighings. gross_kg = net_kg = the reading,
// tare_kg 0, basis "GS". Returns the length written, or 0 when the weighing cannot be sent
// (no kg meaning, no clock, or the buffer is too small).
size_t save_weighing_body(const SavedWeighing &w, const char *id_prefix, char *out, size_t size);

// What the Hub delivery loop does with an HTTP status for a weighing POST.
enum SaveSendOutcome {
  SAVE_SEND_DELIVERED,  // 200 (duplicate) or 201 (stored): remove it from the queue
  SAVE_SEND_RETRY,      // network, 409 nonce, 429, 5xx: try again soon
  SAVE_SEND_REJECTED,   // other 4xx (bad signature, bad body): keep it, log, wait long
};

SaveSendOutcome save_send_outcome(int http_status);

// Wait before the next send: 1 s after a delivery (pacing), retries back off 5 s, 10 s, 20 s ... up
// to 60 s, and a rejection waits 60 s (it stays queued and is tried again; never dropped).
uint32_t save_retry_ms(SaveSendOutcome outcome, uint32_t failures_in_a_row);
