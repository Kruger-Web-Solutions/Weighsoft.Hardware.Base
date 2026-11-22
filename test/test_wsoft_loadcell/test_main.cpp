// Host tests for the HX711 load cell rules (lib/wsoft_core/src/wsoft_loadcell.cpp).
// Run: pio test -e native -f test_wsoft_loadcell   (needs a host g++ on PATH)

#include <unity.h>

#include "wsoft_loadcell.h"

void setUp() {}
void tearDown() {}

static WsoftLoadcellCal calibrated(int32_t zero, int32_t at_weight, int32_t value, int decimals) {
  WsoftLoadcellCal cal;
  wsoft_loadcell_defaults(&cal);
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_zero(&cal, true, zero));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_span(&cal, true, at_weight, value, decimals, 1));
  return cal;
}

void test_decode_is_24_bit_twos_complement() {
  TEST_ASSERT_EQUAL_INT32(0, wsoft_hx711_decode(0x000000));
  TEST_ASSERT_EQUAL_INT32(1, wsoft_hx711_decode(0x000001));
  TEST_ASSERT_EQUAL_INT32(8388607, wsoft_hx711_decode(0x7FFFFF));
  TEST_ASSERT_EQUAL_INT32(-8388608, wsoft_hx711_decode(0x800000));
  TEST_ASSERT_EQUAL_INT32(-1, wsoft_hx711_decode(0xFFFFFF));
  // Bits above the 24th are not part of the count.
  TEST_ASSERT_EQUAL_INT32(-1, wsoft_hx711_decode(0xFFFFFFFF));
}

void test_the_two_end_values_are_out_of_range() {
  TEST_ASSERT_TRUE(wsoft_hx711_saturated(8388607));
  TEST_ASSERT_TRUE(wsoft_hx711_saturated(-8388608));
  TEST_ASSERT_FALSE(wsoft_hx711_saturated(8388606));
  TEST_ASSERT_FALSE(wsoft_hx711_saturated(-8388607));
  TEST_ASSERT_FALSE(wsoft_hx711_saturated(0));
}

void test_a_new_board_is_not_calibrated_and_gives_no_weight() {
  WsoftLoadcellCal cal;
  wsoft_loadcell_defaults(&cal);
  int32_t value = 77;
  TEST_ASSERT_FALSE(wsoft_loadcell_calibrated(cal));
  TEST_ASSERT_FALSE(wsoft_loadcell_weight(cal, 123456, &value));
  TEST_ASSERT_EQUAL_INT32(77, value);
  // Zero alone is not enough.
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_zero(&cal, true, 1000));
  TEST_ASSERT_FALSE(wsoft_loadcell_calibrated(cal));
  TEST_ASSERT_FALSE(wsoft_loadcell_weight(cal, 123456, &value));
}

void test_zero_and_test_weight_give_the_weight() {
  // Empty = 84000 counts, 2.000 kg = 484000 counts: 200 counts per gram.
  WsoftLoadcellCal cal = calibrated(84000, 484000, 2000, 3);
  int32_t value = -1;
  TEST_ASSERT_TRUE(wsoft_loadcell_calibrated(cal));
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, 84000, &value));
  TEST_ASSERT_EQUAL_INT32(0, value);
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, 484000, &value));
  TEST_ASSERT_EQUAL_INT32(2000, value);
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, 284000, &value));
  TEST_ASSERT_EQUAL_INT32(1000, value);
  // Below zero is a minus weight, never hidden here.
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, 64000, &value));
  TEST_ASSERT_EQUAL_INT32(-100, value);
  TEST_ASSERT_EQUAL(3, cal.decimals);
  TEST_ASSERT_EQUAL(1, cal.unit);
}

void test_rounding_is_half_away_from_zero() {
  WsoftLoadcellCal cal = calibrated(0, 200000, 1000, 3);  // 200 counts per digit
  int32_t value;
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, 99, &value));
  TEST_ASSERT_EQUAL_INT32(0, value);
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, 100, &value));
  TEST_ASSERT_EQUAL_INT32(1, value);
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, -100, &value));
  TEST_ASSERT_EQUAL_INT32(-1, value);
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, -99, &value));
  TEST_ASSERT_EQUAL_INT32(0, value);
}

