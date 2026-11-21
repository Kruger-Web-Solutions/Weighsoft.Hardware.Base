// Host tests for the ESP LED colours (lib/wsoft_core/src/wsoft_led.cpp).
// Run: pio test -e native   (needs a host g++ on PATH)

#include <unity.h>

#include "wsoft_led.h"

void setUp() {}
void tearDown() {}

static bool dark(const WsoftLedColour &c) {
  return c.red == 0 && c.green == 0 && c.blue == 0;
}

void test_under_is_orange_correct_is_green_over_is_red() {
  WsoftLedColour low = wsoft_led_colour(true, WSOFT_RELAY_LOW);
  TEST_ASSERT_TRUE(low.red > low.green && low.green > 0 && low.blue == 0);
  WsoftLedColour ok = wsoft_led_colour(true, WSOFT_RELAY_OK);
  TEST_ASSERT_TRUE(ok.green > 0 && ok.red == 0 && ok.blue == 0);
  WsoftLedColour high = wsoft_led_colour(true, WSOFT_RELAY_HIGH);
  TEST_ASSERT_TRUE(high.red > 0 && high.green == 0 && high.blue == 0);
}

void test_no_zone_is_dark() {
  TEST_ASSERT_TRUE(dark(wsoft_led_colour(true, WSOFT_RELAY_NONE)));
  TEST_ASSERT_TRUE(dark(wsoft_led_colour(true, (WsoftRelayZone)9)));
}

void test_switched_off_is_dark_in_every_zone() {
  TEST_ASSERT_TRUE(dark(wsoft_led_colour(false, WSOFT_RELAY_NONE)));
  TEST_ASSERT_TRUE(dark(wsoft_led_colour(false, WSOFT_RELAY_LOW)));
  TEST_ASSERT_TRUE(dark(wsoft_led_colour(false, WSOFT_RELAY_OK)));
  TEST_ASSERT_TRUE(dark(wsoft_led_colour(false, WSOFT_RELAY_HIGH)));
}

void test_the_three_colours_differ() {
  WsoftLedColour low = wsoft_led_colour(true, WSOFT_RELAY_LOW);
  WsoftLedColour ok = wsoft_led_colour(true, WSOFT_RELAY_OK);
  WsoftLedColour high = wsoft_led_colour(true, WSOFT_RELAY_HIGH);
  TEST_ASSERT_FALSE(wsoft_led_same(low, ok));
  TEST_ASSERT_FALSE(wsoft_led_same(ok, high));
  TEST_ASSERT_FALSE(wsoft_led_same(low, high));
  TEST_ASSERT_TRUE(wsoft_led_same(ok, ok));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_under_is_orange_correct_is_green_over_is_red);
  RUN_TEST(test_no_zone_is_dark);
  RUN_TEST(test_switched_off_is_dark_in_every_zone);
  RUN_TEST(test_the_three_colours_differ);
  return UNITY_END();
}
