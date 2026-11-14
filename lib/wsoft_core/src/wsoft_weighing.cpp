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

  char hilo[400] = "";
  if (meta.hilo != nullptr && meta.hilo->on && !wsoft_hilo_json(*meta.hilo, net, hilo, sizeof hilo)) return false;

  char when[32];
  time_t seconds = (time_t)w.at;
  struct tm utc;
  gmtime_r(&seconds, &utc);
  strftime(when, sizeof when, "%Y-%m-%dT%H:%M:%SZ", &utc);

  int n = snprintf(out, size,
                   "{\"weighing_id\":\"%s-%lld-%lu\",\"committed_at\":\"%s\","
                   "\"gross_kg\":%s,\"tare_kg\":%s,\"net_kg\":%s,\"basis\":\"%s\","
                   "\"payload\":{\"seq\":%lu,\"reading\":\"%s\",\"unit\":\"%s\",\"source\":\"%s\","
                   "\"station_model\":\"%s\",\"for_trade\":%s%s}%s}",
                   meta.device_tag, (long long)w.at, (unsigned long)w.seq, when, kg, tare, net, meta.tared ? "NT" : "GS",
                   (unsigned long)w.seq, reading,
                   weight_unit_text(w.unit), meta.source, meta.station_model, meta.for_trade ? "true" : "false",
                   meta.alarm_test_override ? ",\"alarm_test_override\":true" : "", hilo);
  return n > 0 && (size_t)n < size;
}

WsoftAlarmGate wsoft_alarm_test_gate(bool alarm, bool test_on, bool approval_on) {
  WsoftAlarmGate gate;
  gate.overridden = alarm && test_on && !approval_on;
  gate.alarm = alarm && !gate.overridden;
  return gate;
}

