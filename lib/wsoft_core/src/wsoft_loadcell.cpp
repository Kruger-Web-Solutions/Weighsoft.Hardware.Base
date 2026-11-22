#include "wsoft_loadcell.h"

#include "wsoft_scale_setup.h"

int32_t wsoft_hx711_decode(uint32_t bits24) {
  bits24 &= 0xFFFFFFu;
  if (bits24 & 0x800000u) return (int32_t)bits24 - 0x1000000;
  return (int32_t)bits24;
}

bool wsoft_hx711_saturated(int32_t raw) {
  return raw >= 0x7FFFFF || raw <= -0x800000;
}

void wsoft_loadcell_defaults(WsoftLoadcellCal *cal) {
  cal->zero_set = false;
  cal->zero_raw = 0;
  cal->span_set = false;
  cal->span_counts = 0;
  cal->span_value = 0;
  cal->decimals = 3;
  cal->unit = 1;
  cal->step = 1;
  cal->filter = WSOFT_LOADCELL_FILTER_MEDIUM;
  cal->zero_track = 0;
  cal->drift_guard = 0;
}

bool wsoft_loadcell_calibrated(const WsoftLoadcellCal &cal) {
  return cal.zero_set && cal.span_set && cal.span_counts != 0 && cal.span_value > 0;
}

bool wsoft_loadcell_step_valid(int step) {
  return step == 1 || step == 2 || step == 5 || step == 10 || step == 20 || step == 50;
}

bool wsoft_loadcell_filter_valid(int filter) {
  return filter >= WSOFT_LOADCELL_FILTER_OFF && filter <= WSOFT_LOADCELL_FILTER_HIGH;
}

size_t wsoft_loadcell_filter_window(int filter) {
  switch (filter) {
    case WSOFT_LOADCELL_FILTER_OFF: return 1;
    case WSOFT_LOADCELL_FILTER_LOW: return 4;
    case WSOFT_LOADCELL_FILTER_HIGH: return WSOFT_LOADCELL_COUNTS;
    default: return 12;
  }
}

WsoftLoadcellError wsoft_loadcell_set_filter(WsoftLoadcellCal *cal, int filter) {
  if (!wsoft_loadcell_filter_valid(filter)) return WSOFT_LOADCELL_BAD_FILTER;
  cal->filter = filter;
  return WSOFT_LOADCELL_OK;
}

WsoftLoadcellError wsoft_loadcell_set_zero_track(WsoftLoadcellCal *cal, int divisions) {
  if (divisions < 0 || divisions > WSOFT_LOADCELL_ZERO_TRACK_MAX) return WSOFT_LOADCELL_BAD_ZERO_TRACK;
  cal->zero_track = divisions;
  return WSOFT_LOADCELL_OK;
}

WsoftLoadcellError wsoft_loadcell_set_drift_guard(WsoftLoadcellCal *cal, int divisions) {
  if (divisions < 0 || divisions > WSOFT_LOADCELL_DRIFT_GUARD_MAX) return WSOFT_LOADCELL_BAD_DRIFT_GUARD;
  cal->drift_guard = divisions;
  return WSOFT_LOADCELL_OK;
}

WsoftLoadcellError wsoft_loadcell_set_zero(WsoftLoadcellCal *cal, bool have_raw, int32_t raw) {
  if (!have_raw || wsoft_hx711_saturated(raw)) return WSOFT_LOADCELL_NO_READING;
  cal->zero_set = true;
  cal->zero_raw = raw;
  return WSOFT_LOADCELL_OK;
}

static bool format_valid(int decimals, int unit) {
  if (unit != 1 && unit != 3) return false;
  return decimals >= 0 && decimals <= (unit == 3 ? 2 : 4);
}

bool wsoft_loadcell_rescale(int32_t value, int from_decimals, int from_unit, int to_decimals, int to_unit,
                            int32_t *out) {
  // Through kg, with the setup's own exact conversions (wsoft_scale_setup).
  int64_t kg = 0;
  int32_t digits = 0;
  if (value <= 0 || !wsoft_reading_to_kg(value, from_decimals, from_unit, &kg) ||
      !wsoft_kg_to_reading(kg, to_decimals, to_unit, &digits) || digits <= 0 || digits > WSOFT_LOADCELL_MAX_VALUE) {
    return false;
  }
  *out = digits;
  return true;
}

