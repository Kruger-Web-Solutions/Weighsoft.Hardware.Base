// Host tests for the HX711 load cell rules (lib/wsoft_core/src/wsoft_loadcell.cpp).
// Run: pio test -e native -f test_wsoft_loadcell   (needs a host g++ on PATH)

#include <unity.h>

#include "wsoft_loadcell.h"

void setUp() {}
void tearDown() {}

static WsoftLoadcellCal calibrated(int32_t zero, int32_t at_weight, int32_t value, int decimals) {
  WsoftLoadcellCal cal;
  wsoft_loadcell_defaults(&cal);
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_format(&cal, decimals, 1, 1));
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
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_format(&cal, 3, 1, 5));
  int32_t value;
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, 200 * 12, &value));  // 12 digits
  TEST_ASSERT_EQUAL_INT32(10, value);
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, 200 * 13, &value));  // 13 digits
  TEST_ASSERT_EQUAL_INT32(15, value);
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, -200 * 13, &value));
  TEST_ASSERT_EQUAL_INT32(-15, value);
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_STEP, wsoft_loadcell_set_format(&cal, 3, 1, 3));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_STEP, wsoft_loadcell_set_format(&cal, 3, 1, 0));
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
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_format(&cal, 0, 3, 1));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_span(&cal, true, 500000, 500, 0, 3));
  int32_t value;
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, 250000, &value));
  TEST_ASSERT_EQUAL_INT32(250, value);
  TEST_ASSERT_EQUAL(0, cal.decimals);
  TEST_ASSERT_EQUAL(3, cal.unit);
}

void test_the_test_weight_is_kept_in_the_scales_own_digits() {
  // A new board shows 0.001 kg. "2" kg is stored as 2000, "2.5" as 2500, 500 g as 500.
  WsoftLoadcellCal cal;
  wsoft_loadcell_defaults(&cal);
  wsoft_loadcell_set_zero(&cal, true, 0);
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_span(&cal, true, 400000, 2, 0, 1));
  TEST_ASSERT_EQUAL_INT32(2000, cal.span_value);
  TEST_ASSERT_EQUAL(3, cal.decimals);
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_span(&cal, true, 400000, 25, 1, 1));
  TEST_ASSERT_EQUAL_INT32(2500, cal.span_value);
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_span(&cal, true, 400000, 500, 0, 3));
  TEST_ASSERT_EQUAL_INT32(500, cal.span_value);
  TEST_ASSERT_EQUAL(1, cal.unit);
  // More digits than the scale shows: refused, nothing changed.
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_DECIMALS, wsoft_loadcell_set_span(&cal, true, 400000, 20005, 4, 1));
  TEST_ASSERT_EQUAL_INT32(500, cal.span_value);
}

void test_the_scale_setup_sets_unit_decimals_and_division() {
  WsoftLoadcellCal cal = calibrated(0, 400000, 2000, 3);  // 2.000 kg
  int32_t value;
  // 0.002 kg division: the weight moves in twos.
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_format(&cal, 3, 1, 2));
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, 200600, &value));  // 1.003 kg
  TEST_ASSERT_EQUAL_INT32(1004, value);
  // 0.01 kg: the span is re-written (2.00), the weight is still right.
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_format(&cal, 2, 1, 1));
  TEST_ASSERT_EQUAL_INT32(200, cal.span_value);
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, 200000, &value));
  TEST_ASSERT_EQUAL_INT32(100, value);
  // To grams, 1 g division: 2000 g.
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_format(&cal, 0, 3, 1));
  TEST_ASSERT_EQUAL_INT32(2000, cal.span_value);
  TEST_ASSERT_EQUAL(3, cal.unit);
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(cal, 200000, &value));
  TEST_ASSERT_EQUAL_INT32(1000, value);
  // And back to kg with no decimals: 2 kg.
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_format(&cal, 0, 1, 1));
  TEST_ASSERT_EQUAL_INT32(2, cal.span_value);
}

