#include "save_logic.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

SaveDecision SaveEngine::update(const SaveInput &in) {
  // Same stability rule as TradeEngine: only fresh, unchanged readings build up time. A failed poll
  // restarts the clock at the next fresh reading, so a carried-over value never counts.
  bool same = in.value == motion_value_ && in.decimals == motion_decimals_ && in.unit == motion_unit_;
  if (!started_ || !in.linked || !in.fresh || after_stale_ || !same) {
    started_ = true;
    motion_value_ = in.value;
    motion_decimals_ = in.decimals;
    motion_unit_ = in.unit;
    unchanged_since_ = in.now_ms;
  }
  after_stale_ = !in.fresh;
  uint32_t held_ms = in.now_ms - unchanged_since_;
  stable_ = in.linked && in.fresh && !in.alarm && held_ms >= SAVE_MANUAL_STABLE_MS;
  // Back to empty re-arms the next save.
  if (in.linked && in.fresh && in.value <= 0) saved_this_load_ = false;

  char kg[24];
  SaveDecision d;
  d.save = false;
  d.automatic = false;
  if (!in.linked) d.check = SAVE_NO_SIGNAL;
  else if (!in.fresh) d.check = SAVE_STALE;
  else if (in.alarm) d.check = SAVE_ALARM;
  else if (!stable_) d.check = SAVE_MOTION;
  else if (in.value <= 0) d.check = SAVE_EMPTY;
  else if (saved_this_load_) d.check = SAVE_DONE;
  else if (!save_kg_text(in.value, in.decimals, in.unit, kg, sizeof kg)) d.check = SAVE_UNIT;
  else if (in.now_unix < SAVE_CLOCK_VALID_AFTER) d.check = SAVE_NO_CLOCK;
  else if (in.queue_full) d.check = SAVE_QUEUE_FULL;
  else d.check = SAVE_OK;

  if (d.check != SAVE_OK) return d;

  uint32_t auto_ms = in.auto_stable_ms < SAVE_MANUAL_STABLE_MS ? SAVE_MANUAL_STABLE_MS : in.auto_stable_ms;
  if (in.manual) {
    d.save = true;
  } else if (in.auto_on && held_ms >= auto_ms) {
    d.save = true;
    d.automatic = true;
  }
  if (d.save) saved_this_load_ = true;
  return d;
}

const char *save_check_text(SaveCheck c) {
  switch (c) {
    case SAVE_OK: return "ok";
    case SAVE_NO_SIGNAL: return "no_signal";
    case SAVE_STALE: return "stale";
    case SAVE_ALARM: return "alarm";
    case SAVE_MOTION: return "moving";
    case SAVE_EMPTY: return "empty";
    case SAVE_DONE: return "saved";
    case SAVE_UNIT: return "unit";
    case SAVE_NO_CLOCK: return "no_clock";
    case SAVE_QUEUE_FULL: return "queue_full";
  }
  return "unknown";
}

// ---------- queue ----------

bool SaveQueue::push(const SavedWeighing &w) {
  if (full()) return false;
  items_[(head_ + count_) % CAPACITY] = w;
  count_++;
  return true;
}

size_t SaveQueue::ack_through(uint32_t acked_seq) {
  size_t dropped = 0;
  while (count_ > 0 && items_[head_ % CAPACITY].seq <= acked_seq) {
    head_++;
    count_--;
    dropped++;
  }
  return dropped;
}

void SaveQueue::restore(uint32_t head, size_t count, const SavedWeighing *slots) {
  head_ = head;
  count_ = count > CAPACITY ? CAPACITY : count;
  for (size_t i = 0; i < CAPACITY; i++) items_[i] = slots[i];
}

// ---------- Hub body ----------

static const int SAVE_MAX_DJB_DECIMALS = 6;
static const int64_t SAVE_KG_MAX_WHOLE = 99999;

static int64_t pow10_i64(int exponent) {
  int64_t result = 1;
  for (int i = 0; i < exponent; i++) result *= 10;
  return result;
}