WsoftSaveResult WsoftWeighingSaver::update(const TradeReading &r, bool auto_save, bool save_requested,
                                           bool queue_full, TradeWeight *issued, WsoftSaveResult hold,
                                           WsoftHiloState hilo) {
  if (issued) *issued = TradeWeight();
  // Stability exactly as TradeEngine: fresh, unchanged readings for TRADE_STABLE_MS.
  if (!started_ || !r.linked || !r.fresh || after_stale_ || r.value != motion_value_) {
    started_ = true;
    motion_value_ = r.value;
    unchanged_since_ = r.now_ms;
  }
  after_stale_ = !r.fresh;
  // Still is still: an alarm blocks trade on its own (TRADE_ALARM), it is not motion (14 Nov 2026).
  stable_ = r.linked && r.fresh && r.now_ms - unchanged_since_ >= TRADE_STABLE_MS;
  if (r.linked && r.fresh && r.value <= 0) seen_empty_ = true;

  if (hold == WSOFT_SAVE_WRITER) {
    moving_ = false;
    armed_ = false;
    waiting_ = WSOFT_SAVE_WRITER;
    return save_requested ? WSOFT_SAVE_WRITER : WSOFT_SAVE_NONE;
  }

  WsoftSaveResult blocked = WSOFT_SAVE_NONE;
  if (!r.linked || !r.fresh) blocked = WSOFT_SAVE_NO_SIGNAL;
  else if (r.alarm) blocked = WSOFT_SAVE_ALARM;
  else if (!stable_) blocked = WSOFT_SAVE_NOT_STABLE;
  else if (r.value <= 0) blocked = WSOFT_SAVE_EMPTY;
  else if (r.now_unix < TRADE_CLOCK_VALID_AFTER) blocked = WSOFT_SAVE_NO_CLOCK;
  if (hold == WSOFT_SAVE_BLANK && blocked != WSOFT_SAVE_NO_SIGNAL) blocked = hold;
  else if (hold != WSOFT_SAVE_NONE && blocked == WSOFT_SAVE_NONE) blocked = hold;

  // A new load only after the scale showed 0: a changed weight alone is not a new load.
  bool new_load = seen_empty_;
  bool loaded = r.linked && r.fresh && r.value > 0;
  bool same_as_saved = last_.valid && r.value == last_.value && r.decimals == last_.decimals && r.unit == last_.unit;
  bool outside = hilo == WSOFT_HILO_LOW || hilo == WSOFT_HILO_HIGH;

  // The notice: the real reason, not just the first check that failed. A load that has not
  // been off the scale is the reason even while it still moves (9 Nov 2026: the Edge said
  // "waiting for a steady weight" when the scale had to show 0 first).
  if (blocked == WSOFT_SAVE_NOT_STABLE && loaded) {
    if (!moving_) {
      moving_ = true;
      moving_since_ = r.now_ms;
    }
  } else {
    moving_ = false;
  }
  if (blocked == WSOFT_SAVE_NO_SIGNAL || blocked == WSOFT_SAVE_BLANK || blocked == WSOFT_SAVE_ALARM) waiting_ = blocked;
  else if (r.now_unix < TRADE_CLOCK_VALID_AFTER) waiting_ = WSOFT_SAVE_NO_CLOCK;
  else if (loaded && !new_load) waiting_ = same_as_saved ? WSOFT_SAVE_ALREADY : WSOFT_SAVE_NEED_ZERO;
  else if (!loaded) waiting_ = WSOFT_SAVE_NONE;
  else if (queue_full) waiting_ = WSOFT_SAVE_FULL;
  else if (blocked == WSOFT_SAVE_BELOW_MIN) waiting_ = blocked;
  else if (blocked == WSOFT_SAVE_NONE && outside) waiting_ = WSOFT_SAVE_OUT_OF_LIMITS;
  else if (moving_ && r.now_ms - moving_since_ >= WSOFT_MOVING_NOTICE_MS) waiting_ = WSOFT_SAVE_NOT_STABLE;
  else waiting_ = WSOFT_SAVE_NONE;

  if (!save_requested && !(auto_save && new_load)) return WSOFT_SAVE_NONE;
  // A tap on a load that never left the scale gets that reason, even while it moves.
  if (blocked == WSOFT_SAVE_NOT_STABLE && save_requested && loaded && !new_load) blocked = WSOFT_SAVE_NONE;
  if (blocked != WSOFT_SAVE_NONE) return save_requested ? blocked : WSOFT_SAVE_NONE;
  if (!new_load) {  // only reachable by a tap
    return same_as_saved ? WSOFT_SAVE_ALREADY : WSOFT_SAVE_NEED_ZERO;
  }
  if (queue_full) return WSOFT_SAVE_FULL;

  bool overridden = false;
  if (outside) {
    if (!save_requested) return WSOFT_SAVE_NONE;  // auto save waits for a weight inside the limits
    bool second = armed_ && r.value == armed_value_ && r.now_ms - armed_ms_ <= WSOFT_OVERRIDE_MS;
    if (!second) {
      armed_ = true;
      armed_ms_ = r.now_ms;
      armed_value_ = r.value;
      return WSOFT_SAVE_OUT_OF_LIMITS;
    }
    overridden = true;
  }
  armed_ = false;

  TradeWeight w;
  w.valid = true;
  w.seq = next_seq_++;
  w.value = r.value;
  w.decimals = r.decimals;
  w.unit = r.unit;
  w.at = r.now_unix;
  last_ = w;
  last_overridden_ = overridden;
  seen_empty_ = false;
  waiting_ = WSOFT_SAVE_ALREADY;  // saved: the load still on the scale is this one
  if (issued) *issued = w;
  return WSOFT_SAVE_SAVED;
}