void test_a_bad_scale_setup_changes_nothing() {
  WsoftLoadcellCal cal = calibrated(0, 400000, 2500, 3);  // 2.500 kg
  WsoftLoadcellCal before = cal;
  // 2.5 kg is not a whole number of kg.
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_FORMAT_SPAN, wsoft_loadcell_set_format(&cal, 0, 1, 1));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_DECIMALS, wsoft_loadcell_set_format(&cal, 5, 1, 1));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_DECIMALS, wsoft_loadcell_set_format(&cal, 3, 3, 1));  // 0.001 g is too fine
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_UNIT, wsoft_loadcell_set_format(&cal, 3, 2, 1));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_STEP, wsoft_loadcell_set_format(&cal, 3, 1, 3));
  TEST_ASSERT_EQUAL_INT32(before.span_value, cal.span_value);
  TEST_ASSERT_EQUAL(before.decimals, cal.decimals);
  TEST_ASSERT_EQUAL(before.unit, cal.unit);
  TEST_ASSERT_EQUAL(before.step, cal.step);
  // With no test weight yet, any valid setup goes.
  WsoftLoadcellCal fresh;
  wsoft_loadcell_defaults(&fresh);
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_format(&fresh, 0, 1, 50));
  TEST_ASSERT_EQUAL(50, fresh.step);
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
  TEST_ASSERT_EQUAL(10, (int)filter.count());  // the counts are kept; the average is the last 8
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

// A scale calibrated at 100 counts a division: zero 1000, 2.000 kg = 200000 counts above it.
static WsoftLoadcellCal steady_cal(int filter) {
  WsoftLoadcellCal cal;
  wsoft_loadcell_defaults(&cal);
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_zero(&cal, true, 1000));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_span(&cal, true, 201000, 2000, 3, 1));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_filter(&cal, filter));
  return cal;
}

void test_the_filter_setting_is_medium_until_set() {
  WsoftLoadcellCal cal;
  wsoft_loadcell_defaults(&cal);
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_FILTER_MEDIUM, cal.filter);
  TEST_ASSERT_EQUAL(1, (int)wsoft_loadcell_filter_window(WSOFT_LOADCELL_FILTER_OFF));
  TEST_ASSERT_EQUAL(4, (int)wsoft_loadcell_filter_window(WSOFT_LOADCELL_FILTER_LOW));
  TEST_ASSERT_EQUAL(12, (int)wsoft_loadcell_filter_window(WSOFT_LOADCELL_FILTER_MEDIUM));
  TEST_ASSERT_EQUAL(32, (int)wsoft_loadcell_filter_window(WSOFT_LOADCELL_FILTER_HIGH));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_filter(&cal, WSOFT_LOADCELL_FILTER_HIGH));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_FILTER_HIGH, cal.filter);
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_FILTER, wsoft_loadcell_set_filter(&cal, 4));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_FILTER, wsoft_loadcell_set_filter(&cal, -1));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_FILTER_HIGH, cal.filter);
}

void test_the_last_digit_is_held_on_the_edge_between_two_divisions() {
  WsoftLoadcellCal cal = steady_cal(WSOFT_LOADCELL_FILTER_MEDIUM);
  int32_t value = 0;
  // 5.4 divisions: shows 5.
  TEST_ASSERT_TRUE(wsoft_loadcell_weight_steady(cal, 1540, false, 0, &value));
  TEST_ASSERT_EQUAL_INT32(5, value);
  // 5.6 would round to 6, but 5 is on the screen and the load is only 0.6 away: stays 5.
  TEST_ASSERT_TRUE(wsoft_loadcell_weight_steady(cal, 1560, true, 5, &value));
  TEST_ASSERT_EQUAL_INT32(5, value);
  // Exactly 3/4 of a division away still holds; a little more lets go.
  TEST_ASSERT_TRUE(wsoft_loadcell_weight_steady(cal, 1575, true, 5, &value));
  TEST_ASSERT_EQUAL_INT32(5, value);
  TEST_ASSERT_TRUE(wsoft_loadcell_weight_steady(cal, 1580, true, 5, &value));
  TEST_ASSERT_EQUAL_INT32(6, value);
  // And back down: 6 holds until the load is under 5.25.
  TEST_ASSERT_TRUE(wsoft_loadcell_weight_steady(cal, 1530, true, 6, &value));
  TEST_ASSERT_EQUAL_INT32(6, value);
  TEST_ASSERT_TRUE(wsoft_loadcell_weight_steady(cal, 1520, true, 6, &value));
  TEST_ASSERT_EQUAL_INT32(5, value);
  // A real change is shown at once, rounded as always.
  TEST_ASSERT_TRUE(wsoft_loadcell_weight_steady(cal, 101000, true, 5, &value));
  TEST_ASSERT_EQUAL_INT32(1000, value);
  // Filter OFF: plain rounding, nothing held.
  cal.filter = WSOFT_LOADCELL_FILTER_OFF;
  TEST_ASSERT_TRUE(wsoft_loadcell_weight_steady(cal, 1560, true, 5, &value));
  TEST_ASSERT_EQUAL_INT32(6, value);
}

