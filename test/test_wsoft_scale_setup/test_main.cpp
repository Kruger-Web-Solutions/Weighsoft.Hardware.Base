// Host tests for the RS485 scale setup kept on the board (lib/wsoft_core/src/wsoft_scale_setup.cpp):
// A5 checks, complete / missing, blanking, below Min, stable samples and tare.
// Run: pio test -e native   (needs a host g++ on PATH)

#include <unity.h>

#include <string.h>

#include "wsoft_scale_setup.h"

void setUp() {}
void tearDown() {}

static WsoftCardFormat card(int32_t division, int decimal, int unit = 1) {
  WsoftCardFormat c;
  c.read = true;
  c.division = division;
  c.decimal = decimal;
  c.unit = unit;
  return c;
}

static const WsoftCardFormat HALF_KG = card(5, 1);  // e = 0.5 kg

static WsoftSetupText text(const char *value) {
  WsoftSetupText t;
  t.given = true;
  t.text = value;
  return t;
}

static WsoftSetupInt number(int64_t value) {
  WsoftSetupInt n;
  n.given = true;
  n.value = value;
  return n;
}

// A full, valid body for e = 0.5 kg.
static WsoftScaleSetupInput good() {
  WsoftScaleSetupInput in;
  in.sa_number = text("SA 1234/56");
  in.max_kg = text("3000");
  in.min_kg = text("10");
  in.blank_below_e = number(2);
  in.blank_above_e = number(9);
  in.stable_samples = number(4);
  in.has_tare_enabled = true;
  in.tare_enabled = true;
  in.tare_max_kg = text("500");
  in.preset_tare_kg = text("12.5");
  in.reason = "bench setup";
  return in;
}

static WsoftSetupError apply(const WsoftScaleSetupInput &in, WsoftScaleSetup *s, const WsoftCardFormat &c = HALF_KG) {
  bool changed = false;
  return wsoft_scale_setup_update(in, c, s, &changed);
}

void test_e_from_the_card() {
  int64_t e = 0;
  TEST_ASSERT_TRUE(wsoft_card_e(HALF_KG, &e));
  TEST_ASSERT_EQUAL(50000, e);
  TEST_ASSERT_TRUE(wsoft_card_e(card(2, 3), &e));  // 0.002 kg
  TEST_ASSERT_EQUAL(200, e);
  TEST_ASSERT_TRUE(wsoft_card_e(card(1, 2, 2), &e));  // 0.01 t = 10 kg
  TEST_ASSERT_EQUAL(1000000, e);
  TEST_ASSERT_TRUE(wsoft_card_e(card(1, 0, 3), &e));  // 1 g
  TEST_ASSERT_EQUAL(100, e);
  TEST_ASSERT_FALSE(wsoft_card_e(card(1, 4, 3), &e));  // 0.0001 g: finer than 0.00001 kg
  TEST_ASSERT_FALSE(wsoft_card_e(card(1, 0, 0), &e));  // no unit
  TEST_ASSERT_FALSE(wsoft_card_e(card(0, 0), &e));     // no division
  TEST_ASSERT_FALSE(wsoft_card_e(WsoftCardFormat(), &e));  // not read yet
}

