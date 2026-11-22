#include "wsoft_scale_setup.h"

#include <stdio.h>
#include <string.h>

#include "wsoft_switch.h"

bool operator==(const WsoftScaleSetup &a, const WsoftScaleSetup &b) {
  return strcmp(a.sa_number, b.sa_number) == 0 && a.max_kg == b.max_kg && a.min_kg == b.min_kg &&
         a.blank_below_e == b.blank_below_e && a.blank_above_e == b.blank_above_e &&
         a.stable_samples == b.stable_samples && a.tare_enabled == b.tare_enabled && a.tare_max_kg == b.tare_max_kg &&
         a.preset_tare_kg == b.preset_tare_kg;
}

const char *wsoft_setup_error_code(WsoftSetupError error) {
  switch (error) {
    case WSOFT_SETUP_OK: return "";
    case WSOFT_SETUP_LOCKED: return "SETUP_LOCKED";
    case WSOFT_SETUP_ORDER: return "SETUP_ORDER";
    case WSOFT_SETUP_NUMBER: return "SETUP_NUMBER";
    case WSOFT_SETUP_TEXT: return "SETUP_TEXT";
    case WSOFT_SETUP_RANGE: return "SETUP_RANGE";
    case WSOFT_SETUP_REASON_REQUIRED: return "REASON_REQUIRED";
    case WSOFT_SETUP_REASON_TOO_LONG: return "REASON_TOO_LONG";
    case WSOFT_SETUP_BUSY: return "BUSY";
  }
  return "SETUP_NUMBER";
}

const char *wsoft_setup_error_text(WsoftSetupError error) {
  switch (error) {
    case WSOFT_SETUP_OK: return "Saved.";
    case WSOFT_SETUP_LOCKED: return "The setup is locked: switch Approval off first (RS232 scales are set on the indicator).";
    case WSOFT_SETUP_ORDER: return "Max must be more than e, Min at least e, and Min below Max.";
    case WSOFT_SETUP_NUMBER: return "Use a kg number with at most 5 decimals that is a whole number of e.";
    case WSOFT_SETUP_TEXT: return "The SA number is 1 to 32 letters, digits and signs.";
    case WSOFT_SETUP_RANGE: return "Stable samples 3 to 20, blank counts 0 to 100, tare at most Max, preset at most the tare limit.";
    case WSOFT_SETUP_REASON_REQUIRED: return "Give a short reason (letters, digits and signs only).";
    case WSOFT_SETUP_REASON_TOO_LONG: return "The reason can be at most 80 characters.";
    case WSOFT_SETUP_BUSY: return "The board is busy. Try again.";
  }
  return "";
}

static int64_t power10(int n) {
  int64_t p = 1;
  for (int i = 0; i < n; i++) p *= 10;
  return p;
}

// kg units = reading * 10^exponent (exponent may be negative).
static bool unit_exponent(int decimals, int unit, int *exponent) {
  if (decimals < 0 || decimals > 9) return false;
  int shift;
  if (unit == 1) shift = 0;
  else if (unit == 2) shift = 3;
  else if (unit == 3) shift = -3;
  else return false;
  *exponent = 5 - decimals + shift;
  return true;
}

bool wsoft_reading_to_kg(int32_t value, int decimals, int unit, int64_t *kg) {
  int p;
  if (!unit_exponent(decimals, unit, &p)) return false;
  if (p >= 0) {
    *kg = (int64_t)value * power10(p);
    return true;
  }
  int64_t div = power10(-p);
  if (value % div != 0) return false;
  *kg = value / div;
  return true;
}

bool wsoft_kg_to_reading(int64_t kg, int decimals, int unit, int32_t *value) {
  int p;
  if (!unit_exponent(decimals, unit, &p)) return false;
  int64_t v;
  if (p >= 0) {
    int64_t div = power10(p);
    if (kg % div != 0) return false;
    v = kg / div;
  } else {
    v = kg * power10(-p);
  }
  if (v > INT32_MAX || v < INT32_MIN) return false;
  *value = (int32_t)v;
  return true;
}