void test_the_hold_works_in_whole_divisions() {
  WsoftLoadcellCal cal = steady_cal(WSOFT_LOADCELL_FILTER_LOW);
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_format(&cal, 3, 1, 5));  // e = 0.005 kg
  int32_t value = 0;
  // 13 digits: 2.6 divisions, 0.6 from the 10 on the screen: stays 10.
  TEST_ASSERT_TRUE(wsoft_loadcell_weight_steady(cal, 2300, true, 10, &value));
  TEST_ASSERT_EQUAL_INT32(10, value);
  // 14 digits: 0.8 of a division away: moves to 15.
  TEST_ASSERT_TRUE(wsoft_loadcell_weight_steady(cal, 2400, true, 10, &value));
  TEST_ASSERT_EQUAL_INT32(15, value);
  // A held value that is not a whole division (the division was just changed) is not kept.
  TEST_ASSERT_TRUE(wsoft_loadcell_weight_steady(cal, 2300, true, 12, &value));
  TEST_ASSERT_EQUAL_INT32(15, value);
  // Not calibrated: never a weight.
  WsoftLoadcellCal blank;
  wsoft_loadcell_defaults(&blank);
  value = 7;
  TEST_ASSERT_FALSE(wsoft_loadcell_weight_steady(blank, 2300, true, 10, &value));
  TEST_ASSERT_EQUAL_INT32(7, value);
}

void test_the_jump_is_four_divisions_in_counts() {
  WsoftLoadcellCal cal = steady_cal(WSOFT_LOADCELL_FILTER_HIGH);
  TEST_ASSERT_EQUAL_INT32(400, wsoft_loadcell_filter_jump(cal));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_format(&cal, 3, 1, 5));
  TEST_ASSERT_EQUAL_INT32(2000, wsoft_loadcell_filter_jump(cal));
  cal.filter = WSOFT_LOADCELL_FILTER_OFF;
  TEST_ASSERT_EQUAL_INT32(0, wsoft_loadcell_filter_jump(cal));
  WsoftLoadcellCal blank;
  wsoft_loadcell_defaults(&blank);
  TEST_ASSERT_EQUAL_INT32(0, wsoft_loadcell_filter_jump(blank));
}

void test_the_filter_averages_the_window_it_is_given() {
  WsoftLoadcellFilter filter;
  int32_t average = 0;
  for (int i = 1; i <= 40; i++) filter.add(i * 10, 32);
  TEST_ASSERT_EQUAL(32, (int)filter.count());
  TEST_ASSERT_TRUE(filter.average(&average));
  TEST_ASSERT_EQUAL_INT32(245, average);  // 90..400: 245
  filter.add(410, 4);                     // 380, 390, 400, 410
  TEST_ASSERT_TRUE(filter.average(&average));
  TEST_ASSERT_EQUAL_INT32(395, average);
  filter.add(500, 1);                     // window 1 = the newest count
  TEST_ASSERT_TRUE(filter.average(&average));
  TEST_ASSERT_EQUAL_INT32(500, average);
  filter.add(600, 99);                    // a window that is too long is the longest
  TEST_ASSERT_TRUE(filter.average(&average));
  TEST_ASSERT_EQUAL_INT32(283, average);  // 120..410 (30 counts), 500, 600: 9050 / 32
}

