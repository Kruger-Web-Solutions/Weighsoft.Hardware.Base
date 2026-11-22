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
// Not from the datasheet, our own choices: the filter (averaging lengths, the jump and the hold
// on the last digit), the smallest span that counts as a real test weight, and the display steps.

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
  int decimals;         // 0..4 digits after the point (the scale setup on the Calibrate tab)
  int unit;             // DJB unit code: 1 kg, 3 g
  int step;             // display step in digits: 1, 2, 5, 10, 20 or 50
  int filter;           // WSOFT_LOADCELL_FILTER_OFF .. _HIGH: how hard the reading is steadied
  int zero_track;       // auto zero: 0 off, 1..WSOFT_LOADCELL_ZERO_TRACK_MAX divisions around zero
  int drift_guard;      // Drift Guard: 0 off, 1..WSOFT_LOADCELL_DRIFT_GUARD_MAX divisions it may hold back
};

// The filter on the Scale setup tab (Rhyno, 22 Nov 2026: the HX711 is not steady).
// OFF shows every count as it comes. The others average the last counts (wsoft_loadcell_filter_window),
// still follow a new load at once (WsoftLoadcellFilter) and hold the last digit until the
// weight has really moved (wsoft_loadcell_weight_steady).
enum { WSOFT_LOADCELL_FILTER_OFF = 0, WSOFT_LOADCELL_FILTER_LOW, WSOFT_LOADCELL_FILTER_MEDIUM, WSOFT_LOADCELL_FILTER_HIGH };
bool wsoft_loadcell_filter_valid(int filter);
// Counts averaged: 1, 4, 12, 32. The filter gets 10 counts a second whatever the chip's rate
// (WsoftLoadcellBlock), so that is 0.1, 0.4, 1.2 and 3.2 seconds.
size_t wsoft_loadcell_filter_window(int filter);
// How far one count must be from the average to be a new load and not noise: 4 divisions, in
// raw counts. 0 (= plain averaging) with the filter off or before the board is calibrated.
int32_t wsoft_loadcell_filter_jump(const WsoftLoadcellCal &cal);

enum WsoftLoadcellError {
  WSOFT_LOADCELL_OK = 0,
  WSOFT_LOADCELL_NO_READING,   // the HX711 gave no usable count
  WSOFT_LOADCELL_NO_ZERO,      // set zero first
  WSOFT_LOADCELL_BAD_WEIGHT,   // the test weight is not above 0, or too big
  WSOFT_LOADCELL_BAD_DECIMALS,
  WSOFT_LOADCELL_BAD_UNIT,
  WSOFT_LOADCELL_BAD_STEP,
  WSOFT_LOADCELL_TOO_SMALL,    // the count hardly moved: no test weight on the scale?
  WSOFT_LOADCELL_FORMAT_SPAN,  // the stored test weight is not a whole number of the new last digit
  WSOFT_LOADCELL_BAD_FILTER,
  WSOFT_LOADCELL_BAD_ZERO_TRACK,
  WSOFT_LOADCELL_BAD_DRIFT_GUARD
};

void wsoft_loadcell_defaults(WsoftLoadcellCal *cal);
// Zero and span are both set: the board can show a weight.
bool wsoft_loadcell_calibrated(const WsoftLoadcellCal &cal);

// Zero with the scale empty. A span that is already set stays (it is a difference of counts).
WsoftLoadcellError wsoft_loadcell_set_zero(WsoftLoadcellCal *cal, bool have_raw, int32_t raw);
// Span with the test weight on. value / decimals / unit: the weight as typed. It is stored in
// the scale's own decimals and unit (wsoft_loadcell_set_format), so "2" on a 0.001 kg scale is
// 2000. BAD_DECIMALS when it has more digits than the scale shows. On an error *cal is unchanged.
WsoftLoadcellError wsoft_loadcell_set_span(WsoftLoadcellCal *cal, bool have_raw, int32_t raw, int32_t value,
                                           int decimals, int unit);

