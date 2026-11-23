// Host tests for the RS485 scale tuning (lib/wsoft_core/src/wsoft_rs485_tuning.cpp) and the
// stable range / time rule (WsoftStableSamples): one Scale setup for the junction box, 23 Nov 2026.
// Run: pio test -e native   (needs a host g++ on PATH)

#include <unity.h>

#include <string.h>

#include "wsoft_rs485_tuning.h"
#include "wsoft_scale_setup.h"

void setUp() {}
void tearDown() {}

void test_tuning_defaults_keep_the_old_rules() {
  WsoftRs485Tuning t;
  TEST_ASSERT_TRUE(wsoft_rs485_tuning_valid(t));
  TEST_ASSERT_EQUAL_INT32(0, t.stable_range);
  TEST_ASSERT_EQUAL_INT32(0, t.stable_time);
  TEST_ASSERT_EQUAL_INT32(0, t.zero_track_range);
  TEST_ASSERT_EQUAL_INT32(10, t.zero_track_time);
}

void test_tuning_limits_follow_the_m1x_manual() {
  WsoftRs485Tuning t;
  t.stable_range = 999;
  t.stable_time = 99;
  t.zero_track_range = 9999;
  t.zero_track_time = 99;
  TEST_ASSERT_TRUE(wsoft_rs485_tuning_valid(t));
  WsoftRs485Tuning bad = t;
  bad.stable_range = 1000;
  TEST_ASSERT_FALSE(wsoft_rs485_tuning_valid(bad));
  bad = t;
  bad.stable_time = 100;
  TEST_ASSERT_FALSE(wsoft_rs485_tuning_valid(bad));
  bad = t;
  bad.zero_track_range = 10000;
  TEST_ASSERT_FALSE(wsoft_rs485_tuning_valid(bad));
  bad = t;
  bad.zero_track_time = 0;  // the card takes 0.1 .. 9.9 s
  TEST_ASSERT_FALSE(wsoft_rs485_tuning_valid(bad));
  bad = t;
  bad.stable_range = -1;
  TEST_ASSERT_FALSE(wsoft_rs485_tuning_valid(bad));
}

void test_round_to_step_half_away_from_zero() {
  TEST_ASSERT_EQUAL_INT32(1234, wsoft_round_to_step(1234, 1));
  TEST_ASSERT_EQUAL_INT32(1234, wsoft_round_to_step(1234, 0));
  TEST_ASSERT_EQUAL_INT32(1234, wsoft_round_to_step(1234, 2));
  TEST_ASSERT_EQUAL_INT32(1236, wsoft_round_to_step(1235, 2));
  TEST_ASSERT_EQUAL_INT32(1235, wsoft_round_to_step(1233, 5));
  TEST_ASSERT_EQUAL_INT32(1230, wsoft_round_to_step(1232, 5));
  TEST_ASSERT_EQUAL_INT32(-1236, wsoft_round_to_step(-1235, 2));
  TEST_ASSERT_EQUAL_INT32(-1230, wsoft_round_to_step(-1232, 5));
  TEST_ASSERT_EQUAL_INT32(0, wsoft_round_to_step(-2, 5));
  TEST_ASSERT_EQUAL_INT32(500, wsoft_round_to_step(250, 500));
}

void test_stable_range_lets_a_small_wobble_count() {
  WsoftStableSamples exact, wobble;
  const int32_t readings[] = {1000, 1001, 999, 1001, 1000};
  for (int32_t r : readings) {
    exact.update(true, true, r);
    wobble.update(true, true, r, 1, 0);
  }
  TEST_ASSERT_FALSE(exact.stable(3));
  TEST_ASSERT_TRUE(wobble.stable(5));
  // More than the range starts a new run.
  wobble.update(true, true, 1003, 1, 0);
  TEST_ASSERT_EQUAL_INT(1, wobble.count());
  // A failed read breaks the run.
  wobble.update(true, false, 1003, 1, 0);
  TEST_ASSERT_EQUAL_INT(0, wobble.count());
}

void test_stable_time_must_pass() {
  WsoftStableSamples s;
  s.update(true, true, 500, 0, 10000);
  s.update(true, true, 500, 0, 10250);
  s.update(true, true, 500, 0, 10500);
  TEST_ASSERT_TRUE(s.stable(3));
  TEST_ASSERT_TRUE(s.stable_for(3, 0, 10500));    // no time rule
  TEST_ASSERT_TRUE(s.stable_for(3, 500, 10500));  // 0.5 s since the run began
  TEST_ASSERT_FALSE(s.stable_for(3, 600, 10500));
  TEST_ASSERT_TRUE(s.stable_for(3, 600, 10600));
  // A new run restarts the clock.
  s.update(true, true, 600, 0, 10700);
  TEST_ASSERT_FALSE(s.stable_for(1, 600, 11000));
  TEST_ASSERT_TRUE(s.stable_for(1, 600, 11300));
}

