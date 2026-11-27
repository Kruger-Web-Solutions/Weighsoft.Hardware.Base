#include "trade_logic.h"

#include <stdio.h>

bool TradeQueue::push(const TradeWeight &w) {
  if (full()) return false;
  items_[(head_ + count_) % CAPACITY] = w;
  count_++;
  return true;
}

size_t TradeQueue::ack_through(uint32_t acked_seq) {
  size_t dropped = 0;
  while (count_ > 0 && items_[head_ % CAPACITY].seq <= acked_seq) {
    head_++;
    count_--;
    dropped++;
  }
  return dropped;
}

void TradeQueue::restore(uint32_t head, size_t count, const TradeWeight *slots) {
  head_ = head;
  count_ = count > CAPACITY ? CAPACITY : count;
  for (size_t i = 0; i < CAPACITY; i++) items_[i] = slots[i];
}

void TradeEngine::reset() {
  last_ = TradeWeight();
  current_ = false;
  seen_empty_ = true;
}

TradeCheck TradeEngine::update(const TradeReading &r, bool queue_full, TradeWeight *issued) {
  // Only fresh, unchanged readings build up stability. A failed poll restarts the clock at the next
  // fresh reading, so carried-over values never count toward the stable time.
  const int64_t range = r.stable_range > 0 ? r.stable_range : 0;
  const int64_t moved = (int64_t)r.value - (int64_t)motion_value_;
  if (!started_ || !r.linked || !r.fresh || after_stale_ || moved > range || moved < -range) {
    started_ = true;
    motion_value_ = r.value;
    unchanged_since_ = r.now_ms;
    issued_this_run_ = false;
  }
  after_stale_ = !r.fresh;
  // Still is still: an alarm blocks trade on its own (TRADE_ALARM), it is not motion (14 Nov 2026).
  const uint32_t still_ms = r.stable_ms > 0 ? r.stable_ms : TRADE_STABLE_MS;
  stable_ = r.linked && r.fresh && r.now_ms - unchanged_since_ >= still_ms;
  if (r.linked && r.fresh && r.value <= 0) seen_empty_ = true;

  TradeCheck check;
  if (!r.on) check = TRADE_OFF;
  else if (!r.linked) check = TRADE_NO_SIGNAL;
  else if (!r.fresh) check = TRADE_STALE;
  else if (r.alarm) check = TRADE_ALARM;
  else if (!stable_) check = TRADE_MOTION;
  else if (r.value <= 0) check = TRADE_EMPTY;
  else if (r.now_unix < TRADE_CLOCK_VALID_AFTER) check = TRADE_NO_CLOCK;
  else check = TRADE_OK;

  // A new trade weight once per load: the stable weight differs from the last one, or the scale was emptied.
  const int64_t from_last = (int64_t)r.value - (int64_t)last_.value;
  // Inside one still run (the reading stayed within the range) the load is the one already issued.
  bool new_load = !last_.valid || seen_empty_ ||
                  (!issued_this_run_ && (from_last > range || from_last < -range)) ||
                  r.decimals != last_.decimals || r.unit != last_.unit;
  if (check == TRADE_OK && new_load && queue_full) check = TRADE_QUEUE_FULL;

  if (check == TRADE_OK && new_load) {
    TradeWeight w;
    w.valid = true;
    w.seq = next_seq_++;
    w.value = r.value;
    w.decimals = r.decimals;
    w.unit = r.unit;
    w.at = r.now_unix;
    last_ = w;
    seen_empty_ = false;
    issued_this_run_ = true;
    if (issued) *issued = w;
  } else if (issued) {
    *issued = TradeWeight();
  }
  current_ = check == TRADE_OK;
  return check;
}

const char *trade_check_text(TradeCheck c) {
  switch (c) {
    case TRADE_OK: return "ok";
    case TRADE_OFF: return "approval_off";
    case TRADE_NO_SIGNAL: return "no_signal";
    case TRADE_STALE: return "stale";
    case TRADE_ALARM: return "alarm";
    case TRADE_MOTION: return "motion";
    case TRADE_EMPTY: return "empty";
    case TRADE_NO_CLOCK: return "no_clock";
    case TRADE_QUEUE_FULL: return "queue_full";
  }
  return "unknown";
}

void format_weight(int32_t value, int decimals, char *out, size_t size) {
  bool negative = value < 0;
  uint32_t magnitude = negative ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
  if (decimals <= 0) {
    snprintf(out, size, "%s%lu", negative ? "-" : "", (unsigned long)magnitude);
    return;
  }
  uint32_t scale = 1;
  for (int i = 0; i < decimals; i++) scale *= 10;
  snprintf(out, size, "%s%lu.%0*lu", negative ? "-" : "", (unsigned long)(magnitude / scale), decimals,
           (unsigned long)(magnitude % scale));
}

const char *weight_unit_text(int unit) {
  switch (unit) {
    case 1: return "kg";
    case 2: return "t";
    case 3: return "g";
    default: return "";
  }
}
