#pragma once

// Pure trade-weight rules with no Arduino or NVS calls, so they build and run in host unit tests
// (test/test_trade_logic). approval.cpp wraps them with storage, the screen and the Hub.

#include <stddef.h>
#include <stdint.h>

enum TradeCheck {
  TRADE_OK,
  TRADE_OFF,
  TRADE_NO_SIGNAL,
  TRADE_STALE,
  TRADE_ALARM,
  TRADE_MOTION,
  TRADE_EMPTY,
  TRADE_NO_CLOCK,
  TRADE_QUEUE_FULL,
};

struct TradeWeight {
  bool valid = false;
  uint32_t seq = 0;
  int32_t value = 0;
  int decimals = 0;
  int unit = 0;
  int64_t at = 0;
};

// The scale counts as stable when fresh readings have not changed for this long.
static const uint32_t TRADE_STABLE_MS = 1000;
// A clock before this has not been set by NTP yet.
static const int64_t TRADE_CLOCK_VALID_AFTER = 1700000000;

// Trade weights waiting for the Hub to accept them, oldest first. Full means stop issuing, never overwrite.
class TradeQueue {
 public:
  static const size_t CAPACITY = 32;

  size_t size() const { return count_; }
  bool full() const { return count_ == CAPACITY; }
  // Absolute index of the oldest entry; storage slot = index % CAPACITY.
  uint32_t head() const { return head_; }
  const TradeWeight &at(size_t i) const { return items_[(head_ + i) % CAPACITY]; }

  bool push(const TradeWeight &w);
  // Drops every entry with seq <= acked_seq from the front; returns how many.
  size_t ack_through(uint32_t acked_seq);
  // Rebuilds the queue from storage.
  void restore(uint32_t head, size_t count, const TradeWeight *slots);

 private:
  TradeWeight items_[CAPACITY];
  uint32_t head_ = 0;
  size_t count_ = 0;
};

struct TradeReading {
  bool on;
  bool linked;
  bool fresh;  // false when this poll failed and the reading was carried over from an earlier one
  bool alarm;
  int32_t value;
  int decimals;
  int unit;
  uint32_t now_ms;
  int64_t now_unix;
};

class TradeEngine {
 public:
  explicit TradeEngine(uint32_t next_seq = 1) : next_seq_(next_seq) {}

  // Decides whether this reading is a trade weight. When it is a new one, it is written to *issued
  // and next_seq() moves on; the caller must queue it.
  TradeCheck update(const TradeReading &r, bool queue_full, TradeWeight *issued);

  bool stable() const { return stable_; }
  bool current() const { return current_; }
  uint32_t next_seq() const { return next_seq_; }
  const TradeWeight &last() const { return last_; }
  // After an approval switch: forget the last trade weight so the next load gets a fresh one.
  void reset();

 private:
  uint32_t next_seq_;
  bool started_ = false;
  bool after_stale_ = false;
  int32_t motion_value_ = 0;
  uint32_t unchanged_since_ = 0;
  bool stable_ = false;
  bool current_ = false;
  bool seen_empty_ = true;
  TradeWeight last_;
};

const char *trade_check_text(TradeCheck check);

// "1.520" from 1520 with 3 decimals; DJB unit codes 1 kg, 2 t, 3 g, anything else blank.
void format_weight(int32_t value, int decimals, char *out, size_t size);
const char *weight_unit_text(int unit);