bool save_kg_text(int32_t value, int decimals, int unit, char *out, size_t size) {
  if (out == nullptr || size == 0) return false;
  out[0] = '\0';
  if (value < 0 || decimals < 0 || decimals > SAVE_MAX_DJB_DECIMALS) return false;
  // kg = digits * 10^-exponent
  int64_t digits = value;
  int exponent;
  switch (unit) {
    case 1: exponent = decimals; break;      // kg
    case 2: exponent = decimals - 3; break;  // t
    case 3: exponent = decimals + 3; break;  // g
    default: return false;
  }
  if (exponent < 0) {
    digits *= pow10_i64(-exponent);
    exponent = 0;
  }
  while (exponent > 0 && digits % 10 == 0) {
    digits /= 10;
    exponent--;
  }
  if (exponent > SAVE_KG_MAX_DECIMALS) return false;
  int64_t scale = pow10_i64(exponent);
  int64_t whole = digits / scale;
  if (whole > SAVE_KG_MAX_WHOLE) return false;
  // Both parts fit in a long (whole <= 99999, fraction < 10^5), so no %lld (nano printf safe).
  int written;
  if (exponent == 0) {
    written = snprintf(out, size, "%ld", (long)whole);
  } else {
    written = snprintf(out, size, "%ld.%0*ld", (long)whole, exponent, (long)(digits % scale));
  }
  if (written < 0 || (size_t)written >= size) {
    out[0] = '\0';
    return false;
  }
  return true;
}

bool save_time_text(int64_t at, char *out, size_t size) {
  if (out == nullptr || size == 0) return false;
  out[0] = '\0';
  if (at < SAVE_CLOCK_VALID_AFTER) return false;
  time_t seconds = (time_t)at;
  struct tm utc;
  if (gmtime_r(&seconds, &utc) == nullptr) return false;
  return strftime(out, size, "%Y-%m-%dT%H:%M:%SZ", &utc) > 0;
}

void save_weighing_id(const char *id_prefix, uint32_t seq, char *out, size_t size) {
  snprintf(out, size, "%s-%lu", id_prefix ? id_prefix : "wsoft", (unsigned long)seq);
}

size_t save_weighing_body(const SavedWeighing &w, const char *id_prefix, char *out, size_t size) {
  if (out == nullptr || size == 0) return 0;
  out[0] = '\0';
  char kg[24], when[32], id[96];
  if (!w.valid || !save_kg_text(w.value, w.decimals, w.unit, kg, sizeof kg)) return 0;
  if (!save_time_text(w.at, when, sizeof when)) return 0;
  save_weighing_id(id_prefix, w.seq, id, sizeof id);
  int written = snprintf(out, size,
                         "{\"weighing_id\":\"%s\",\"committed_at\":\"%s\",\"gross_kg\":%s,\"tare_kg\":0,"
                         "\"net_kg\":%s,\"basis\":\"GS\",\"payload\":{\"source\":\"wsoft\",\"seq\":%lu,"
                         "\"value\":%ld,\"decimals\":%d,\"unit\":%d,\"trade\":%s,\"auto_saved\":%s}}",
                         id, when, kg, kg, (unsigned long)w.seq, (long)w.value, w.decimals, w.unit,
                         w.trade ? "true" : "false", w.auto_saved ? "true" : "false");
  if (written < 0 || (size_t)written >= size) {
    out[0] = '\0';
    return 0;
  }
  return (size_t)written;
}

// ---------- Hub answers ----------

SaveSendOutcome save_send_outcome(int http_status) {
  if (http_status == 200 || http_status == 201) return SAVE_SEND_DELIVERED;
  // <= 0: no connection or timeout. 409: nonce replayed (a new nonce fixes it). 408/429/5xx: busy.
  if (http_status <= 0 || http_status == 408 || http_status == 409 || http_status == 429 || http_status >= 500) {
    return SAVE_SEND_RETRY;
  }
  return SAVE_SEND_REJECTED;
}

static const uint32_t SAVE_PACE_MS = 1000;  // the Hub allows 120 requests a minute per IP
static const uint32_t SAVE_RETRY_FIRST_MS = 5000;
static const uint32_t SAVE_RETRY_MAX_MS = 60000;

uint32_t save_retry_ms(SaveSendOutcome outcome, uint32_t failures_in_a_row) {
  if (outcome == SAVE_SEND_DELIVERED) return SAVE_PACE_MS;
  if (outcome == SAVE_SEND_REJECTED) return SAVE_RETRY_MAX_MS;
  uint32_t wait = SAVE_RETRY_FIRST_MS;
  for (uint32_t i = 1; i < failures_in_a_row && wait < SAVE_RETRY_MAX_MS; i++) wait *= 2;
  return wait > SAVE_RETRY_MAX_MS ? SAVE_RETRY_MAX_MS : wait;
}