void test_one_spike_is_dropped_and_a_new_load_is_followed_at_once() {
  WsoftLoadcellFilter filter;
  int32_t average = 0;
  for (int i = 0; i < 32; i++) filter.add(1000 + (i % 2), 32, 400);
  TEST_ASSERT_TRUE(filter.average(&average));
  TEST_ASSERT_EQUAL_INT32(1001, average);  // 1000.5 rounds away from zero
  // One count far off, then normal again: it never reaches the average.
  filter.add(9000, 32, 400);
  TEST_ASSERT_TRUE(filter.average(&average));
  TEST_ASSERT_EQUAL_INT32(1001, average);
  filter.add(1000, 32, 400);
  TEST_ASSERT_EQUAL(32, (int)filter.count());
  TEST_ASSERT_TRUE(filter.average(&average));
  TEST_ASSERT_TRUE(average == 1000 || average == 1001);
  // A spike up then a spike down are two spikes, not a new load.
  filter.add(9000, 32, 400);
  filter.add(-7000, 32, 400);
  TEST_ASSERT_EQUAL(32, (int)filter.count());
  filter.add(1000, 32, 400);
  // Two counts far off on the same side: a new load. The average starts again from those two.
  filter.add(50000, 32, 400);
  filter.add(50010, 32, 400);
  TEST_ASSERT_EQUAL(2, (int)filter.count());
  TEST_ASSERT_TRUE(filter.average(&average));
  TEST_ASSERT_EQUAL_INT32(50005, average);
  // A noisy scale: the counts swing 1000 apart, more than the jump of 400. A count inside
  // that swing is noise and is averaged in, twice in a row too; one well outside is a new load.
  WsoftLoadcellFilter noisy;
  for (int i = 0; i < 32; i++) noisy.add(i % 2 ? 2000 : 1000, 32, 0);
  TEST_ASSERT_EQUAL(32, (int)noisy.count());
  noisy.add(2000, 32, 400);
  noisy.add(2000, 32, 400);
  TEST_ASSERT_EQUAL(32, (int)noisy.count());
  noisy.add(9000, 32, 400);
  noisy.add(9100, 32, 400);
  TEST_ASSERT_EQUAL(2, (int)noisy.count());
  // No jump given (filter off, or not calibrated): plain averaging, nothing dropped.
  WsoftLoadcellFilter plain;
  plain.add(1000, 2, 0);
  plain.add(9000, 2, 0);
  TEST_ASSERT_TRUE(plain.average(&average));
  TEST_ASSERT_EQUAL_INT32(5000, average);
}

void test_the_noise_is_the_spread_of_the_counts_in_divisions() {
  WsoftLoadcellFilter filter;
  int32_t spread = 0, tenths = 0;
  filter.add(1000);
  TEST_ASSERT_FALSE(filter.spread(&spread));
  filter.add(1030);
  filter.add(990);
  TEST_ASSERT_TRUE(filter.spread(&spread));
  TEST_ASSERT_EQUAL_INT32(40, spread);
  WsoftLoadcellCal cal = steady_cal(WSOFT_LOADCELL_FILTER_MEDIUM);  // 100 counts a division
  TEST_ASSERT_TRUE(wsoft_loadcell_noise_tenths(cal, spread, &tenths));
  TEST_ASSERT_EQUAL_INT32(4, tenths);  // 0.4 of a division
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_format(&cal, 3, 1, 5));
  TEST_ASSERT_TRUE(wsoft_loadcell_noise_tenths(cal, 1000, &tenths));
  TEST_ASSERT_EQUAL_INT32(20, tenths);  // 1000 counts at 500 a division
  WsoftLoadcellCal blank;
  wsoft_loadcell_defaults(&blank);
  TEST_ASSERT_FALSE(wsoft_loadcell_noise_tenths(blank, spread, &tenths));
}