// The scale setup the weighing card holds on RS485: unit, digits after the point and the
// division e (= step in the last digit: 0.002 kg is decimals 3, step 2). A span that is set is
// kept and re-written in the new decimals and unit; FORMAT_SPAN when that is not exact (a
// 2.5 kg test weight on a scale with no decimals). On an error *cal is unchanged.
// g takes at most 2 decimals: the setup rules count in 0.00001 kg (wsoft_scale_setup.h).
WsoftLoadcellError wsoft_loadcell_set_format(WsoftLoadcellCal *cal, int decimals, int unit, int step);
// value at (from_decimals, from_unit) as digits at (to_decimals, to_unit). False when not exact
// or outside 1..WSOFT_LOADCELL_MAX_VALUE. Units: 1 kg, 3 g.
bool wsoft_loadcell_rescale(int32_t value, int from_decimals, int from_unit, int to_decimals, int to_unit,
                            int32_t *out);
bool wsoft_loadcell_step_valid(int step);
WsoftLoadcellError wsoft_loadcell_set_filter(WsoftLoadcellCal *cal, int filter);
WsoftLoadcellError wsoft_loadcell_set_zero_track(WsoftLoadcellCal *cal, int divisions);
WsoftLoadcellError wsoft_loadcell_set_drift_guard(WsoftLoadcellCal *cal, int divisions);

// The weight for a count, rounded half away from zero to the display step. False when the
// board is not calibrated or the result does not fit: never a weight then.
bool wsoft_loadcell_weight(const WsoftLoadcellCal &cal, int32_t raw, int32_t *value);

// "zero_first", "too_small" ... for the JSON answer; a sentence for the page.
const char *wsoft_loadcell_error_code(WsoftLoadcellError error);
const char *wsoft_loadcell_error_text(WsoftLoadcellError error);

// The same weight, with the last digit held (filter not OFF): a weight already shown (have_last,
// last) stays until the load is more than 3/4 of a division away from it, so a load that sits
// on the edge between two divisions does not flip between them. Plain rounding when nothing
// was shown yet or the filter is OFF.
bool wsoft_loadcell_weight_steady(const WsoftLoadcellCal &cal, int32_t raw, bool have_last, int32_t last,
                                  int32_t *value);
// How far the counts in the filter are apart (highest less lowest), in tenths of a division:
// 10 = the noise is one whole division. False when the board is not calibrated.
bool wsoft_loadcell_noise_tenths(const WsoftLoadcellCal &cal, int32_t spread, int32_t *tenths);

// The HX711 runs at 10 or 80 counts a second (set on its board). The counts of each tenth of a
// second are gathered into one (their mean), so the filter, the jump and the auto zero work the
// same at either rate (bench, 22 Nov 2026: a blip two counts long got through the filter).
static const uint32_t WSOFT_LOADCELL_BLOCK_MS = 100;

class WsoftLoadcellBlock {
 public:
  void reset();
  // One count from the chip at now_ms. True when the tenth of a second before it is complete:
  // *mean is the mean of its counts. This count then opens the next one.
  bool add(int32_t raw, uint32_t now_ms, int32_t *mean);

 private:
  int64_t _sum = 0;
  uint32_t _n = 0;
  uint32_t _start = 0;
};

// Auto zero (zero tracking; Rhyno, 22 Nov 2026: the empty scale creeps off zero). While the
// load has stayed within cal.zero_track divisions of zero for WSOFT_LOADCELL_ZERO_TRACK_STILL
// filtered counts in a row (2 seconds), the zero follows it: an eighth of what is left, each
// count. The offset is kept in memory only (a restart or a new zero clears it) and never goes
// further than WSOFT_LOADCELL_ZERO_TRACK_REACH divisions from the set zero. Our own choices.
// It also takes away a small load that is put on and left inside that band: that is what auto
// zero does on any scale.
static const int WSOFT_LOADCELL_ZERO_TRACK_MAX = 3;
static const int WSOFT_LOADCELL_ZERO_TRACK_STILL = 20;
static const int WSOFT_LOADCELL_ZERO_TRACK_REACH = 20;