const char *wsoft_save_result_text(WsoftSaveResult result) {
  switch (result) {
    case WSOFT_SAVE_NONE: return "";
    case WSOFT_SAVE_SAVED: return "Saved";
    case WSOFT_SAVE_ALREADY: return "Already saved";
    case WSOFT_SAVE_NEED_ZERO: return "Take load off";
    case WSOFT_SAVE_NOT_STABLE: return "Wait: moving";
    case WSOFT_SAVE_EMPTY: return "Scale is empty";
    case WSOFT_SAVE_NO_SIGNAL: return "No scale signal";
    case WSOFT_SAVE_NO_CLOCK: return "Clock not set";
    case WSOFT_SAVE_FULL: return "Memory full";
    case WSOFT_SAVE_BLANK: return "Out of range";
    case WSOFT_SAVE_BELOW_MIN: return "Below Min";
    case WSOFT_SAVE_OUT_OF_LIMITS: return "Tap again: save";
    case WSOFT_SAVE_WRITER: return "Writer: no save";
    case WSOFT_SAVE_ALARM: return "Scale alarm";
  }
  return "";
}

bool wsoft_save_notice(WsoftSaveResult result, WsoftSaveNotice *out) {
  WsoftSaveNotice n;
  switch (result) {
    case WSOFT_SAVE_NONE:
    case WSOFT_SAVE_SAVED:
    case WSOFT_SAVE_EMPTY:
      return false;
    case WSOFT_SAVE_NEED_ZERO:
      n.code = "NEED_ZERO";
      n.title = "TAKE THE LOAD OFF";
      n.detail = "The scale must show 0 kg first";
      n.level = WSOFT_NOTICE_STOP;
      break;
    case WSOFT_SAVE_ALREADY:
      n.code = "ALREADY";
      n.title = "SAVED";
      n.detail = "Take it off for the next load";
      n.level = WSOFT_NOTICE_INFO;
      break;
    case WSOFT_SAVE_NOT_STABLE:
      n.code = "NOT_STABLE";
      n.title = "WEIGHT IS MOVING";
      n.detail = "Keep the load still";
      n.level = WSOFT_NOTICE_WAIT;
      break;
    case WSOFT_SAVE_NO_SIGNAL:
      n.code = "NO_SIGNAL";
      n.title = "NO SCALE SIGNAL";
      n.detail = "Check the scale cable and power";
      n.level = WSOFT_NOTICE_STOP;
      break;
    case WSOFT_SAVE_NO_CLOCK:
      n.code = "NO_CLOCK";
      n.title = "CLOCK NOT SET";
      n.detail = "Connect Wi-Fi to get the time";
      n.level = WSOFT_NOTICE_STOP;
      break;
    case WSOFT_SAVE_FULL:
      n.code = "FULL";
      n.title = "MEMORY FULL";
      n.detail = "Send saved weighings to the Hub";
      n.level = WSOFT_NOTICE_STOP;
      break;
    case WSOFT_SAVE_BLANK:
      n.code = "BLANK";
      n.title = "OUT OF RANGE";
      n.detail = "Weight is past the scale range";
      n.level = WSOFT_NOTICE_STOP;
      break;
    case WSOFT_SAVE_BELOW_MIN:
      n.code = "BELOW_MIN";
      n.title = "BELOW MIN";
      n.detail = "Too light while Approval is ON";
      n.level = WSOFT_NOTICE_STOP;
      break;
    case WSOFT_SAVE_OUT_OF_LIMITS:
      n.code = "OUT_OF_LIMITS";
      n.title = "OUTSIDE HI-LO LIMITS";
      n.detail = "Tap SAVE twice to save anyway";
      n.level = WSOFT_NOTICE_WAIT;
      break;
    case WSOFT_SAVE_WRITER:
      n.code = "WRITER";
      n.title = "WRITER MODE";
      n.detail = "The weigh board saves weighings";
      n.level = WSOFT_NOTICE_INFO;
      break;
    case WSOFT_SAVE_ALARM:
      n.code = "ALARM";
      n.title = "SCALE ALARM";
      n.detail = "The junction box shows an alarm";
      n.level = WSOFT_NOTICE_STOP;
      break;
  }
  if (n.title[0] == 0) return false;
  if (out) *out = n;
  return true;
}