void test_kg_text_both_ways() {
  int64_t kg = 0;
  TEST_ASSERT_TRUE(wsoft_kg_parse("", &kg));
  TEST_ASSERT_EQUAL(WSOFT_KG_UNSET, kg);
  TEST_ASSERT_TRUE(wsoft_kg_parse("12.5", &kg));
  TEST_ASSERT_EQUAL(1250000, kg);
  TEST_ASSERT_TRUE(wsoft_kg_parse("99999.99999", &kg));
  TEST_ASSERT_EQUAL(WSOFT_KG_MAX_UNITS, kg);
  TEST_ASSERT_TRUE(wsoft_kg_parse("0", &kg));
  TEST_ASSERT_EQUAL(0, kg);
  TEST_ASSERT_FALSE(wsoft_kg_parse("1.123456", &kg));  // 6 decimals
  TEST_ASSERT_FALSE(wsoft_kg_parse("100000", &kg));    // 6 integer digits
  TEST_ASSERT_FALSE(wsoft_kg_parse("-1", &kg));
  TEST_ASSERT_FALSE(wsoft_kg_parse("1.", &kg));
  TEST_ASSERT_FALSE(wsoft_kg_parse(".5", &kg));
  TEST_ASSERT_FALSE(wsoft_kg_parse("1,5", &kg));
  TEST_ASSERT_FALSE(wsoft_kg_parse(" 1", &kg));
  TEST_ASSERT_FALSE(wsoft_kg_parse(nullptr, &kg));
  char out[24];
  wsoft_kg_text(1250000, out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("12.5", out);
  wsoft_kg_text(300000000, out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("3000", out);
  wsoft_kg_text(1, out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("0.00001", out);
  wsoft_kg_text(WSOFT_KG_UNSET, out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("", out);
}

void test_reading_to_kg_and_back() {
  int64_t kg = 0;
  TEST_ASSERT_TRUE(wsoft_reading_to_kg(30005, 1, 1, &kg));  // 3000.5 kg
  TEST_ASSERT_EQUAL(300050000, kg);
  TEST_ASSERT_TRUE(wsoft_reading_to_kg(-15, 1, 2, &kg));    // -1.5 t
  TEST_ASSERT_EQUAL(-150000000, kg);
  TEST_ASSERT_TRUE(wsoft_reading_to_kg(1520, 0, 3, &kg));   // 1520 g
  TEST_ASSERT_EQUAL(152000, kg);
  TEST_ASSERT_FALSE(wsoft_reading_to_kg(1, 4, 3, &kg));     // 0.0001 g
  int32_t value = 0;
  TEST_ASSERT_TRUE(wsoft_kg_to_reading(1250000, 1, 1, &value));
  TEST_ASSERT_EQUAL(125, value);
  TEST_ASSERT_FALSE(wsoft_kg_to_reading(1250000, 0, 1, &value));  // 12.5 is not whole kg
}

void test_good_body_is_saved_and_complete() {
  WsoftScaleSetup s;
  TEST_ASSERT_FALSE(wsoft_scale_setup_complete(s, HALF_KG));
  bool changed = false;
  TEST_ASSERT_EQUAL(WSOFT_SETUP_OK, wsoft_scale_setup_update(good(), HALF_KG, &s, &changed));
  TEST_ASSERT_TRUE(changed);
  TEST_ASSERT_EQUAL_STRING("SA 1234/56", s.sa_number);
  TEST_ASSERT_EQUAL(300000000, s.max_kg);
  TEST_ASSERT_EQUAL(1000000, s.min_kg);
  TEST_ASSERT_EQUAL(2, s.blank_below_e);
  TEST_ASSERT_EQUAL(4, s.stable_samples);
  TEST_ASSERT_EQUAL(50000000, s.tare_max_kg);
  TEST_ASSERT_EQUAL(1250000, s.preset_tare_kg);
  TEST_ASSERT_TRUE(wsoft_scale_setup_complete(s, HALF_KG));
  // The same body again changes nothing.
  TEST_ASSERT_EQUAL(WSOFT_SETUP_OK, wsoft_scale_setup_update(good(), HALF_KG, &s, &changed));
  TEST_ASSERT_FALSE(changed);
}

void test_missing_fields_keep_their_value() {
  WsoftScaleSetup s;
  apply(good(), &s);
  WsoftScaleSetupInput only;
  only.reason = "new number";
  only.sa_number = text("SA 9");
  TEST_ASSERT_EQUAL(WSOFT_SETUP_OK, apply(only, &s));
  TEST_ASSERT_EQUAL_STRING("SA 9", s.sa_number);
  TEST_ASSERT_EQUAL(300000000, s.max_kg);
}

void test_defaults() {
  WsoftScaleSetup s;
  TEST_ASSERT_EQUAL(9, s.blank_above_e);
  TEST_ASSERT_EQUAL(3, s.stable_samples);
  TEST_ASSERT_TRUE(s.tare_enabled);
  TEST_ASSERT_EQUAL(WSOFT_BLANK_UNSET, s.blank_below_e);
  TEST_ASSERT_EQUAL(WSOFT_KG_UNSET, s.max_kg);
}

void test_reason_is_required() {
  WsoftScaleSetup s;
  WsoftScaleSetupInput in = good();
  in.reason = "  ";
  TEST_ASSERT_EQUAL(WSOFT_SETUP_REASON_REQUIRED, apply(in, &s));
  in.reason = nullptr;
  TEST_ASSERT_EQUAL(WSOFT_SETUP_REASON_REQUIRED, apply(in, &s));
  TEST_ASSERT_EQUAL(WSOFT_KG_UNSET, s.max_kg);  // nothing applied
  TEST_ASSERT_EQUAL_STRING("REASON_REQUIRED", wsoft_setup_error_code(WSOFT_SETUP_REASON_REQUIRED));
  // The board answers BUSY when the setup lock is not free in time (A2's code).
  TEST_ASSERT_EQUAL_STRING("BUSY", wsoft_setup_error_code(WSOFT_SETUP_BUSY));
}

void test_number_errors() {
  WsoftScaleSetup s;
  WsoftScaleSetupInput in = good();
  in.max_kg = text("3000.25");  // not a multiple of 0.5
  TEST_ASSERT_EQUAL(WSOFT_SETUP_NUMBER, apply(in, &s));
  in = good();
  in.min_kg = text("ten");
  TEST_ASSERT_EQUAL(WSOFT_SETUP_NUMBER, apply(in, &s));
  in = good();
  in.preset_tare_kg = text("1.000001");
  TEST_ASSERT_EQUAL(WSOFT_SETUP_NUMBER, apply(in, &s));
  in = good();
  in.tare_max_kg.bad_type = true;
  TEST_ASSERT_EQUAL(WSOFT_SETUP_NUMBER, apply(in, &s));
  TEST_ASSERT_EQUAL(WSOFT_KG_UNSET, s.max_kg);
  TEST_ASSERT_EQUAL_STRING("SETUP_NUMBER", wsoft_setup_error_code(WSOFT_SETUP_NUMBER));
}

void test_order_errors() {
  WsoftScaleSetup s;
  WsoftScaleSetupInput in = good();
  in.max_kg = text("0.5");  // max must be more than e
  in.min_kg = text("");
  in.tare_max_kg = text("");
  in.preset_tare_kg = text("");
  TEST_ASSERT_EQUAL(WSOFT_SETUP_ORDER, apply(in, &s));
  in = good();
  in.min_kg = text("0");  // min must be at least e
  TEST_ASSERT_EQUAL(WSOFT_SETUP_ORDER, apply(in, &s));
  in = good();
  in.min_kg = text("3000");  // min must be below max
  TEST_ASSERT_EQUAL(WSOFT_SETUP_ORDER, apply(in, &s));
  TEST_ASSERT_EQUAL_STRING("SETUP_ORDER", wsoft_setup_error_code(WSOFT_SETUP_ORDER));
}

void test_text_errors() {
  WsoftScaleSetup s;
  WsoftScaleSetupInput in = good();
  in.sa_number = text("123456789012345678901234567890123");  // 33
  TEST_ASSERT_EQUAL(WSOFT_SETUP_TEXT, apply(in, &s));
  in.sa_number = text("tab\there");
  TEST_ASSERT_EQUAL(WSOFT_SETUP_TEXT, apply(in, &s));
  in.sa_number.bad_type = true;
  TEST_ASSERT_EQUAL(WSOFT_SETUP_TEXT, apply(in, &s));
  in.sa_number = text("  SA 7  ");
  TEST_ASSERT_EQUAL(WSOFT_SETUP_OK, apply(in, &s));
  TEST_ASSERT_EQUAL_STRING("SA 7", s.sa_number);  // trimmed
  in.sa_number = text("");  // A5: 1 to 32 characters, so an empty one is refused
  TEST_ASSERT_EQUAL(WSOFT_SETUP_TEXT, apply(in, &s));
  in.sa_number = text("   ");
  TEST_ASSERT_EQUAL(WSOFT_SETUP_TEXT, apply(in, &s));
  TEST_ASSERT_EQUAL_STRING("SA 7", s.sa_number);  // kept
}

void test_range_errors() {
  WsoftScaleSetup s;
  WsoftScaleSetupInput in = good();
  in.stable_samples = number(2);
  TEST_ASSERT_EQUAL(WSOFT_SETUP_RANGE, apply(in, &s));
  in.stable_samples = number(21);
  TEST_ASSERT_EQUAL(WSOFT_SETUP_RANGE, apply(in, &s));
  in = good();
  in.blank_below_e = number(101);
  TEST_ASSERT_EQUAL(WSOFT_SETUP_RANGE, apply(in, &s));
  in = good();
  in.blank_above_e = number(-1);
  TEST_ASSERT_EQUAL(WSOFT_SETUP_RANGE, apply(in, &s));
  in = good();
  in.blank_above_e.null = true;  // only blank_below_e may be null
  TEST_ASSERT_EQUAL(WSOFT_SETUP_RANGE, apply(in, &s));
  in = good();
  in.stable_samples.bad_type = true;
  TEST_ASSERT_EQUAL(WSOFT_SETUP_RANGE, apply(in, &s));
  in = good();
  in.tare_max_kg = text("3000.5");  // more than max
  TEST_ASSERT_EQUAL(WSOFT_SETUP_RANGE, apply(in, &s));
  in = good();
  in.preset_tare_kg = text("500.5");  // more than tare_max
  TEST_ASSERT_EQUAL(WSOFT_SETUP_RANGE, apply(in, &s));
  in = good();
  in.tare_max_kg = text("");
  in.preset_tare_kg = text("3000.5");  // no tare_max: Max is the limit
  TEST_ASSERT_EQUAL(WSOFT_SETUP_RANGE, apply(in, &s));
  in = good();
  in.has_tare_enabled = true;
  in.tare_enabled_bad_type = true;
  TEST_ASSERT_EQUAL(WSOFT_SETUP_RANGE, apply(in, &s));
  in = good();
  in.blank_below_e.null = true;  // allowed: not set
  TEST_ASSERT_EQUAL(WSOFT_SETUP_OK, apply(in, &s));
  TEST_ASSERT_EQUAL(WSOFT_BLANK_UNSET, s.blank_below_e);
}

void test_missing_keys_in_order() {
  WsoftScaleSetup s;
  const char *keys[5];
  TEST_ASSERT_EQUAL(5, wsoft_scale_setup_missing(s, WsoftCardFormat(), keys));
  TEST_ASSERT_EQUAL_STRING("sa_number", keys[0]);
  TEST_ASSERT_EQUAL_STRING("max_kg", keys[1]);
  TEST_ASSERT_EQUAL_STRING("min_kg", keys[2]);
  TEST_ASSERT_EQUAL_STRING("blank_below_e", keys[3]);
  TEST_ASSERT_EQUAL_STRING("card", keys[4]);
  apply(good(), &s);
  TEST_ASSERT_EQUAL(0, wsoft_scale_setup_missing(s, HALF_KG, keys));
  TEST_ASSERT_EQUAL(1, wsoft_scale_setup_missing(s, WsoftCardFormat(), keys));
  TEST_ASSERT_EQUAL_STRING("card", keys[0]);
  // The card's e changed to 20 kg: Max 3000 still fits, Min 10 does not (min >= e).
  TEST_ASSERT_EQUAL(1, wsoft_scale_setup_missing(s, card(20, 0), keys));
  TEST_ASSERT_EQUAL_STRING("min_kg", keys[0]);
  // e = 7 kg: neither 3000 nor 10 is a multiple.
  TEST_ASSERT_EQUAL(2, wsoft_scale_setup_missing(s, card(7, 0), keys));
  TEST_ASSERT_EQUAL_STRING("max_kg", keys[0]);
  TEST_ASSERT_EQUAL_STRING("min_kg", keys[1]);
}

void test_card_not_read_skips_the_e_checks() {
  WsoftScaleSetup s;
  WsoftScaleSetupInput in = good();
  in.max_kg = text("3000.25");
  TEST_ASSERT_EQUAL(WSOFT_SETUP_OK, apply(in, &s, WsoftCardFormat()));
  TEST_ASSERT_FALSE(wsoft_scale_setup_complete(s, HALF_KG));  // checked again once the card is read
  in.min_kg = text("4000");  // min < max is still checked
  TEST_ASSERT_EQUAL(WSOFT_SETUP_ORDER, apply(in, &s, WsoftCardFormat()));
}

void test_blanking() {
  WsoftScaleSetup s;
  apply(good(), &s);  // e 0.5 kg, max 3000, blank below 2e, above 9e
  // Readings with 1 decimal in kg (the card's format).
  TEST_ASSERT_EQUAL(WSOFT_HOLD_NONE, wsoft_scale_hold(s, HALF_KG, -10, -10, 1, 1, true));      // -1.0 = -2e
  TEST_ASSERT_EQUAL(WSOFT_HOLD_BLANK, wsoft_scale_hold(s, HALF_KG, -15, -15, 1, 1, true));     // -1.5
  TEST_ASSERT_EQUAL(WSOFT_HOLD_NONE, wsoft_scale_hold(s, HALF_KG, 30045, 30045, 1, 1, true));    // 3004.5 = max + 9e
  TEST_ASSERT_EQUAL(WSOFT_HOLD_BLANK, wsoft_scale_hold(s, HALF_KG, 30050, 30050, 1, 1, true));   // 3005.0
  TEST_ASSERT_EQUAL(WSOFT_HOLD_BLANK, wsoft_scale_hold(s, HALF_KG, 30050, 30050, 1, 1, false));  // blank wins
  // No blank_below_e yet: no blanking under zero.
  s.blank_below_e = WSOFT_BLANK_UNSET;
  TEST_ASSERT_EQUAL(WSOFT_HOLD_NONE, wsoft_scale_hold(s, HALF_KG, -1000, -1000, 1, 1, true));
  // Card not read: nothing can be checked.
  TEST_ASSERT_EQUAL(WSOFT_HOLD_NONE, wsoft_scale_hold(s, WsoftCardFormat(), 99999, 99999, 1, 1, true));
}

void test_below_min_and_samples() {
  WsoftScaleSetup s;
  apply(good(), &s);  // min 10 kg
  TEST_ASSERT_EQUAL(WSOFT_HOLD_BELOW_MIN, wsoft_scale_hold(s, HALF_KG, 95, 95, 1, 1, true));  // 9.5
  TEST_ASSERT_EQUAL(WSOFT_HOLD_NONE, wsoft_scale_hold(s, HALF_KG, 100, 100, 1, 1, true));      // 10.0
  TEST_ASSERT_EQUAL(WSOFT_HOLD_NONE, wsoft_scale_hold(s, HALF_KG, 0, 0, 1, 1, true));        // empty is not "below Min"
  TEST_ASSERT_EQUAL(WSOFT_HOLD_BELOW_MIN, wsoft_scale_hold(s, HALF_KG, 95, 95, 1, 1, false));
  TEST_ASSERT_EQUAL(WSOFT_HOLD_NOT_STABLE, wsoft_scale_hold(s, HALF_KG, 100, 100, 1, 1, false));
  TEST_ASSERT_EQUAL(WSOFT_HOLD_NOT_STABLE, wsoft_scale_hold(s, WsoftCardFormat(), 100, 100, 1, 1, false));
}

// Below Min is checked on the NET load when a tare is on (stricter; to confirm against
// the SA rule). Blanking stays on the gross.
void test_below_min_uses_the_net_load() {
  WsoftScaleSetup s;
  apply(good(), &s);  // min 10 kg, max 3000
  TEST_ASSERT_EQUAL(WSOFT_HOLD_BELOW_MIN, wsoft_scale_hold(s, HALF_KG, 150, 90, 1, 1, true));  // 15 gross, 9 net
  TEST_ASSERT_EQUAL(WSOFT_HOLD_NONE, wsoft_scale_hold(s, HALF_KG, 200, 100, 1, 1, true));      // 10 net
  TEST_ASSERT_EQUAL(WSOFT_HOLD_NONE, wsoft_scale_hold(s, HALF_KG, 60, 0, 1, 1, true));         // tared, empty
  TEST_ASSERT_EQUAL(WSOFT_HOLD_BLANK, wsoft_scale_hold(s, HALF_KG, 30050, 20000, 1, 1, true)); // gross over
}

// With a complete setup, a reading that cannot be turned into kg is not trusted: blank.
void test_reading_that_is_not_kg_is_blank() {
  WsoftScaleSetup s;
  apply(good(), &s);
  TEST_ASSERT_TRUE(wsoft_scale_setup_complete(s, HALF_KG));
  TEST_ASSERT_EQUAL(WSOFT_HOLD_BLANK, wsoft_scale_hold(s, HALF_KG, 1, 1, 7, 1, true));    // 1e-7 kg
  TEST_ASSERT_EQUAL(WSOFT_HOLD_BLANK, wsoft_scale_hold(s, HALF_KG, 100, 100, 1, 99, true));  // unknown unit
  WsoftScaleSetup empty;  // not complete: nothing to check yet
  TEST_ASSERT_EQUAL(WSOFT_HOLD_NONE, wsoft_scale_hold(empty, HALF_KG, 1, 1, 7, 1, true));
}

void test_stable_samples_counter() {
  WsoftStableSamples c;
  c.update(true, true, 100);
  c.update(true, true, 100);
  TEST_ASSERT_FALSE(c.stable(3));
  c.update(true, true, 100);
  TEST_ASSERT_TRUE(c.stable(3));
  TEST_ASSERT_EQUAL(3, c.count());
  c.update(true, true, 101);  // changed: starts again
  TEST_ASSERT_EQUAL(1, c.count());
  c.update(true, true, 101);
  c.update(true, false, 101);  // a failed poll starts again
  TEST_ASSERT_EQUAL(0, c.count());
  c.update(false, true, 101);
  TEST_ASSERT_EQUAL(0, c.count());
  c.update(true, true, 101);
  TEST_ASSERT_EQUAL(1, c.count());
}

void test_tare_button() {
  WsoftScaleSetup s;
  apply(good(), &s);  // tare_max 500 kg
  WsoftTare t;
  TEST_ASSERT_EQUAL(WSOFT_TARE_NOT_STABLE, wsoft_tare_take(s, false, 1200, 1, 1, &t));
  TEST_ASSERT_FALSE(t.on);
  TEST_ASSERT_EQUAL(WSOFT_TARE_TOO_BIG, wsoft_tare_take(s, true, 5005, 1, 1, &t));  // 500.5 kg
  TEST_ASSERT_EQUAL(WSOFT_TARE_EMPTY, wsoft_tare_take(s, true, 0, 1, 1, &t));
  TEST_ASSERT_EQUAL(WSOFT_TARE_OK, wsoft_tare_take(s, true, 5000, 1, 1, &t));       // 500.0 kg
  TEST_ASSERT_TRUE(t.on);
  int32_t net = 0;
  TEST_ASSERT_TRUE(wsoft_tare_net(t, 7500, 1, 1, &net));
  TEST_ASSERT_EQUAL(2500, net);  // net = gross - tare
  TEST_ASSERT_FALSE(wsoft_tare_net(t, 750, 0, 1, &net));  // decimals changed: no tare
  TEST_ASSERT_EQUAL(750, net);
  s.tare_enabled = false;
  WsoftTare off;
  TEST_ASSERT_EQUAL(WSOFT_TARE_OFF, wsoft_tare_take(s, true, 100, 1, 1, &off));
  TEST_ASSERT_EQUAL_STRING("TARE_NOT_STABLE", wsoft_tare_error_code(WSOFT_TARE_NOT_STABLE));
  TEST_ASSERT_EQUAL_STRING("TARE_TOO_BIG", wsoft_tare_error_code(WSOFT_TARE_TOO_BIG));
}

void test_tare_limit_falls_back_to_max() {
  WsoftScaleSetup s;
  WsoftScaleSetupInput in = good();
  in.tare_max_kg = text("");
  in.preset_tare_kg = text("");
  apply(in, &s);
  WsoftTare t;
  TEST_ASSERT_EQUAL(WSOFT_TARE_OK, wsoft_tare_take(s, true, 30000, 1, 1, &t));
  TEST_ASSERT_EQUAL(WSOFT_TARE_TOO_BIG, wsoft_tare_take(s, true, 30005, 1, 1, &t));
}

void test_preset_tare() {
  WsoftScaleSetup s;
  apply(good(), &s);  // preset 12.5 kg
  WsoftTare t;
  TEST_ASSERT_EQUAL(WSOFT_TARE_OK, wsoft_tare_preset(s, 1, 1, &t));
  TEST_ASSERT_EQUAL(125, t.value);
  TEST_ASSERT_EQUAL(1, t.decimals);
  TEST_ASSERT_EQUAL(WSOFT_TARE_EMPTY, wsoft_tare_preset(s, 0, 1, &t));  // 12.5 is not whole kg
  s.preset_tare_kg = WSOFT_KG_UNSET;
  TEST_ASSERT_EQUAL(WSOFT_TARE_EMPTY, wsoft_tare_preset(s, 1, 1, &t));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_e_from_the_card);
  RUN_TEST(test_kg_text_both_ways);
  RUN_TEST(test_reading_to_kg_and_back);
  RUN_TEST(test_good_body_is_saved_and_complete);
  RUN_TEST(test_missing_fields_keep_their_value);
  RUN_TEST(test_defaults);
  RUN_TEST(test_reason_is_required);
  RUN_TEST(test_number_errors);
  RUN_TEST(test_order_errors);
  RUN_TEST(test_text_errors);
  RUN_TEST(test_range_errors);
  RUN_TEST(test_missing_keys_in_order);
  RUN_TEST(test_card_not_read_skips_the_e_checks);
  RUN_TEST(test_blanking);
  RUN_TEST(test_below_min_and_samples);
  RUN_TEST(test_below_min_uses_the_net_load);
  RUN_TEST(test_reading_that_is_not_kg_is_blank);
  RUN_TEST(test_stable_samples_counter);
  RUN_TEST(test_tare_button);
  RUN_TEST(test_tare_limit_falls_back_to_max);
  RUN_TEST(test_preset_tare);
  return UNITY_END();
}