class WsoftLoadcellZeroTrack {
 public:
  void reset();
  // Counts to add to cal.zero_raw.
  int32_t offset() const { return _offset; }
  // One filtered count (10 a second).
  void update(const WsoftLoadcellCal &cal, int32_t average);

 private:
  int32_t _offset = 0;
  int _still = 0;
};

// Drift Guard (Rhyno, 22 Nov 2026: the zero floats, about 12 g in a slow wave, the same with
// the scale empty and loaded). With a load on, the board sees one number, so it cannot tell
// the zero floating from a slow real change. What it can do: once the count has stayed within
// one division for WSOFT_LOADCELL_DRIFT_GUARD_STILL filtered counts (2 seconds), every slow
// movement after that is taken as drift and held back, so the weight shown stands still.
// It lets go, and shows the whole weight again, when
//   - one count moves more than half a division from the last (a load put on or taken off), or
//   - the drift held back would pass cal.drift_guard divisions (a slow real change, like a
//     pack being topped up: never hidden by more than that limit).
// Near zero with auto zero on it does nothing: that is auto zero's job. The amount held back
// is kept in memory only and starts from 0 at every release, so it can never pile up.
// The price, by design: a slow change smaller than the limit stays hidden until the next
// release. Our own choices, not from a standard.
// The limit went from 5 to 20 divisions after the first bench run (22 Nov): on a 0.002 kg
// division the 12 g wave is 6 divisions, more than 5 could hold.
static const int WSOFT_LOADCELL_DRIFT_GUARD_MAX = 20;
static const int WSOFT_LOADCELL_DRIFT_GUARD_STILL = 20;

class WsoftLoadcellDriftGuard {
 public:
  void reset();
  // Counts held back: take them off the filtered count (or add them to the zero).
  int32_t offset() const { return _offset; }
  // True while the weight shown is being held.
  bool holding() const { return _still >= WSOFT_LOADCELL_DRIFT_GUARD_STILL; }
  // One filtered count (10 a second). zero_offset: what auto zero has moved the zero by.
  void update(const WsoftLoadcellCal &cal, int32_t average, int32_t zero_offset);
  // Since the last reset, for checking it on a bench (LOADCELL INFO on the cable): how often it
  // began to hold, and how often it let go of a held weight for a jump or for the limit.
  uint32_t holds() const { return _holds; }
  uint32_t jump_releases() const { return _jumps; }
  uint32_t limit_releases() const { return _limits; }

 private:
  void release(int32_t average);
  uint32_t _holds = 0;
  uint32_t _jumps = 0;
  uint32_t _limits = 0;
  int32_t _offset = 0;
  int32_t _last = 0;
  int32_t _ref = 0;
  bool _have = false;
  int _still = 0;
};

// The last counts from the chip. average() is over the newest `window` of them. With a jump
// above 0, a count further than that from the average is a new load, not noise: one such
// count alone is dropped (a spike); a second one on the same side starts the average again
// from those two, so the weight follows a new load at once however long the average is.
// When the counts held are themselves further apart than the jump (a noisy scale), that
// spread is the jump: noise is never taken for a new load.
static const size_t WSOFT_LOADCELL_COUNTS = 32;

class WsoftLoadcellFilter {
 public:
  void reset();
  void add(int32_t raw, size_t window = 8, int32_t jump = 0);
  size_t count() const { return _count; }
  // False until at least one count is in.
  bool average(int32_t *out) const;
  // Highest less lowest of the counts held (up to WSOFT_LOADCELL_COUNTS). False under 2 counts.
  bool spread(int32_t *out) const;

 private:
  void push(int32_t raw);
  int32_t _samples[WSOFT_LOADCELL_COUNTS] = {};
  size_t _count = 0;
  size_t _next = 0;
  size_t _window = 8;
  bool _odd = false;      // one count far from the average is waiting for a second
  int32_t _odd_raw = 0;
};