void test_a_fast_chip_gives_the_filter_ten_counts_a_second() {
  WsoftLoadcellBlock block;
  int32_t mean = 0;
  int blocks = 0;
  int64_t sum = 0;
  // 80 a second for one second: a count every 12.5 ms, values 0..79.
  for (int i = 0; i < 80; i++) {
    if (block.add(i, (uint32_t)(1000 + i * 25 / 2), &mean)) {
      if (blocks == 0) TEST_ASSERT_EQUAL_INT32(4, mean);  // 0..7: 3.5 rounds away from zero
      blocks++;
      sum += mean;
    }
  }
  TEST_ASSERT_EQUAL(9, blocks);  // the tenth is still open
  // 10 a second: every count is its own block, one count late.
  WsoftLoadcellBlock slow;
  TEST_ASSERT_FALSE(slow.add(500, 0, &mean));
  TEST_ASSERT_TRUE(slow.add(600, 100, &mean));
  TEST_ASSERT_EQUAL_INT32(500, mean);
  TEST_ASSERT_TRUE(slow.add(700, 195, &mean));  // a little early still counts
  TEST_ASSERT_EQUAL_INT32(600, mean);
  // The millisecond clock wrapping round does not break a block.
  WsoftLoadcellBlock wrap;
  TEST_ASSERT_FALSE(wrap.add(10, 0xFFFFFFF0u, &mean));
  TEST_ASSERT_FALSE(wrap.add(20, 0xFFFFFFFFu, &mean));
  TEST_ASSERT_TRUE(wrap.add(30, 0x00000060u, &mean));
  TEST_ASSERT_EQUAL_INT32(15, mean);
  wrap.reset();
  TEST_ASSERT_FALSE(wrap.add(30, 0x00001000u, &mean));
}

void test_auto_zero_follows_a_small_creep_and_leaves_a_load_alone() {
  WsoftLoadcellCal cal = steady_cal(WSOFT_LOADCELL_FILTER_MEDIUM);  // zero 1000, 100 counts a division
  WsoftLoadcellZeroTrack track;
  // Off: nothing moves.
  for (int i = 0; i < 100; i++) track.update(cal, 1150);
  TEST_ASSERT_EQUAL_INT32(0, track.offset());
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_zero_track(&cal, 2));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_ZERO_TRACK, wsoft_loadcell_set_zero_track(&cal, 4));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_ZERO_TRACK, wsoft_loadcell_set_zero_track(&cal, -1));
  TEST_ASSERT_EQUAL(2, cal.zero_track);
  // The empty scale sits 1.5 divisions high. Nothing for the first 2 seconds (20 counts) ...
  for (int i = 0; i < 20; i++) track.update(cal, 1150);
  TEST_ASSERT_EQUAL_INT32(0, track.offset());
  // ... then the zero follows: an eighth of what is left each count, all the way.
  track.update(cal, 1150);
  TEST_ASSERT_EQUAL_INT32(18, track.offset());  // 150 / 8
  for (int i = 0; i < 100; i++) track.update(cal, 1150);
  TEST_ASSERT_EQUAL_INT32(150, track.offset());
  // With that offset the scale shows 0 again.
  WsoftLoadcellCal now = cal;
  now.zero_raw += track.offset();
  int32_t value = 9;
  TEST_ASSERT_TRUE(wsoft_loadcell_weight(now, 1150, &value));
  TEST_ASSERT_EQUAL_INT32(0, value);
  // A load (10 divisions) is outside the band: left alone, however long it sits.
  for (int i = 0; i < 100; i++) track.update(cal, 2150);
  TEST_ASSERT_EQUAL_INT32(150, track.offset());
  // Back near zero, it waits its 2 seconds again before moving.
  for (int i = 0; i < 20; i++) track.update(cal, 1100);
  TEST_ASSERT_EQUAL_INT32(150, track.offset());
  for (int i = 0; i < 100; i++) track.update(cal, 1100);
  TEST_ASSERT_EQUAL_INT32(100, track.offset());
  // A creep that keeps going stops 20 divisions from the set zero.
  int32_t count = 1100;
  for (int i = 0; i < 5000; i++) {
    if (i % 10 == 0) count += 10;
    track.update(cal, count);
  }
  TEST_ASSERT_EQUAL_INT32(2000, track.offset());
  // Switched off, or a new zero (reset): the offset is gone.
  cal.zero_track = 0;
  track.update(cal, 1100);
  TEST_ASSERT_EQUAL_INT32(0, track.offset());
  // Not calibrated: nothing to follow.
  WsoftLoadcellCal blank;
  wsoft_loadcell_defaults(&blank);
  blank.zero_track = 3;
  for (int i = 0; i < 100; i++) track.update(blank, 5);
  TEST_ASSERT_EQUAL_INT32(0, track.offset());
}