void test_a_load_cell_wired_the_other_way_round_still_weighs() {
  // Counts fall as the load rises (A+ and A- swapped): the span is negative, the weight is not.
  WsoftLoadcellCal cal = calibrated(500000, 100000, 2000, 3);
  int32_t value;
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, 300000, &value));
  TEST_ASSERT_EQUAL_INT32(1000, value);
}

void test_the_step_rounds_the_shown_weight() {
  WsoftLoadcellCal cal = calibrated(0, 200000, 1000, 3);
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_step(&cal, 5));
  int32_t value;
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, 200 * 12, &value));  // 12 digits
  TEST_ASSERT_EQUAL_INT32(10, value);
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, 200 * 13, &value));  // 13 digits
  TEST_ASSERT_EQUAL_INT32(15, value);
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, -200 * 13, &value));
  TEST_ASSERT_EQUAL_INT32(-15, value);
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_STEP, wsoft_loadcell_set_step(&cal, 3));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_STEP, wsoft_loadcell_set_step(&cal, 0));
  TEST_ASSERT_EQUAL(5, cal.step);
}

void test_span_needs_zero_first() {
  WsoftLoadcellCal cal;
  wsoft_loadcell_defaults(&cal);
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_NO_ZERO, wsoft_loadcell_set_span(&cal, true, 400000, 2000, 3, 1));
  TEST_ASSERT_FALSE(cal.span_set);
}

void test_span_with_nothing_on_the_scale_is_refused() {
  WsoftLoadcellCal cal;
  wsoft_loadcell_defaults(&cal);
  wsoft_loadcell_set_zero(&cal, true, 84000);
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_TOO_SMALL, wsoft_loadcell_set_span(&cal, true, 84000 + 999, 2000, 3, 1));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_TOO_SMALL, wsoft_loadcell_set_span(&cal, true, 84000 - 999, 2000, 3, 1));
  TEST_ASSERT_FALSE(cal.span_set);
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_span(&cal, true, 84000 + 1000, 2000, 3, 1));
  TEST_ASSERT_TRUE(cal.span_set);
}

void test_bad_span_input_changes_nothing() {
  WsoftLoadcellCal cal = calibrated(84000, 484000, 2000, 3);
  WsoftLoadcellCal before = cal;
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_WEIGHT, wsoft_loadcell_set_span(&cal, true, 900000, 0, 3, 1));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_WEIGHT, wsoft_loadcell_set_span(&cal, true, 900000, -5, 3, 1));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_WEIGHT, wsoft_loadcell_set_span(&cal, true, 900000, 10000000, 3, 1));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_DECIMALS, wsoft_loadcell_set_span(&cal, true, 900000, 2000, 5, 1));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_DECIMALS, wsoft_loadcell_set_span(&cal, true, 900000, 2000, -1, 1));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_UNIT, wsoft_loadcell_set_span(&cal, true, 900000, 2000, 3, 2));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_UNIT, wsoft_loadcell_set_span(&cal, true, 900000, 2000, 3, 0));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_NO_READING, wsoft_loadcell_set_span(&cal, false, 900000, 2000, 3, 1));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_NO_READING, wsoft_loadcell_set_span(&cal, true, 8388607, 2000, 3, 1));
  TEST_ASSERT_EQUAL_INT32(before.span_counts, cal.span_counts);
  TEST_ASSERT_EQUAL_INT32(before.span_value, cal.span_value);
  TEST_ASSERT_EQUAL(before.decimals, cal.decimals);
  TEST_ASSERT_EQUAL(before.unit, cal.unit);
}

void test_zero_needs_a_real_reading() {
  WsoftLoadcellCal cal;
  wsoft_loadcell_defaults(&cal);
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_NO_READING, wsoft_loadcell_set_zero(&cal, false, 5));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_NO_READING, wsoft_loadcell_set_zero(&cal, true, -8388608));
  TEST_ASSERT_FALSE(cal.zero_set);
}

void test_a_new_zero_keeps_the_span() {
  WsoftLoadcellCal cal = calibrated(84000, 484000, 2000, 3);
  // The empty scale drifted by 1000 counts: zero again, and the same load weighs the same.
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_zero(&cal, true, 85000));
  int32_t value;
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, 85000, &value));
  TEST_ASSERT_EQUAL_INT32(0, value);
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, 485000, &value));
  TEST_ASSERT_EQUAL_INT32(2000, value);
}

