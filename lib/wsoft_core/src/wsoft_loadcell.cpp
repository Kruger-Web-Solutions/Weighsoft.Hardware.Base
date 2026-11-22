#include "wsoft_loadcell.h"

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
}

bool wsoft_loadcell_calibrated(const WsoftLoadcellCal &cal) {
  return cal.zero_set && cal.span_set && cal.span_counts != 0 && cal.span_value > 0;
}

bool wsoft_loadcell_step_valid(int step) {
  return step == 1 || step == 2 || step == 5 || step == 10 || step == 20 || step == 50;
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
  if (value <= 0) return false;
  // kg to g is three more digits, g to kg three fewer.
  int shift = (to_decimals - from_decimals) + (from_unit == 1 && to_unit == 3 ? 3 : 0) -
              (from_unit == 3 && to_unit == 1 ? 3 : 0);
  int64_t digits = value;
  for (; shift > 0; shift--) {
    digits *= 10;
    if (digits > WSOFT_LOADCELL_MAX_VALUE) return false;
  }
  for (; shift < 0; shift++) {
    if (digits % 10 != 0) return false;
    digits /= 10;
  }
  if (digits <= 0 || digits > WSOFT_LOADCELL_MAX_VALUE) return false;
  *out = (int32_t)digits;
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

WsoftLoadcellError wsoft_loadcell_set_step(WsoftLoadcellCal *cal, int step) {
  if (!wsoft_loadcell_step_valid(step)) return WSOFT_LOADCELL_BAD_STEP;
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

bool wsoft_loadcell_weight(const WsoftLoadcellCal &cal, int32_t raw, int32_t *value) {
  if (!wsoft_loadcell_calibrated(cal) || wsoft_hx711_saturated(raw)) return false;
  int64_t counts = (int64_t)raw - (int64_t)cal.zero_raw;
  int64_t digits = divide_round(counts * (int64_t)cal.span_value, (int64_t)cal.span_counts);
  int step = wsoft_loadcell_step_valid(cal.step) ? cal.step : 1;
  digits = divide_round(digits, step) * step;
  if (digits > WSOFT_LOADCELL_MAX_VALUE || digits < -WSOFT_LOADCELL_MAX_VALUE) return false;
  *value = (int32_t)digits;
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
  }
  return "Something went wrong.";
}

void WsoftLoadcellFilter::reset() {
  _count = 0;
  _next = 0;
}

void WsoftLoadcellFilter::add(int32_t raw) {
  _samples[_next] = raw;
  _next = (_next + 1) % WSOFT_LOADCELL_AVERAGE;
  if (_count < WSOFT_LOADCELL_AVERAGE) _count++;
}

bool WsoftLoadcellFilter::average(int32_t *out) const {
  if (_count == 0) return false;
  int64_t sum = 0;
  for (size_t i = 0; i < _count; i++) sum += _samples[i];
  *out = (int32_t)divide_round(sum, (int64_t)_count);
  return true;
}
