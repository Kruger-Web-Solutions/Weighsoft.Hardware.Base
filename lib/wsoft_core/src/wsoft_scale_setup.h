#pragma once

// RS485 scale setup kept on the board (contract A4/A5 in
// docs/superpowers/plans/2026-11-07-wsoft-switches-scale-setup.md). Pure C++ for host tests
// (test/test_wsoft_scale_setup).
//
// Split: e (division), decimals and unit live in the weighing card and are read from it;
// the SA number, Max, Min, blanking points, stable samples and tare live on the board.
// Nothing here makes a scale fit for trade on its own: that needs approval and on-site checks.
//
// kg values are held as whole units of 0.00001 kg (5 decimals, the Hub's limit), or
// WSOFT_KG_UNSET. In JSON they are decimal text, "" when not set.

#include <stddef.h>
#include <stdint.h>

static const int64_t WSOFT_KG_UNSET = -1;
static const int64_t WSOFT_KG_UNITS_PER_KG = 100000;
static const int64_t WSOFT_KG_MAX_UNITS = 9999999999LL;  // 99999.99999 kg
static const int WSOFT_BLANK_UNSET = -1;
static const size_t WSOFT_SA_NUMBER_MAX = 32;
static const int WSOFT_STABLE_SAMPLES_MIN = 3;
static const int WSOFT_STABLE_SAMPLES_MAX = 20;
static const int WSOFT_BLANK_E_MAX = 100;

// The last values read from the weighing card. unit: 1 kg, 2 t, 3 g.
struct WsoftCardFormat {
  bool read = false;
  int32_t division = 0;
  int decimal = 0;
  int unit = 1;
};

struct WsoftScaleSetup {
  char sa_number[WSOFT_SA_NUMBER_MAX + 1] = "";
  int64_t max_kg = WSOFT_KG_UNSET;
  int64_t min_kg = WSOFT_KG_UNSET;
  int blank_below_e = WSOFT_BLANK_UNSET;  // no default until the SA rule is confirmed
  int blank_above_e = 9;
  int stable_samples = 3;
  bool tare_enabled = true;
  int64_t tare_max_kg = WSOFT_KG_UNSET;
  int64_t preset_tare_kg = WSOFT_KG_UNSET;
};

bool operator==(const WsoftScaleSetup &a, const WsoftScaleSetup &b);
inline bool operator!=(const WsoftScaleSetup &a, const WsoftScaleSetup &b) { return !(a == b); }

// A5 body, one field at a time. A field that is not there keeps its stored value.
// The service sets bad_type when the JSON value has the wrong type.
struct WsoftSetupText {
  bool given = false;
  bool bad_type = false;
  const char *text = nullptr;
};
struct WsoftSetupInt {
  bool given = false;
  bool bad_type = false;
  bool null = false;  // only blank_below_e may be null (= not set)
  int64_t value = 0;
};
struct WsoftScaleSetupInput {
  WsoftSetupText sa_number, max_kg, min_kg, tare_max_kg, preset_tare_kg;
  WsoftSetupInt blank_below_e, blank_above_e, stable_samples;
  bool has_tare_enabled = false;
  bool tare_enabled_bad_type = false;
  bool tare_enabled = true;
  const char *reason = nullptr;
};

enum WsoftSetupError {
  WSOFT_SETUP_OK = 0,
  WSOFT_SETUP_LOCKED,           // Approval is ON, or the scale is on RS232 (checked by the board)
  WSOFT_SETUP_ORDER,            // needs max > e, min >= e, min < max
  WSOFT_SETUP_NUMBER,           // not a number, more than 5 decimals, or not a multiple of e
  WSOFT_SETUP_TEXT,             // SA number: 1 to 32 printable characters
  WSOFT_SETUP_RANGE,            // stable samples 3..20, blank counts 0..100, tare_max <= max, preset <= tare_max
  WSOFT_SETUP_REASON_REQUIRED,
  WSOFT_SETUP_REASON_TOO_LONG,
  WSOFT_SETUP_BUSY,             // the board could not take the setup lock in time (checked by the board)
};

const char *wsoft_setup_error_code(WsoftSetupError error);
const char *wsoft_setup_error_text(WsoftSetupError error);