bool wsoft_card_e(const WsoftCardFormat &card, int64_t *e) {
  if (!card.read || card.division <= 0) return false;
  int64_t kg;
  if (!wsoft_reading_to_kg(card.division, card.decimal, card.unit, &kg) || kg <= 0) return false;
  *e = kg;
  return true;
}

bool wsoft_kg_parse(const char *text, int64_t *kg) {
  if (text == nullptr) return false;
  if (text[0] == 0) {
    *kg = WSOFT_KG_UNSET;
    return true;
  }
  int64_t whole = 0, fraction = 0;
  int whole_digits = 0, fraction_digits = 0;
  const char *p = text;
  for (; *p >= '0' && *p <= '9'; p++, whole_digits++) whole = whole * 10 + (*p - '0');
  if (whole_digits == 0 || whole_digits > 5) return false;
  if (*p == '.') {
    p++;
    for (; *p >= '0' && *p <= '9'; p++, fraction_digits++) fraction = fraction * 10 + (*p - '0');
    if (fraction_digits == 0 || fraction_digits > 5) return false;
  }
  if (*p != 0) return false;
  *kg = whole * WSOFT_KG_UNITS_PER_KG + fraction * power10(5 - fraction_digits);
  return true;
}

void wsoft_kg_text(int64_t kg, char *out, size_t size) {
  if (size == 0) return;
  if (kg < 0) {
    out[0] = 0;
    return;
  }
  int64_t fraction = kg % WSOFT_KG_UNITS_PER_KG;
  int digits = 5;
  while (digits > 0 && fraction % 10 == 0) {
    fraction /= 10;
    digits--;
  }
  if (digits == 0) {
    snprintf(out, size, "%lld", (long long)(kg / WSOFT_KG_UNITS_PER_KG));
  } else {
    snprintf(out, size, "%lld.%0*lld", (long long)(kg / WSOFT_KG_UNITS_PER_KG), digits, (long long)fraction);
  }
}

// ---------- A5 checks ----------

static WsoftSetupError take_sa_number(const WsoftSetupText &in, char out[WSOFT_SA_NUMBER_MAX + 1]) {
  if (!in.given) return WSOFT_SETUP_OK;
  if (in.bad_type || in.text == nullptr) return WSOFT_SETUP_TEXT;
  const char *start = in.text;
  while (*start == ' ') start++;
  size_t length = strlen(start);
  while (length > 0 && start[length - 1] == ' ') length--;
  if (length == 0 || length > WSOFT_SA_NUMBER_MAX) return WSOFT_SETUP_TEXT;
  for (size_t i = 0; i < length; i++) {
    unsigned char c = (unsigned char)start[i];
    if (c < 0x20 || c > 0x7E) return WSOFT_SETUP_TEXT;
  }
  memcpy(out, start, length);
  out[length] = 0;
  return WSOFT_SETUP_OK;
}

static WsoftSetupError take_kg(const WsoftSetupText &in, int64_t *out) {
  if (!in.given) return WSOFT_SETUP_OK;
  if (in.bad_type || !wsoft_kg_parse(in.text, out)) return WSOFT_SETUP_NUMBER;
  return WSOFT_SETUP_OK;
}

static WsoftSetupError take_int(const WsoftSetupInt &in, bool may_be_null, int low, int high, int *out) {
  if (!in.given) return WSOFT_SETUP_OK;
  if (in.bad_type) return WSOFT_SETUP_RANGE;
  if (in.null) {
    if (!may_be_null) return WSOFT_SETUP_RANGE;
    *out = WSOFT_BLANK_UNSET;
    return WSOFT_SETUP_OK;
  }
  if (in.value < low || in.value > high) return WSOFT_SETUP_RANGE;
  *out = (int)in.value;
  return WSOFT_SETUP_OK;
}

static bool set(int64_t kg) {
  return kg != WSOFT_KG_UNSET;
}

static bool fits_e(int64_t kg, int64_t e) {
  return !set(kg) || kg % e == 0;
}

// The tare limit: tare_max, else Max, else none (-1).
static int64_t tare_limit(const WsoftScaleSetup &s) {
  return set(s.tare_max_kg) ? s.tare_max_kg : s.max_kg;
}

