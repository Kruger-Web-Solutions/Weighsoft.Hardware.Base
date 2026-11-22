#pragma once

// Load cell on an HX711 amplifier board (Rhyno, 22 Nov 2026): the third W-Soft weight source,
// next to the RS485 junction box and an RS232 scale. Pure C++ so the rules run in host tests;
// the pins, the sampling task and the store are in lib/wsoft_esp/src/loadcell.cpp.
//
// The HX711 sends a raw 24-bit count, not a weight. The board turns it into a weight with a
// two-point calibration kept on the board:
//   zero: the count with the scale empty;
//   span: the count with a known test weight on, and that weight as typed.
// weight = (count - zero count) * test weight / (test weight count - zero count).
//
// From the HX711 datasheet (Avia Semiconductor): the count is 24-bit two's complement, and it
// stays at 0x800000 (lowest) or 0x7FFFFF (highest) when the input is out of range.
// Not from the datasheet, our own choices: the averaging length, the smallest span that counts
// as a real test weight, and the display steps.

#include <stddef.h>
#include <stdint.h>

// 24 bits as clocked out of the chip (first bit = most significant) to a signed count.
int32_t wsoft_hx711_decode(uint32_t bits24);
// The chip's out-of-range values: never a weight.
bool wsoft_hx711_saturated(int32_t raw);

// A test weight must move the count at least this far from zero, or the "test weight" was
// most likely an empty scale. Our choice (about 0.01 % of the chip's range), not a datasheet value.
static const int32_t WSOFT_LOADCELL_MIN_SPAN_COUNTS = 1000;
// The weight is a DJB-style value (digits without the point) and must fit the screen.
static const int32_t WSOFT_LOADCELL_MAX_VALUE = 9999999;

struct WsoftLoadcellCal {
  bool zero_set;
  int32_t zero_raw;
  bool span_set;
  int32_t span_counts;  // count with the test weight on, less the zero count; never 0 when span_set
  int32_t span_value;   // the test weight in display digits (2.000 kg at 3 decimals = 2000)
  int decimals;         // 0..4, taken from how the test weight was typed
  int unit;             // DJB unit code: 1 kg, 3 g
  int step;             // display step in digits: 1, 2, 5, 10, 20 or 50
};

enum WsoftLoadcellError {
  WSOFT_LOADCELL_OK = 0,
  WSOFT_LOADCELL_NO_READING,   // the HX711 gave no usable count
  WSOFT_LOADCELL_NO_ZERO,      // set zero first
  WSOFT_LOADCELL_BAD_WEIGHT,   // the test weight is not above 0, or too big
  WSOFT_LOADCELL_BAD_DECIMALS,
  WSOFT_LOADCELL_BAD_UNIT,
  WSOFT_LOADCELL_BAD_STEP,
  WSOFT_LOADCELL_TOO_SMALL     // the count hardly moved: no test weight on the scale?
};

void wsoft_loadcell_defaults(WsoftLoadcellCal *cal);
// Zero and span are both set: the board can show a weight.
bool wsoft_loadcell_calibrated(const WsoftLoadcellCal &cal);

// Zero with the scale empty. A span that is already set stays (it is a difference of counts).
WsoftLoadcellError wsoft_loadcell_set_zero(WsoftLoadcellCal *cal, bool have_raw, int32_t raw);
// Span with the test weight on. value / decimals: the weight as typed. On an error *cal is unchanged.
WsoftLoadcellError wsoft_loadcell_set_span(WsoftLoadcellCal *cal, bool have_raw, int32_t raw, int32_t value,
                                           int decimals, int unit);
WsoftLoadcellError wsoft_loadcell_set_step(WsoftLoadcellCal *cal, int step);
bool wsoft_loadcell_step_valid(int step);

// The weight for a count, rounded half away from zero to the display step. False when the
// board is not calibrated or the result does not fit: never a weight then.
bool wsoft_loadcell_weight(const WsoftLoadcellCal &cal, int32_t raw, int32_t *value);

// "zero_first", "too_small" ... for the JSON answer; a sentence for the page.
const char *wsoft_loadcell_error_code(WsoftLoadcellError error);
const char *wsoft_loadcell_error_text(WsoftLoadcellError error);

// A moving average over the last WSOFT_LOADCELL_AVERAGE counts, to steady the last digits.
static const size_t WSOFT_LOADCELL_AVERAGE = 8;

class WsoftLoadcellFilter {
 public:
  void reset();
  void add(int32_t raw);
  size_t count() const { return _count; }
  // False until at least one count is in.
  bool average(int32_t *out) const;

 private:
  int32_t _samples[WSOFT_LOADCELL_AVERAGE] = {};
  size_t _count = 0;
  size_t _next = 0;
};
