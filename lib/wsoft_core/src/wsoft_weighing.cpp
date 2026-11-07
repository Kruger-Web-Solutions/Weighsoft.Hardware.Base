#include "wsoft_weighing.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

bool wsoft_weight_kg_text(int32_t value, int decimals, int unit, char *out, size_t size) {
  if (out == nullptr || size == 0 || decimals < 0 || decimals > 9) return false;
  // kg = value / 10^decimals, shifted for t (x1000) and g (/1000).
  int dec = decimals;
  int64_t mantissa = value;
  if (unit == 2) dec -= 3;
  else if (unit == 3) dec += 3;
  else if (unit != 1) return false;
  while (dec < 0) {
    mantissa *= 10;
    dec++;
  }
  // Drop trailing zero decimals, then refuse more precision than the Hub keeps.
  while (dec > 0 && mantissa % 10 == 0) {
    mantissa /= 10;
    dec--;
  }
  if (dec > 5) return false;
  bool negative = mantissa < 0;
  uint64_t magnitude = negative ? (uint64_t)(-mantissa) : (uint64_t)mantissa;
  uint64_t scale = 1;
  for (int i = 0; i < dec; i++) scale *= 10;
  if (magnitude / scale > 99999) return false;
  int n;
  if (dec == 0) {
    n = snprintf(out, size, "%s%llu", negative ? "-" : "", (unsigned long long)magnitude);
  } else {
    n = snprintf(out, size, "%s%llu.%0*llu", negative ? "-" : "", (unsigned long long)(magnitude / scale), dec,
                 (unsigned long long)(magnitude % scale));
  }
  return n > 0 && (size_t)n < size;
}

static bool safe_text(const char *s) {
  if (s == nullptr) return false;
  for (; *s; s++) {
    if (*s == '"' || *s == '\\' || (unsigned char)*s < 0x20) return false;
  }
  return true;
}

bool wsoft_weighing_json(const TradeWeight &w, const WsoftWeighingMeta &meta, char *out, size_t size) {
  if (!w.valid || w.value <= 0 || w.at < TRADE_CLOCK_VALID_AFTER) return false;
  if (!safe_text(meta.device_tag) || !safe_text(meta.source) || !safe_text(meta.station_model)) return false;
  if (meta.device_tag[0] == 0) return false;

  char kg[24], tare[24], net[24];
  if (!wsoft_weight_kg_text(w.value, w.decimals, w.unit, kg, sizeof kg)) return false;
  if (meta.tared) {
    if (meta.tare_value <= 0) return false;
    if (!wsoft_weight_kg_text(meta.tare_value, w.decimals, w.unit, tare, sizeof tare)) return false;
    if (!wsoft_weight_kg_text(w.value - meta.tare_value, w.decimals, w.unit, net, sizeof net)) return false;
  } else {
    strcpy(tare, "0");
    strcpy(net, kg);
  }
  char reading[24];
  format_weight(w.value, w.decimals, reading, sizeof reading);

  char when[32];
  time_t seconds = (time_t)w.at;
  struct tm utc;
  gmtime_r(&seconds, &utc);
  strftime(when, sizeof when, "%Y-%m-%dT%H:%M:%SZ", &utc);

  int n = snprintf(out, size,
                   "{\"weighing_id\":\"%s-%lld-%lu\",\"committed_at\":\"%s\","
                   "\"gross_kg\":%s,\"tare_kg\":%s,\"net_kg\":%s,\"basis\":\"%s\","
                   "\"payload\":{\"seq\":%lu,\"reading\":\"%s\",\"unit\":\"%s\",\"source\":\"%s\","
                   "\"station_model\":\"%s\",\"for_trade\":%s}}",
                   meta.device_tag, (long long)w.at, (unsigned long)w.seq, when, kg, tare, net, meta.tared ? "NT" : "GS",
                   (unsigned long)w.seq, reading,
                   weight_unit_text(w.unit), meta.source, meta.station_model, meta.for_trade ? "true" : "false");
  return n > 0 && (size_t)n < size;
}

WsoftSaveResult WsoftWeighingSaver::update(const TradeReading &r, bool auto_save, bool save_requested,
                                           bool queue_full, TradeWeight *issued, WsoftSaveResult hold) {
  if (issued) *issued = TradeWeight();
  // Stability exactly as TradeEngine: fresh, unchanged readings for TRADE_STABLE_MS.
  if (!started_ || !r.linked || !r.fresh || after_stale_ || r.value != motion_value_) {
    started_ = true;
    motion_value_ = r.value;
    unchanged_since_ = r.now_ms;
  }
  after_stale_ = !r.fresh;
  stable_ = r.linked && r.fresh && !r.alarm && r.now_ms - unchanged_since_ >= TRADE_STABLE_MS;
  if (r.linked && r.fresh && r.value <= 0) seen_empty_ = true;

  WsoftSaveResult blocked = WSOFT_SAVE_NONE;
  if (!r.linked || !r.fresh) blocked = WSOFT_SAVE_NO_SIGNAL;
  else if (r.alarm || !stable_) blocked = WSOFT_SAVE_NOT_STABLE;
  else if (r.value <= 0) blocked = WSOFT_SAVE_EMPTY;
  else if (r.now_unix < TRADE_CLOCK_VALID_AFTER) blocked = WSOFT_SAVE_NO_CLOCK;
  if (hold == WSOFT_SAVE_BLANK && blocked != WSOFT_SAVE_NO_SIGNAL) blocked = hold;
  else if (hold != WSOFT_SAVE_NONE && blocked == WSOFT_SAVE_NONE) blocked = hold;

  bool new_load = !last_.valid || seen_empty_ || r.value != last_.value || r.decimals != last_.decimals ||
                  r.unit != last_.unit;
  if (!save_requested && !(auto_save && new_load)) return WSOFT_SAVE_NONE;
  if (blocked != WSOFT_SAVE_NONE) return save_requested ? blocked : WSOFT_SAVE_NONE;
  if (!new_load) return WSOFT_SAVE_ALREADY;  // only reachable by a tap
  if (queue_full) return WSOFT_SAVE_FULL;

  TradeWeight w;
  w.valid = true;
  w.seq = next_seq_++;
  w.value = r.value;
  w.decimals = r.decimals;
  w.unit = r.unit;
  w.at = r.now_unix;
  last_ = w;
  seen_empty_ = false;
  if (issued) *issued = w;
  return WSOFT_SAVE_SAVED;
}

const char *wsoft_save_result_text(WsoftSaveResult result) {
  switch (result) {
    case WSOFT_SAVE_NONE: return "";
    case WSOFT_SAVE_SAVED: return "Saved";
    case WSOFT_SAVE_ALREADY: return "Already saved";
    case WSOFT_SAVE_NOT_STABLE: return "Wait: moving";
    case WSOFT_SAVE_EMPTY: return "Scale is empty";
    case WSOFT_SAVE_NO_SIGNAL: return "No scale signal";
    case WSOFT_SAVE_NO_CLOCK: return "Clock not set";
    case WSOFT_SAVE_FULL: return "Memory full";
    case WSOFT_SAVE_BLANK: return "Out of range";
    case WSOFT_SAVE_BELOW_MIN: return "Below Min";
  }
  return "";
}