static WsoftSetupError check(const WsoftScaleSetup &s, const WsoftCardFormat &card) {
  int64_t e = 0;
  bool have_e = wsoft_card_e(card, &e);
  if (have_e && (!fits_e(s.max_kg, e) || !fits_e(s.min_kg, e) || !fits_e(s.tare_max_kg, e) ||
                 !fits_e(s.preset_tare_kg, e))) {
    return WSOFT_SETUP_NUMBER;
  }
  if (set(s.tare_max_kg) && set(s.max_kg) && s.tare_max_kg > s.max_kg) return WSOFT_SETUP_RANGE;
  int64_t limit = tare_limit(s);
  if (set(s.preset_tare_kg) && set(limit) && s.preset_tare_kg > limit) return WSOFT_SETUP_RANGE;
  if (have_e && set(s.max_kg) && s.max_kg <= e) return WSOFT_SETUP_ORDER;
  if (have_e && set(s.min_kg) && s.min_kg < e) return WSOFT_SETUP_ORDER;
  if (set(s.min_kg) && set(s.max_kg) && s.min_kg >= s.max_kg) return WSOFT_SETUP_ORDER;
  return WSOFT_SETUP_OK;
}

WsoftSetupError wsoft_scale_setup_update(const WsoftScaleSetupInput &in, const WsoftCardFormat &card,
                                         WsoftScaleSetup *setup, bool *changed) {
  *changed = false;
  char reason[WSOFT_SWITCH_REASON_MAX + 1];
  WsoftSwitchError why = wsoft_reason_check(in.reason, true, reason);
  if (why == WSOFT_SWITCH_REASON_TOO_LONG) return WSOFT_SETUP_REASON_TOO_LONG;
  if (why != WSOFT_SWITCH_OK) return WSOFT_SETUP_REASON_REQUIRED;

  WsoftScaleSetup next = *setup;
  WsoftSetupError error = take_sa_number(in.sa_number, next.sa_number);
  if (error != WSOFT_SETUP_OK) return error;
  const WsoftSetupText *kg_in[] = {&in.max_kg, &in.min_kg, &in.tare_max_kg, &in.preset_tare_kg};
  int64_t *kg_out[] = {&next.max_kg, &next.min_kg, &next.tare_max_kg, &next.preset_tare_kg};
  for (int i = 0; i < 4; i++) {
    error = take_kg(*kg_in[i], kg_out[i]);
    if (error != WSOFT_SETUP_OK) return error;
  }
  if ((error = take_int(in.blank_below_e, true, 0, WSOFT_BLANK_E_MAX, &next.blank_below_e)) != WSOFT_SETUP_OK ||
      (error = take_int(in.blank_above_e, false, 0, WSOFT_BLANK_E_MAX, &next.blank_above_e)) != WSOFT_SETUP_OK ||
      (error = take_int(in.stable_samples, false, WSOFT_STABLE_SAMPLES_MIN, WSOFT_STABLE_SAMPLES_MAX,
                        &next.stable_samples)) != WSOFT_SETUP_OK) {
    return error;
  }
  if (in.has_tare_enabled) {
    if (in.tare_enabled_bad_type) return WSOFT_SETUP_RANGE;
    next.tare_enabled = in.tare_enabled;
  }
  error = check(next, card);
  if (error != WSOFT_SETUP_OK) return error;
  *changed = next != *setup;
  *setup = next;
  return WSOFT_SETUP_OK;
}

int wsoft_scale_setup_missing(const WsoftScaleSetup &s, const WsoftCardFormat &card, const char *keys[5]) {
  int64_t e = 0;
  bool have_e = wsoft_card_e(card, &e);
  bool max_ok = set(s.max_kg) && (!have_e || (s.max_kg % e == 0 && s.max_kg > e));
  bool min_ok = set(s.min_kg) && (!have_e || (s.min_kg % e == 0 && s.min_kg >= e)) &&
                (!set(s.max_kg) || s.min_kg < s.max_kg);
  int n = 0;
  if (s.sa_number[0] == 0) keys[n++] = "sa_number";
  if (!max_ok) keys[n++] = "max_kg";
  if (!min_ok) keys[n++] = "min_kg";
  if (s.blank_below_e == WSOFT_BLANK_UNSET) keys[n++] = "blank_below_e";
  if (!have_e) keys[n++] = "card";
  return n;
}