WsoftLoadcellError wsoft_loadcell_set_span(WsoftLoadcellCal *cal, bool have_raw, int32_t raw, int32_t value,
                                           int decimals, int unit) {
  if (!have_raw || wsoft_hx711_saturated(raw)) return WSOFT_LOADCELL_NO_READING;
  if (!cal->zero_set) return WSOFT_LOADCELL_NO_ZERO;
  if (value <= 0 || value > WSOFT_LOADCELL_MAX_VALUE) return WSOFT_LOADCELL_BAD_WEIGHT;
  if (decimals < 0 || decimals > 4) return WSOFT_LOADCELL_BAD_DECIMALS;
  if (unit != 1 && unit != 3) return WSOFT_LOADCELL_BAD_UNIT;
  int32_t digits = 0;
  if (!wsoft_loadcell_rescale(value, decimals, unit, cal->decimals, cal->unit, &digits)) {
    return WSOFT_LOADCELL_BAD_DECIMALS;
  }
  int64_t counts = (int64_t)raw - (int64_t)cal->zero_raw;
  int64_t size = counts < 0 ? -counts : counts;
  if (size < WSOFT_LOADCELL_MIN_SPAN_COUNTS) return WSOFT_LOADCELL_TOO_SMALL;
  cal->span_set = true;
  cal->span_counts = (int32_t)counts;
  cal->span_value = digits;
  return WSOFT_LOADCELL_OK;
}

WsoftLoadcellError wsoft_loadcell_set_format(WsoftLoadcellCal *cal, int decimals, int unit, int step) {
  if (unit != 1 && unit != 3) return WSOFT_LOADCELL_BAD_UNIT;
  if (!format_valid(decimals, unit)) return WSOFT_LOADCELL_BAD_DECIMALS;
  if (!wsoft_loadcell_step_valid(step)) return WSOFT_LOADCELL_BAD_STEP;
  int32_t span_value = cal->span_value;
  if (cal->span_set && !wsoft_loadcell_rescale(cal->span_value, cal->decimals, cal->unit, decimals, unit, &span_value)) {
    return WSOFT_LOADCELL_FORMAT_SPAN;
  }
  cal->span_value = span_value;
  cal->decimals = decimals;
  cal->unit = unit;
  cal->step = step;
  return WSOFT_LOADCELL_OK;
}

// a / b rounded half away from zero; b is not 0.
static int64_t divide_round(int64_t a, int64_t b) {
  if (b < 0) {
    a = -a;
    b = -b;
  }
  return a >= 0 ? (a + b / 2) / b : -((-a + b / 2) / b);
}

// The load in sixteenths of a digit (16 = one last digit), before any rounding to the division.
static bool fine_digits(const WsoftLoadcellCal &cal, int32_t raw, int64_t *fine) {
  if (!wsoft_loadcell_calibrated(cal) || wsoft_hx711_saturated(raw)) return false;
  int64_t counts = (int64_t)raw - (int64_t)cal.zero_raw;
  *fine = divide_round(counts * (int64_t)cal.span_value * 16, (int64_t)cal.span_counts);
  return true;
}

static int step_of(const WsoftLoadcellCal &cal) {
  return wsoft_loadcell_step_valid(cal.step) ? cal.step : 1;
}

bool wsoft_loadcell_weight_steady(const WsoftLoadcellCal &cal, int32_t raw, bool have_last, int32_t last,
                                  int32_t *value) {
  int64_t fine = 0;
  if (!fine_digits(cal, raw, &fine)) return false;
  int step = step_of(cal);
  // The shown weight is rounded from the counts themselves, exactly as before the filter came.
  int64_t counts = (int64_t)raw - (int64_t)cal.zero_raw;
  int64_t digits = divide_round(counts * (int64_t)cal.span_value, (int64_t)cal.span_counts);
  digits = divide_round(digits, step) * step;
  if (have_last && cal.filter != WSOFT_LOADCELL_FILTER_OFF && last % step == 0) {
    int64_t away = fine - (int64_t)last * 16;
    if (away < 0) away = -away;
    if (away <= (int64_t)step * 12) digits = last;  // 12/16 = 3/4 of a division
  }
  if (digits > WSOFT_LOADCELL_MAX_VALUE || digits < -WSOFT_LOADCELL_MAX_VALUE) return false;
  *value = (int32_t)digits;
  return true;
}

bool wsoft_loadcell_weight(const WsoftLoadcellCal &cal, int32_t raw, int32_t *value) {
  return wsoft_loadcell_weight_steady(cal, raw, false, 0, value);
}

// Raw counts in one division, times 10 (so a division of 2.5 counts is 25). 0 = not calibrated.
static int64_t division_counts_x10(const WsoftLoadcellCal &cal) {
  if (!wsoft_loadcell_calibrated(cal)) return 0;
  int64_t counts = cal.span_counts < 0 ? -(int64_t)cal.span_counts : (int64_t)cal.span_counts;
  return counts * step_of(cal) * 10 / cal.span_value;
}

