#include "wsoft_rs485_tuning.h"

bool operator==(const WsoftRs485Tuning &a, const WsoftRs485Tuning &b) {
  return a.stable_range == b.stable_range && a.stable_time == b.stable_time &&
         a.zero_track_range == b.zero_track_range && a.zero_track_time == b.zero_track_time;
}

bool wsoft_rs485_tuning_valid(const WsoftRs485Tuning &t) {
  if (t.stable_range < 0 || t.stable_range > WSOFT_TUNE_STABLE_RANGE_MAX) return false;
  if (t.stable_time < 0 || t.stable_time > WSOFT_TUNE_TIME_MAX) return false;
  if (t.zero_track_range < 0 || t.zero_track_range > WSOFT_TUNE_ZERO_TRACK_MAX) return false;
  if (t.zero_track_time < WSOFT_TUNE_TIME_MIN || t.zero_track_time > WSOFT_TUNE_TIME_MAX) return false;
  return true;
}

int32_t wsoft_round_to_step(int32_t value, int32_t step) {
  if (step <= 1) return value;
  int64_t v = value;
  int64_t half = step / 2;
  int64_t q = v >= 0 ? (v + half) / step : -((-v + half) / step);
  int64_t r = q * step;
  if (r > INT32_MAX || r < INT32_MIN) return value;
  return (int32_t)r;
}

void WsoftZeroTracker::reset() {
  offset_ = 0;
  inside_ = false;
  since_ms_ = 0;
}

bool WsoftZeroTracker::update(bool linked, bool stable, int32_t shown, int32_t range, uint32_t time_ms,
                              uint32_t now_ms) {
  if (range <= 0 || !linked || !stable || shown == 0 || shown > range || shown < -range) {
    // shown == 0: nothing to take away; keep the clock so a creep starts a fresh wait.
    inside_ = false;
    return false;
  }
  if (!inside_) {
    inside_ = true;
    since_ms_ = now_ms;
    return false;
  }
  if ((uint32_t)(now_ms - since_ms_) < time_ms) return false;
  int64_t next = (int64_t)offset_ + shown;
  int64_t reach = (int64_t)range * WSOFT_ZERO_TRACK_REACH;
  if (next > reach || next < -reach) {
    inside_ = false;  // at the edge of its reach: leave the rest showing
    return false;
  }
  offset_ = (int32_t)next;
  inside_ = false;
  return true;
}

bool wsoft_board_zero_apply(const WsoftBoardZero &zero, int32_t track_offset, int32_t raw, int decimals, int unit,
                            int32_t step, int32_t *shown) {
  int64_t v = raw;
  if (zero.set && zero.decimals == decimals && zero.unit == unit) v -= zero.raw;
  v -= track_offset;
  if (v > INT32_MAX || v < INT32_MIN) {
    *shown = raw;
    return false;
  }
  *shown = wsoft_round_to_step((int32_t)v, step);
  return true;
}

const char *wsoft_zero_error_screen(WsoftZeroError error) {
  switch (error) {
    case WSOFT_ZERO_OK: return "ZERO SET";
    case WSOFT_ZERO_APPROVAL: return "APPROVAL IS ON";
    case WSOFT_ZERO_TARE: return "CLEAR TARE FIRST";
    case WSOFT_ZERO_NO_SIGNAL: return "NO SIGNAL";
    case WSOFT_ZERO_NOT_STEADY: return "NOT STEADY";
    case WSOFT_ZERO_SOURCE: return "NOT ON RS485";
  }
  return "NOT STEADY";
}

const char *wsoft_zero_error_code(WsoftZeroError error) {
  switch (error) {
    case WSOFT_ZERO_OK: return "";
    case WSOFT_ZERO_APPROVAL: return "approval_on";
    case WSOFT_ZERO_TARE: return "tare_on";
    case WSOFT_ZERO_NO_SIGNAL: return "no_signal";
    case WSOFT_ZERO_NOT_STEADY: return "not_steady";
    case WSOFT_ZERO_SOURCE: return "not_rs485";
  }
  return "not_steady";
}

WsoftZeroError wsoft_zero_check(bool rs485_card, bool approval_on, bool tare_on, bool linked, bool stable) {
  if (!rs485_card) return WSOFT_ZERO_SOURCE;
  if (approval_on) return WSOFT_ZERO_APPROVAL;
  if (tare_on) return WSOFT_ZERO_TARE;
  if (!linked) return WSOFT_ZERO_NO_SIGNAL;
  if (!stable) return WSOFT_ZERO_NOT_STEADY;
  return WSOFT_ZERO_OK;
}