bool wsoft_scale_setup_complete(const WsoftScaleSetup &setup, const WsoftCardFormat &card) {
  const char *keys[5];
  return wsoft_scale_setup_missing(setup, card, keys) == 0;
}

// ---------- each reading ----------

void WsoftStableSamples::update(bool linked, bool fresh, int32_t value) {
  if (!linked || !fresh) {
    count_ = 0;
    return;
  }
  if (count_ > 0 && value == value_) {
    if (count_ < WSOFT_STABLE_SAMPLES_MAX) count_++;
    return;
  }
  value_ = value;
  count_ = 1;
}

WsoftScaleHold wsoft_scale_hold(const WsoftScaleSetup &s, const WsoftCardFormat &card, int32_t value, int32_t net,
                                int decimals, int unit, bool samples_stable) {
  int64_t e = 0, kg = 0, net_kg = 0;
  if (wsoft_card_e(card, &e) && wsoft_reading_to_kg(value, decimals, unit, &kg) &&
      wsoft_reading_to_kg(net, decimals, unit, &net_kg)) {
    if (s.blank_below_e != WSOFT_BLANK_UNSET && kg < -(int64_t)s.blank_below_e * e) return WSOFT_HOLD_BLANK;
    if (set(s.max_kg) && kg > s.max_kg + (int64_t)s.blank_above_e * e) return WSOFT_HOLD_BLANK;
    // The load on the plate: NET when a tare is on (to confirm against the SA rule).
    if (set(s.min_kg) && net_kg > 0 && net_kg < s.min_kg) return WSOFT_HOLD_BELOW_MIN;
  } else if (wsoft_scale_setup_complete(s, card)) {
    return WSOFT_HOLD_BLANK;  // a weight we cannot check is not shown as one
  }
  return samples_stable ? WSOFT_HOLD_NONE : WSOFT_HOLD_NOT_STABLE;
}

// ---------- tare ----------

const char *wsoft_tare_error_code(WsoftTareError error) {
  switch (error) {
    case WSOFT_TARE_OK: return "";
    case WSOFT_TARE_OFF: return "TARE_OFF";
    case WSOFT_TARE_NOT_STABLE: return "TARE_NOT_STABLE";
    case WSOFT_TARE_TOO_BIG: return "TARE_TOO_BIG";
    case WSOFT_TARE_EMPTY: return "TARE_EMPTY";
  }
  return "TARE_OFF";
}

WsoftTareError wsoft_tare_take(const WsoftScaleSetup &s, bool stable, int32_t value, int decimals, int unit,
                               WsoftTare *tare) {
  if (!s.tare_enabled) return WSOFT_TARE_OFF;
  if (!stable) return WSOFT_TARE_NOT_STABLE;
  if (value <= 0) return WSOFT_TARE_EMPTY;
  int64_t kg = 0, limit = tare_limit(s);
  if (!wsoft_reading_to_kg(value, decimals, unit, &kg)) return WSOFT_TARE_TOO_BIG;
  if (set(limit) && kg > limit) return WSOFT_TARE_TOO_BIG;
  tare->on = true;
  tare->value = value;
  tare->decimals = decimals;
  tare->unit = unit;
  return WSOFT_TARE_OK;
}

WsoftTareError wsoft_tare_preset(const WsoftScaleSetup &s, int decimals, int unit, WsoftTare *tare) {
  if (!s.tare_enabled) return WSOFT_TARE_OFF;
  int32_t value = 0;
  if (!set(s.preset_tare_kg) || s.preset_tare_kg == 0 || !wsoft_kg_to_reading(s.preset_tare_kg, decimals, unit, &value)) {
    return WSOFT_TARE_EMPTY;
  }
  tare->on = true;
  tare->value = value;
  tare->decimals = decimals;
  tare->unit = unit;
  return WSOFT_TARE_OK;
}

bool wsoft_tare_net(const WsoftTare &tare, int32_t gross, int decimals, int unit, int32_t *net) {
  if (!tare.on || tare.decimals != decimals || tare.unit != unit) {
    *net = gross;
    return false;
  }
  *net = gross - tare.value;
  return true;
}