int32_t wsoft_loadcell_filter_jump(const WsoftLoadcellCal &cal) {
  if (cal.filter == WSOFT_LOADCELL_FILTER_OFF) return 0;
  int64_t jump = division_counts_x10(cal) * 4 / 10;
  if (jump > 0x3FFFFF) jump = 0x3FFFFF;
  return (int32_t)jump;
}

bool wsoft_loadcell_noise_tenths(const WsoftLoadcellCal &cal, int32_t spread, int32_t *tenths) {
  int64_t division = division_counts_x10(cal);
  if (division <= 0 || spread < 0) return false;
  int64_t value = (int64_t)spread * 100 / division;
  *tenths = value > 99999 ? 99999 : (int32_t)value;
  return true;
}

const char *wsoft_loadcell_error_code(WsoftLoadcellError error) {
  switch (error) {
    case WSOFT_LOADCELL_OK: return "ok";
    case WSOFT_LOADCELL_NO_READING: return "no_reading";
    case WSOFT_LOADCELL_NO_ZERO: return "zero_first";
    case WSOFT_LOADCELL_BAD_WEIGHT: return "bad_weight";
    case WSOFT_LOADCELL_BAD_DECIMALS: return "bad_decimals";
    case WSOFT_LOADCELL_BAD_UNIT: return "bad_unit";
    case WSOFT_LOADCELL_BAD_STEP: return "bad_step";
    case WSOFT_LOADCELL_TOO_SMALL: return "too_small";
    case WSOFT_LOADCELL_FORMAT_SPAN: return "format_span";
    case WSOFT_LOADCELL_BAD_FILTER: return "bad_filter";
    case WSOFT_LOADCELL_BAD_ZERO_TRACK: return "bad_zero_track";
    case WSOFT_LOADCELL_BAD_DRIFT_GUARD: return "bad_drift_guard";
  }
  return "error";
}

const char *wsoft_loadcell_error_text(WsoftLoadcellError error) {
  switch (error) {
    case WSOFT_LOADCELL_OK: return "Done.";
    case WSOFT_LOADCELL_NO_READING: return "No reading from the load cell board. Check the four wires on CN1.";
    case WSOFT_LOADCELL_NO_ZERO: return "Set zero first, with the scale empty.";
    case WSOFT_LOADCELL_BAD_WEIGHT: return "Type the test weight as a number above 0.";
    case WSOFT_LOADCELL_BAD_DECIMALS:
      return "Too many digits after the point for this division size. Type fewer, or pick a finer division.";
    case WSOFT_LOADCELL_BAD_UNIT: return "The unit must be kg or g.";
    case WSOFT_LOADCELL_BAD_STEP: return "Pick a division size from the list.";
    case WSOFT_LOADCELL_TOO_SMALL: return "The reading hardly moved. Put the test weight on the scale, then try again.";
    case WSOFT_LOADCELL_FORMAT_SPAN:
      return "The test weight used last time does not fit this division size. Pick a finer one, or set the test weight again after.";
    case WSOFT_LOADCELL_BAD_FILTER: return "Pick a filter from the list.";
    case WSOFT_LOADCELL_BAD_ZERO_TRACK: return "Pick an auto zero choice from the list.";
    case WSOFT_LOADCELL_BAD_DRIFT_GUARD: return "Pick a Drift Guard choice from the list.";
  }
  return "Something went wrong.";
}

void WsoftLoadcellBlock::reset() {
  _sum = 0;
  _n = 0;
}

bool WsoftLoadcellBlock::add(int32_t raw, uint32_t now_ms, int32_t *mean) {
  // A little under the block length, so a chip at 10 a second (a count every 100 ms, give or
  // take) gives one block per count and not one per two.
  bool complete = _n > 0 && now_ms - _start >= WSOFT_LOADCELL_BLOCK_MS - 10;
  if (complete) {
    *mean = (int32_t)divide_round(_sum, (int64_t)_n);
    reset();
  }
  if (_n == 0) _start = now_ms;
  _sum += raw;
  _n++;
  return complete;
}

void WsoftLoadcellZeroTrack::reset() {
  _offset = 0;
  _still = 0;
}

