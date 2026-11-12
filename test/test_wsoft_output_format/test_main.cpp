// Host tests for the Writer's weight formatter (lib/wsoft_core/src/wsoft_output_format.cpp).
// Run: pio test -e native   (needs a host g++ on PATH)

#include <unity.h>

#include <string.h>

#include "wsoft_output_format.h"

void setUp() {}
void tearDown() {}

static WsoftOutputFields sample() {
  WsoftOutputFields f;
  f.line = "12.345 kg";
  f.weight = "12.345";
  f.unit = "kg";
  f.has_stable = true;
  f.stable = true;
  return f;
}

void test_default_sends_the_line_unchanged() {
  char out[64];
  TEST_ASSERT_TRUE(wsoft_format_output(nullptr, sample(), out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("12.345 kg", out);
  TEST_ASSERT_TRUE(wsoft_format_output("", sample(), out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("12.345 kg", out);
  TEST_ASSERT_TRUE(wsoft_format_output(WSOFT_OUTPUT_FORMAT_DEFAULT, sample(), out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("12.345 kg", out);
}

void test_fields_are_replaced() {
  char out[64];
  TEST_ASSERT_TRUE(wsoft_format_output("{stable},GS,{weight}{unit}", sample(), out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("ST,GS,12.345kg", out);
  WsoftOutputFields moving = sample();
  moving.stable = false;
  TEST_ASSERT_TRUE(wsoft_format_output("W={weight} {stable}", moving, out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("W=12.345 US", out);
}

void test_unknown_or_open_braces_are_copied() {
  char out[64];
  TEST_ASSERT_TRUE(wsoft_format_output("{tare} {weight", sample(), out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("{tare} {weight", out);
}

void test_missing_stable_flag_is_empty_not_invented() {
  WsoftOutputFields f = sample();
  f.has_stable = false;  // an RS232 weigh board sends only the line and the weight
  char out[64];
  TEST_ASSERT_TRUE(wsoft_format_output("[{stable}]{weight}", f, out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("[]12.345", out);
}

void test_no_weight_is_never_sent_as_a_weight() {
  WsoftOutputFields f = sample();
  f.weight = "";
  char out[64];
  TEST_ASSERT_FALSE(wsoft_format_output("{weight} kg", f, out, sizeof out));
  TEST_ASSERT_TRUE(wsoft_format_output("{line}", f, out, sizeof out));  // the raw line needs no weight
}

void test_too_small_is_cut_and_reported() {
  char out[6];
  TEST_ASSERT_FALSE(wsoft_format_output("{line}", sample(), out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("12.34", out);
  TEST_ASSERT_FALSE(wsoft_format_output("{line}", sample(), out, 0));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_default_sends_the_line_unchanged);
  RUN_TEST(test_fields_are_replaced);
  RUN_TEST(test_unknown_or_open_braces_are_copied);
  RUN_TEST(test_missing_stable_flag_is_empty_not_invented);
  RUN_TEST(test_no_weight_is_never_sent_as_a_weight);
  RUN_TEST(test_too_small_is_cut_and_reported);
  return UNITY_END();
}
