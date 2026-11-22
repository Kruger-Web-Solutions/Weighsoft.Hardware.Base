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
  }
  return "Something went wrong.";
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