void test_zero_tracker_follows_a_creep_after_the_time() {
  WsoftZeroTracker z;
  // Off: nothing ever moves.
  TEST_ASSERT_FALSE(z.update(true, true, 2, 0, 1000, 0));
  TEST_ASSERT_FALSE(z.update(true, true, 2, 0, 1000, 5000));
  TEST_ASSERT_EQUAL_INT32(0, z.offset());
  // Range 3 digits, 1.0 s: the empty scale crept to +2.
  TEST_ASSERT_FALSE(z.update(true, true, 2, 3, 1000, 10000));
  TEST_ASSERT_FALSE(z.update(true, true, 2, 3, 1000, 10900));
  TEST_ASSERT_TRUE(z.update(true, true, 2, 3, 1000, 11000));
  TEST_ASSERT_EQUAL_INT32(2, z.offset());
  // A load bigger than the range is never taken away.
  TEST_ASSERT_FALSE(z.update(true, true, 50, 3, 1000, 20000));
  TEST_ASSERT_FALSE(z.update(true, true, 50, 3, 1000, 25000));
  TEST_ASSERT_EQUAL_INT32(2, z.offset());
  // Not stable or no signal: the wait starts again.
  TEST_ASSERT_FALSE(z.update(true, true, -1, 3, 1000, 30000));
  TEST_ASSERT_FALSE(z.update(true, false, -1, 3, 1000, 30500));
  TEST_ASSERT_FALSE(z.update(true, true, -1, 3, 1000, 31000));
  TEST_ASSERT_FALSE(z.update(true, true, -1, 3, 1000, 31500));
  TEST_ASSERT_TRUE(z.update(true, true, -1, 3, 1000, 32000));
  TEST_ASSERT_EQUAL_INT32(1, z.offset());
  z.reset();
  TEST_ASSERT_EQUAL_INT32(0, z.offset());
}

void test_zero_tracker_stops_at_its_reach() {
  WsoftZeroTracker z;
  uint32_t t = 0;
  // Range 1: reach is 20 digits. A slow creep of +1 at a time is followed 20 times, no further.
  for (int i = 0; i < 30; i++) {
    z.update(true, true, 1, 1, 100, t);
    t += 200;
    z.update(true, true, 1, 1, 100, t);
    t += 200;
  }
  TEST_ASSERT_EQUAL_INT32(20, z.offset());
}

void test_board_zero_and_rounding() {
  WsoftBoardZero zero;
  int32_t shown = 0;
  // No zero set: the card's weight, rounded to e.
  TEST_ASSERT_TRUE(wsoft_board_zero_apply(zero, 0, 1003, 3, 1, 2, &shown));
  TEST_ASSERT_EQUAL_INT32(1004, shown);
  zero.set = true;
  zero.raw = 37;
  zero.decimals = 3;
  zero.unit = 1;
  TEST_ASSERT_TRUE(wsoft_board_zero_apply(zero, 0, 37, 3, 1, 1, &shown));
  TEST_ASSERT_EQUAL_INT32(0, shown);
  TEST_ASSERT_TRUE(wsoft_board_zero_apply(zero, 0, 1037, 3, 1, 1, &shown));
  TEST_ASSERT_EQUAL_INT32(1000, shown);
  // Auto zero adds to it.
  TEST_ASSERT_TRUE(wsoft_board_zero_apply(zero, 2, 1039, 3, 1, 1, &shown));
  TEST_ASSERT_EQUAL_INT32(1000, shown);
  // A zero taken in other decimals or another unit is not used.
  TEST_ASSERT_TRUE(wsoft_board_zero_apply(zero, 0, 1037, 2, 1, 1, &shown));
  TEST_ASSERT_EQUAL_INT32(1037, shown);
  TEST_ASSERT_TRUE(wsoft_board_zero_apply(zero, 0, 1037, 3, 3, 1, &shown));
  TEST_ASSERT_EQUAL_INT32(1037, shown);
}

void test_zero_checks_in_screen_order() {
  TEST_ASSERT_EQUAL(WSOFT_ZERO_OK, wsoft_zero_check(true, false, false, true, true));
  TEST_ASSERT_EQUAL(WSOFT_ZERO_SOURCE, wsoft_zero_check(false, false, false, true, true));
  TEST_ASSERT_EQUAL(WSOFT_ZERO_APPROVAL, wsoft_zero_check(true, true, true, false, false));
  TEST_ASSERT_EQUAL(WSOFT_ZERO_TARE, wsoft_zero_check(true, false, true, false, false));
  TEST_ASSERT_EQUAL(WSOFT_ZERO_NO_SIGNAL, wsoft_zero_check(true, false, false, false, false));
  TEST_ASSERT_EQUAL(WSOFT_ZERO_NOT_STEADY, wsoft_zero_check(true, false, false, true, false));
  // The screen words fit under the ZERO button (17 letters).
  for (int e = WSOFT_ZERO_OK; e <= WSOFT_ZERO_SOURCE; e++) {
    TEST_ASSERT_TRUE(strlen(wsoft_zero_error_screen((WsoftZeroError)e)) <= 17);
  }
  TEST_ASSERT_EQUAL_STRING("APPROVAL IS ON", wsoft_zero_error_screen(WSOFT_ZERO_APPROVAL));
  TEST_ASSERT_EQUAL_STRING("not_steady", wsoft_zero_error_code(WSOFT_ZERO_NOT_STEADY));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_tuning_defaults_keep_the_old_rules);
  RUN_TEST(test_tuning_limits_follow_the_m1x_manual);
  RUN_TEST(test_round_to_step_half_away_from_zero);
  RUN_TEST(test_stable_range_lets_a_small_wobble_count);
  RUN_TEST(test_stable_time_must_pass);
  RUN_TEST(test_zero_tracker_follows_a_creep_after_the_time);
  RUN_TEST(test_zero_tracker_stops_at_its_reach);
  RUN_TEST(test_board_zero_and_rounding);
  RUN_TEST(test_zero_checks_in_screen_order);
  return UNITY_END();
}
