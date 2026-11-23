#pragma once

// RS485 scale tuning (Rhyno, 23 Nov 2026): one Scale setup for the junction box, the same as the
// load cell's. Pure C++ for host tests (test/test_wsoft_rs485_tuning).
//
// Where each value lives:
//   - DJB-M1X (LED display): stable range/time and zero tracking are card registers (M1X manual
//     V2.0, Scale Parameters 05..08, 41309..41315). The board writes them there and reads them back.
//   - DJB-W1X (no display): its manual and the protocol V2.0 table list NO such registers. On the
//     bench (23 Nov 2026) it answered 0 for all of them and did not keep a write. So the board
//     keeps them and does the work itself (stable judging, auto zero), and the page says so.
// The board always judges "stable" itself (no DJB register says stable), so it always uses the
// stable range and time. Auto zero on the board runs only on a card with no format registers:
// an M1X tracks its own zero, and two auto zeros would fight.
//
// ZERO (the button, both cards): the board takes the card's weight now as its zero and shows
// the weight less it. Nothing is written to the card: its calibration stays as it is. Kept over
// a restart (NVS). The Calibrate tab's Zero is the card's own zero calibration (AD register).
//
// Units: ranges are steps of the reading's last digit ("digits"); times are tenths of a second,
// as the M1X keypad shows them (0.1 .. 9.9 s). The register scaling of the two times (tenths) is
// our reading of the manual's 0.1..9.9 range and its 0.5 factory value; the page shows the raw
// number read back so a bench can check it.

#include <stddef.h>
#include <stdint.h>

// Limits from the M1X manual's Scale Parameters table (menu 05..08).
static const int32_t WSOFT_TUNE_STABLE_RANGE_MAX = 999;    // 05. Sr 0..999 (0 = no stable judging on the card)
static const int32_t WSOFT_TUNE_TIME_MIN = 1;              // 06. St / 08. Zt 0.1 s
static const int32_t WSOFT_TUNE_TIME_MAX = 99;             //                  9.9 s
static const int32_t WSOFT_TUNE_ZERO_TRACK_MAX = 9999;     // 07. Zr 0..9999 (0 = no auto zero)
// Protocol V2.0 (DJB-W/M[1-16], so both cards): 41321 filter 1 0..9, 41323 filter 2 0..10.
static const int32_t WSOFT_TUNE_FILTER1_MAX = 9;
static const int32_t WSOFT_TUNE_FILTER2_MAX = 10;

struct WsoftRs485Tuning {
  int32_t stable_range = 0;     // digits; 0 = the reading must not move at all (the old rule)
  int32_t stable_time = 0;      // tenths of a second the reading must hold; 0 = stable samples only
  int32_t zero_track_range = 0; // digits around zero that auto zero takes away; 0 = off
  int32_t zero_track_time = 10; // tenths of a second the reading must sit there first (1.0 s)
};

bool operator==(const WsoftRs485Tuning &a, const WsoftRs485Tuning &b);
inline bool operator!=(const WsoftRs485Tuning &a, const WsoftRs485Tuning &b) { return !(a == b); }

// stable_time 0 is the board's "off"; the card itself takes 1..99. zero_track_time 1..99.
bool wsoft_rs485_tuning_valid(const WsoftRs485Tuning &t);

// value rounded half away from zero to a whole number of step (e in digits). step <= 1: value.
int32_t wsoft_round_to_step(int32_t value, int32_t step);

// Auto zero on the board (a card with no zero tracking, the DJB-W1X). While the reading has been
// stable and within range digits of zero for time_ms, the zero follows it in one go (the M1X
// manual's wording: "after weight being stable and the zero variation in Zero Auto-tracking Time
// is within the Zero Auto-tracking Limit"). The offset stays in memory (a restart, ZERO or a
// tuning change clears it) and never goes further than WSOFT_ZERO_TRACK_REACH times the range
// from the zero that was set: our choice, so a slow real load is not tracked away for good.
static const int32_t WSOFT_ZERO_TRACK_REACH = 20;

class WsoftZeroTracker {
 public:
  void reset();
  // Digits taken off the weight (add to the set zero).
  int32_t offset() const { return offset_; }
  // shown: the weight after the set zero and this offset. stable: the board's stable judgement.
  // Returns true when the offset changed.
  bool update(bool linked, bool stable, int32_t shown, int32_t range, uint32_t time_ms, uint32_t now_ms);

 private:
  int32_t offset_ = 0;
  bool inside_ = false;
  uint32_t since_ms_ = 0;
};

// The board's ZERO for an RS485 card: the card's weight that reads 0, in the decimals and unit it
// was taken in. A zero taken in other decimals or another unit is not used (cleared by the board).
struct WsoftBoardZero {
  bool set = false;
  int32_t raw = 0;
  int decimals = 0;
  int unit = 0;
};

// The weight to show for a card reading: raw less the zero (when it matches decimals and unit) and
// the auto zero offset, then rounded to step (e in digits; 1 = no rounding). False (and *shown =
// raw) when the result does not fit a signed 32-bit number.
bool wsoft_board_zero_apply(const WsoftBoardZero &zero, int32_t track_offset, int32_t raw, int decimals, int unit,
                            int32_t step, int32_t *shown);

enum WsoftZeroError {
  WSOFT_ZERO_OK = 0,
  WSOFT_ZERO_APPROVAL,   // Approval is ON (the same rule as the load cell's ZERO)
  WSOFT_ZERO_TARE,       // a tare is on: it was taken against the old zero
  WSOFT_ZERO_NO_SIGNAL,  // no reading from the card
  WSOFT_ZERO_NOT_STEADY, // not stable yet
  WSOFT_ZERO_SOURCE,     // the scale is not on the RS485 card
};

// The words the screen shows (17 letters at most) and the code the web page gets.
const char *wsoft_zero_error_screen(WsoftZeroError error);
const char *wsoft_zero_error_code(WsoftZeroError error);

// The checks before a ZERO, in the order the screen gives them.
WsoftZeroError wsoft_zero_check(bool rs485_card, bool approval_on, bool tare_on, bool linked, bool stable);