void WsoftLoadcellZeroTrack::update(const WsoftLoadcellCal &cal, int32_t average) {
  int64_t division = division_counts_x10(cal);
  if (cal.zero_track <= 0 || division <= 0) {
    reset();
    return;
  }
  int64_t load = (int64_t)average - cal.zero_raw - _offset;
  int64_t band = division * cal.zero_track / 10;
  if (load > band || load < -band) {
    _still = 0;
    return;
  }
  if (_still < WSOFT_LOADCELL_ZERO_TRACK_STILL) {
    _still++;
    return;
  }
  int64_t step = load / 8;
  if (step == 0) step = load > 0 ? 1 : load < 0 ? -1 : 0;
  int64_t reach = division * WSOFT_LOADCELL_ZERO_TRACK_REACH / 10;
  int64_t next = _offset + step;
  if (next > reach) next = reach;
  if (next < -reach) next = -reach;
  _offset = (int32_t)next;
}

void WsoftLoadcellDriftGuard::reset() {
  _offset = 0;
  _still = 0;
  _have = false;
  _holds = 0;
  _jumps = 0;
  _limits = 0;
}

void WsoftLoadcellDriftGuard::release(int32_t average) {
  _offset = 0;
  _still = 0;
  _ref = average;
}

void WsoftLoadcellDriftGuard::update(const WsoftLoadcellCal &cal, int32_t average, int32_t zero_offset) {
  int64_t division = division_counts_x10(cal);
  if (cal.drift_guard <= 0 || division <= 0) {
    reset();
    return;
  }
  if (!_have) {
    _have = true;
    _last = average;
    release(average);
    return;
  }
  int64_t delta = (int64_t)average - _last;
  _last = average;
  // Near zero with auto zero on: auto zero keeps the zero, nothing is held here.
  int64_t load = (int64_t)average - cal.zero_raw - zero_offset;
  if (cal.zero_track > 0 && (load < 0 ? -load : load) * 10 <= division * cal.zero_track) {
    release(average);
    return;
  }
  // More than half a division in one count: a load went on or came off.
  if ((delta < 0 ? -delta : delta) * 20 > division) {
    if (holding()) _jumps++;
    release(average);
    return;
  }
  if (_still < WSOFT_LOADCELL_DRIFT_GUARD_STILL) {
    // Settling: it must stay within one division of where it began to settle.
    int64_t away = (int64_t)average - _ref;
    if ((away < 0 ? -away : away) * 10 > division) release(average);
    else if (++_still >= WSOFT_LOADCELL_DRIFT_GUARD_STILL) _holds++;
    return;
  }
  int64_t next = _offset + delta;
  if ((next < 0 ? -next : next) * 10 > division * cal.drift_guard) {
    _limits++;
    release(average);  // past the limit: a real change, show all of it
    return;
  }
  _offset = (int32_t)next;
}

void WsoftLoadcellFilter::reset() {
  _count = 0;
  _next = 0;
  _odd = false;
}

void WsoftLoadcellFilter::push(int32_t raw) {
  _samples[_next] = raw;
  _next = (_next + 1) % WSOFT_LOADCELL_COUNTS;
  if (_count < WSOFT_LOADCELL_COUNTS) _count++;
}

void WsoftLoadcellFilter::add(int32_t raw, size_t window, int32_t jump) {
  _window = window < 1 ? 1 : window > WSOFT_LOADCELL_COUNTS ? WSOFT_LOADCELL_COUNTS : window;
  int32_t mean = 0;
  if (jump > 0 && average(&mean)) {
    // On a noisy scale (a fine division) the counts held are already further apart than the
    // jump: then only a count outside what the noise itself covers is a new load.
    int32_t noise = 0;
    if (spread(&noise) && noise > jump) jump = noise;
    int64_t away = (int64_t)raw - mean;
    if (away > jump || away < -jump) {
      bool same_side = _odd && ((int64_t)_odd_raw - mean > 0) == (away > 0);
      if (!same_side) {
        // The first count far off: a spike until a second one says the load changed.
        _odd = true;
        _odd_raw = raw;
        return;
      }
      int32_t first = _odd_raw;
      reset();
      push(first);
    }
  }
  _odd = false;
  push(raw);
}

bool WsoftLoadcellFilter::average(int32_t *out) const {
  if (_count == 0) return false;
  size_t n = _count < _window ? _count : _window;
  int64_t sum = 0;
  for (size_t i = 1; i <= n; i++) sum += _samples[(_next + WSOFT_LOADCELL_COUNTS - i) % WSOFT_LOADCELL_COUNTS];
  *out = (int32_t)divide_round(sum, (int64_t)n);
  return true;
}

bool WsoftLoadcellFilter::spread(int32_t *out) const {
  if (_count < 2) return false;
  int32_t low = _samples[0], high = _samples[0];
  for (size_t i = 1; i < _count; i++) {
    if (_samples[i] < low) low = _samples[i];
    if (_samples[i] > high) high = _samples[i];
  }
  *out = (int32_t)((int64_t)high - (int64_t)low);
  return true;
}