// The weight Drift Guard lets through, in counts from the zero: what is shown before rounding.
static int32_t guarded(const WsoftLoadcellDriftGuard &guard, int32_t average) {
  return average - 1000 - guard.offset();
}

void test_drift_guard_holds_a_slow_wander_and_lets_a_real_change_through() {
  WsoftLoadcellCal cal = steady_cal(WSOFT_LOADCELL_FILTER_MEDIUM);  // zero 1000, 100 counts a division
  WsoftLoadcellDriftGuard guard;
  // Off: nothing is held.
  for (int i = 0; i < 100; i++) guard.update(cal, 6000 + i, 0);
  TEST_ASSERT_EQUAL_INT32(0, guard.offset());
  TEST_ASSERT_FALSE(guard.holding());
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_DRIFT_GUARD, wsoft_loadcell_set_drift_guard(&cal, 21));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_BAD_DRIFT_GUARD, wsoft_loadcell_set_drift_guard(&cal, -1));
  TEST_ASSERT_EQUAL(WSOFT_LOADCELL_OK, wsoft_loadcell_set_drift_guard(&cal, 3));
  // A load of 50 divisions, still for 2 seconds: now it holds.
  for (int i = 0; i <= 20; i++) guard.update(cal, 6000, 0);
  TEST_ASSERT_TRUE(guard.holding());
  // The slow wave from the bench: up 2 divisions over 10 seconds, down 4, back up 2.
  int32_t average = 6000;
  for (int i = 0; i < 100; i++) {
    guard.update(cal, average += 2, 0);
    TEST_ASSERT_EQUAL_INT32(5000, guarded(guard, average));
  }
  for (int i = 0; i < 200; i++) {
    guard.update(cal, average -= 2, 0);
    TEST_ASSERT_EQUAL_INT32(5000, guarded(guard, average));
  }
  TEST_ASSERT_EQUAL_INT32(-200, guard.offset());
  TEST_ASSERT_EQUAL_UINT32(1, guard.holds());
  TEST_ASSERT_EQUAL_UINT32(0, guard.jump_releases());
  // Something is put on (1 division in one count): it lets go at once and shows all of it.
  guard.update(cal, average += 100, 0);
  TEST_ASSERT_EQUAL_INT32(0, guard.offset());
  TEST_ASSERT_FALSE(guard.holding());
  TEST_ASSERT_EQUAL_INT32(average - 1000, guarded(guard, average));
  // It holds again only after 2 still seconds, at the new weight.
  for (int i = 0; i < 20; i++) guard.update(cal, average, 0);
  TEST_ASSERT_TRUE(guard.holding());
  TEST_ASSERT_EQUAL_INT32(0, guard.offset());
  // The scale is emptied: nothing held is carried to the empty scale.
  guard.update(cal, average += 3, 0);
  TEST_ASSERT_EQUAL_INT32(3, guard.offset());
  guard.update(cal, 1000, 0);
  TEST_ASSERT_EQUAL_INT32(0, guard.offset());
}

void test_drift_guard_never_hides_a_slow_top_up_past_its_limit() {
  // Hi-Lo packing: product is added slowly. What is shown may lag, never by more than the
  // limit (3 divisions = 300 counts), and it catches up in full every time it lets go.
  WsoftLoadcellCal cal = steady_cal(WSOFT_LOADCELL_FILTER_MEDIUM);
  cal.drift_guard = 3;
  WsoftLoadcellDriftGuard guard;
  int32_t average = 6000;
  for (int i = 0; i <= 20; i++) guard.update(cal, average, 0);
  TEST_ASSERT_TRUE(guard.holding());
  int releases = 0;
  bool was_holding = true;
  for (int i = 0; i < 3000; i++) {  // 5 minutes, 0.3 of a division a second: 90 divisions added
    guard.update(cal, average += 3, 0);
    int32_t hidden = guard.offset();
    TEST_ASSERT_TRUE(hidden >= 0 && hidden <= 300);
    if (was_holding && !guard.holding()) {
      releases++;
      TEST_ASSERT_EQUAL_INT32(average - 1000, guarded(guard, average));
    }
    was_holding = guard.holding();
  }
  TEST_ASSERT_TRUE(releases >= 20);
  TEST_ASSERT_EQUAL_UINT32((uint32_t)releases, guard.limit_releases());
  TEST_ASSERT_EQUAL_UINT32(0, guard.jump_releases());
  // A faster top-up (more than half a division a count) is never held at all.
  for (int i = 0; i < 100; i++) {
    guard.update(cal, average += 60, 0);
    TEST_ASSERT_EQUAL_INT32(0, guard.offset());
  }
}