void test_grams_and_no_decimals() {
  WsoftLoadcellCal cal;
  wsoft_loadcell_defaults(&cal);
  wsoft_loadcell_set_zero(&cal, true, 0);
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_span(&cal, true, 500000, 500, 0, 3));
  int32_t value;
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, 250000, &value));
  TEST_ASSERT_EQUAL_INT32(250, value);
  TEST_ASSERT_EQUAL(0, cal.decimals);
  TEST_ASSERT_EQUAL(3, cal.unit);
}

void test_an_out_of_range_count_or_a_huge_weight_is_never_a_weight() {
  WsoftLoadcellCal cal = calibrated(0, 1000, 9999999, 3);
  int32_t value = 5;
  TEST_ASSERT_FALSE(wsoft_loadcell_weight(cal, 8388607, &value));
  TEST_ASSERT_FALSE(wsoft_loadcell_weight(cal, 2000, &value));  // twice the biggest value
  TEST_ASSERT_EQUAL_INT32(5, value);
}

void test_the_filter_averages_the_last_eight() {
  WsoftLoadcellFilter filter;
  int32_t average = 9;
  TEST_ASSERT_FALSE(filter.average(&average));
  filter.add(100);
  TEST_ASSERT_TRUE(filter.average(&average));
  TEST_ASSERT_EQUAL_INT32(100, average);
  filter.add(200);
  TEST_ASSERT_TRUE(filter.average(&average));
  TEST_ASSERT_EQUAL_INT32(150, average);
  for (int i = 0; i < 8; i++) filter.add(1000);
  TEST_ASSERT_EQUAL(8, (int)filter.count());
  TEST_ASSERT_TRUE(filter.average(&average));
  TEST_ASSERT_EQUAL_INT32(1000, average);
  filter.add(1800);  // the oldest 1000 drops out
  TEST_ASSERT_TRUE(filter.average(&average));
  TEST_ASSERT_EQUAL_INT32(1100, average);
  filter.reset();
  TEST_ASSERT_FALSE(filter.average(&average));
}

void test_the_filter_holds_minus_counts() {
  WsoftLoadcellFilter filter;
  filter.add(-3);
  filter.add(-4);
  int32_t average;
  TEST_ASSERT_TRUE(filter.average(&average));
  TEST_ASSERT_EQUAL_INT32(-4, average);  // -3.5 rounds away from zero
}

void test_every_error_has_a_code_and_a_sentence() {
  for (int e = WSOFT_LOADCELL_OK; e <= WSOFT_LOADCELL_TOO_SMALL; e++) {
    TEST_ASSERT_NOT_NULL(wsoft_loadcell_error_code((WsoftLoadcellError)e));
    TEST_ASSERT_TRUE(wsoft_loadcell_error_text((WsoftLoadcellError)e)[0] != '\0');
  }
  TEST_ASSERT_EQUAL_STRING("zero_first", wsoft_loadcell_error_code(WSOFT_LOADCELL_NO_ZERO));
  TEST_ASSERT_EQUAL_STRING("too_small", wsoft_loadcell_error_code(WSOFT_LOADCELL_TOO_SMALL));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_decode_is_24_bit_twos_complement);
  RUN_TEST(test_the_two_end_values_are_out_of_range);
  RUN_TEST(test_a_new_board_is_not_calibrated_and_gives_no_weight);
  RUN_TEST(test_zero_and_test_weight_give_the_weight);
  RUN_TEST(test_rounding_is_half_away_from_zero);
  RUN_TEST(test_a_load_cell_wired_the_other_way_round_still_weighs);
  RUN_TEST(test_the_step_rounds_the_shown_weight);
  RUN_TEST(test_span_needs_zero_first);
  RUN_TEST(test_span_with_nothing_on_the_scale_is_refused);
  RUN_TEST(test_bad_span_input_changes_nothing);
  RUN_TEST(test_zero_needs_a_real_reading);
  RUN_TEST(test_a_new_zero_keeps_the_span);
  RUN_TEST(test_grams_and_no_decimals);
  RUN_TEST(test_an_out_of_range_count_or_a_huge_weight_is_never_a_weight);
  RUN_TEST(test_the_filter_averages_the_last_eight);
  RUN_TEST(test_the_filter_holds_minus_counts);
  RUN_TEST(test_every_error_has_a_code_and_a_sentence);
  return UNITY_END();
}