// e in kg units from the card. False when the card has not been read, the unit is not
// kg, t or g, or e is not a whole number of 0.00001 kg.
bool wsoft_card_e(const WsoftCardFormat &card, int64_t *e);

// A reading (value scaled by decimals, in unit) in kg units. False when not exact.
bool wsoft_reading_to_kg(int32_t value, int decimals, int unit, int64_t *kg);
// kg units back to a reading in decimals and unit. False when not exact or too big.
bool wsoft_kg_to_reading(int64_t kg, int decimals, int unit, int32_t *value);

// "" (unset), digits with at most 5 integer digits, and an optional point with 1 to 5 digits.
bool wsoft_kg_parse(const char *text, int64_t *kg);
// "1.5", "3000", "" for WSOFT_KG_UNSET.
void wsoft_kg_text(int64_t kg, char *out, size_t size);

// Checks the A5 body against the card and applies it. On an error *setup is unchanged.
// *changed says whether anything differs from before.
WsoftSetupError wsoft_scale_setup_update(const WsoftScaleSetupInput &in, const WsoftCardFormat &card,
                                         WsoftScaleSetup *setup, bool *changed);

// The A4 "missing" keys, in this order: sa_number, max_kg, min_kg, blank_below_e, card.
// A value that is set but no longer fits the card's e counts as missing.
// Returns how many were written to keys (at most 5). complete = 0 missing.
int wsoft_scale_setup_missing(const WsoftScaleSetup &setup, const WsoftCardFormat &card, const char *keys[5]);
bool wsoft_scale_setup_complete(const WsoftScaleSetup &setup, const WsoftCardFormat &card);

// Consecutive fresh, unchanged readings (the board's part of "stable").
class WsoftStableSamples {
 public:
  void update(bool linked, bool fresh, int32_t value);
  int count() const { return count_; }
  bool stable(int needed) const { return count_ >= needed; }

 private:
  int count_ = 0;
  int32_t value_ = 0;
};

// Why a reading is held back from trade and from a save.
enum WsoftScaleHold {
  WSOFT_HOLD_NONE = 0,
  WSOFT_HOLD_NOT_STABLE,  // fewer than stable_samples unchanged readings
  WSOFT_HOLD_BELOW_MIN,   // above zero but below Min: not for trade
  WSOFT_HOLD_BLANK,       // below -blank_below_e*e or above max + blank_above_e*e: shows ----
};

// blank(value) = value < -blank_below_e*e || value > max + blank_above_e*e. Each side only
// when its setting is there and the card's e is known. Below Min only for 0 < net < min (net =
// gross when no tare is on). With a complete setup, a reading that is not exact kg is BLANK.
WsoftScaleHold wsoft_scale_hold(const WsoftScaleSetup &setup, const WsoftCardFormat &card, int32_t gross, int32_t net,
                                int decimals, int unit, bool samples_stable);

// Runtime tare, in the reading's own decimals and unit (cleared when they change).
struct WsoftTare {
  bool on = false;
  int32_t value = 0;
  int decimals = 0;
  int unit = 0;
};

enum WsoftTareError {
  WSOFT_TARE_OK = 0,
  WSOFT_TARE_OFF,         // tare is switched off in the setup
  WSOFT_TARE_NOT_STABLE,
  WSOFT_TARE_TOO_BIG,     // more than tare_max (or Max when tare_max is not set)
  WSOFT_TARE_EMPTY,       // nothing on the scale, or no preset tare set
};

const char *wsoft_tare_error_code(WsoftTareError error);

// The TARE button: tare = the stable gross on the scale now.
WsoftTareError wsoft_tare_take(const WsoftScaleSetup &setup, bool stable, int32_t value, int decimals, int unit,
                               WsoftTare *tare);
// The preset tare from the setup, in the reading's decimals and unit.
WsoftTareError wsoft_tare_preset(const WsoftScaleSetup &setup, int decimals, int unit, WsoftTare *tare);
// net = gross - tare. False (and *net = gross) when no tare or the reading's format changed.
bool wsoft_tare_net(const WsoftTare &tare, int32_t gross, int decimals, int unit, int32_t *net);