void test_drift_guard_leaves_zero_to_auto_zero_and_forgets_on_reset() {
  WsoftLoadcellCal cal = steady_cal(WSOFT_LOADCELL_FILTER_MEDIUM);
  cal.drift_guard = 3;
  cal.zero_track = 2;
  WsoftLoadcellDriftGuard guard;
  // Empty, inside auto zero's band (2 divisions), creeping: Drift Guard holds nothing.
  int32_t average = 1000;
  for (int i = 0; i < 100; i++) {
    guard.update(cal, ++average, 0);
    TEST_ASSERT_EQUAL_INT32(0, guard.offset());
    TEST_ASSERT_FALSE(guard.holding());
  }
  // Auto zero off: the empty scale is held like any other steady weight.
  cal.zero_track = 0;
  guard.reset();
  for (int i = 0; i <= 21; i++) guard.update(cal, 1100, 0);
  for (int i = 0; i < 50; i++) guard.update(cal, 1100 + i, 0);
  TEST_ASSERT_EQUAL_INT32(49, guard.offset());
  // ZERO, a new calibration, a silent chip: reset, and nothing held is left.
  guard.reset();
  TEST_ASSERT_EQUAL_INT32(0, guard.offset());
  TEST_ASSERT_FALSE(guard.holding());
  // Not calibrated: never holds.
  WsoftLoadcellCal blank;
  wsoft_loadcell_defaults(&blank);
  blank.drift_guard = 3;
  for (int i = 0; i < 100; i++) guard.update(blank, 5 + i, 0);
  TEST_ASSERT_EQUAL_INT32(0, guard.offset());
}

void test_every_error_has_a_code_and_a_sentence() {
  for (int e = WSOFT_LOADCELL_OK; e <= WSOFT_LOADCELL_BAD_DRIFT_GUARD; e++) {
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
  RUN_TEST(test_the_test_weight_is_kept_in_the_scales_own_digits);
  RUN_TEST(test_the_scale_setup_sets_unit_decimals_and_division);
  RUN_TEST(test_a_bad_scale_setup_changes_nothing);
  RUN_TEST(test_an_out_of_range_count_or_a_huge_weight_is_never_a_weight);
  RUN_TEST(test_the_filter_averages_the_last_eight);
  RUN_TEST(test_the_filter_holds_minus_counts);
  RUN_TEST(test_the_filter_setting_is_medium_until_set);
  RUN_TEST(test_the_last_digit_is_held_on_the_edge_between_two_divisions);
  RUN_TEST(test_the_hold_works_in_whole_divisions);
  RUN_TEST(test_the_jump_is_four_divisions_in_counts);
  RUN_TEST(test_the_filter_averages_the_window_it_is_given);
  RUN_TEST(test_one_spike_is_dropped_and_a_new_load_is_followed_at_once);
  RUN_TEST(test_the_noise_is_the_spread_of_the_counts_in_divisions);
  RUN_TEST(test_a_fast_chip_gives_the_filter_ten_counts_a_second);
  RUN_TEST(test_auto_zero_follows_a_small_creep_and_leaves_a_load_alone);
  RUN_TEST(test_drift_guard_holds_a_slow_wander_and_lets_a_real_change_through);
  RUN_TEST(test_drift_guard_never_hides_a_slow_top_up_past_its_limit);
  RUN_TEST(test_drift_guard_leaves_zero_to_auto_zero_and_forgets_on_reset);
  RUN_TEST(test_every_error_has_a_code_and_a_sentence);
  return UNITY_END();
}
