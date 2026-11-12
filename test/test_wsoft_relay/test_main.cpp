// Host tests for the Hi-Lo relay rules (lib/wsoft_core/src/wsoft_relay.cpp).
// Run: pio test -e native   (needs a host g++ on PATH)

#include <unity.h>

#include "wsoft_relay.h"

void setUp() {}
void tearDown() {}

// A good reading: linked, no alarm, product picked, load on the scale.
static WsoftRelayInput good(WsoftHiloState state) {
  WsoftRelayInput in;
  in.linked = true;
  in.picked = true;
  in.value = 7100;
  in.state = state;
  return in;
}

void test_zone_follows_the_hilo_check() {
  TEST_ASSERT_EQUAL(WSOFT_RELAY_LOW, wsoft_relay_want(good(WSOFT_HILO_LOW)));
  TEST_ASSERT_EQUAL(WSOFT_RELAY_OK, wsoft_relay_want(good(WSOFT_HILO_PASS)));
  TEST_ASSERT_EQUAL(WSOFT_RELAY_HIGH, wsoft_relay_want(good(WSOFT_HILO_HIGH)));
  TEST_ASSERT_EQUAL(WSOFT_RELAY_NONE, wsoft_relay_want(good(WSOFT_HILO_NONE)));
}

void test_anything_unsafe_is_none() {
  WsoftRelayInput in = good(WSOFT_HILO_PASS);
  in.linked = false;
  TEST_ASSERT_EQUAL_MESSAGE(WSOFT_RELAY_NONE, wsoft_relay_want(in), "no signal");
  in = good(WSOFT_HILO_PASS);
  in.alarm = true;
  TEST_ASSERT_EQUAL_MESSAGE(WSOFT_RELAY_NONE, wsoft_relay_want(in), "alarm");
  in = good(WSOFT_HILO_PASS);
  in.blanked = true;
  TEST_ASSERT_EQUAL_MESSAGE(WSOFT_RELAY_NONE, wsoft_relay_want(in), "blanked");
  in = good(WSOFT_HILO_PASS);
  in.picked = false;
  TEST_ASSERT_EQUAL_MESSAGE(WSOFT_RELAY_NONE, wsoft_relay_want(in), "no product");
  TEST_ASSERT_EQUAL_MESSAGE(WSOFT_RELAY_NONE, wsoft_relay_want(WsoftRelayInput()), "boot defaults");
}

void test_empty_scale_is_none_not_low() {
  // 0 kg is below every low limit; an empty scale must not hold the LOW relay on.
  WsoftRelayInput in = good(WSOFT_HILO_LOW);
  in.value = 0;
  TEST_ASSERT_EQUAL(WSOFT_RELAY_NONE, wsoft_relay_want(in));
  in.value = -5;
  TEST_ASSERT_EQUAL(WSOFT_RELAY_NONE, wsoft_relay_want(in));
  in.value = 1;
  TEST_ASSERT_EQUAL(WSOFT_RELAY_LOW, wsoft_relay_want(in));
}

void test_a_zone_needs_two_fresh_polls() {
  WsoftRelayHold hold;
  TEST_ASSERT_EQUAL(WSOFT_RELAY_NONE, wsoft_relay_step(&hold, WSOFT_RELAY_OK, true));
  TEST_ASSERT_EQUAL(WSOFT_RELAY_OK, wsoft_relay_step(&hold, WSOFT_RELAY_OK, true));
  TEST_ASSERT_EQUAL(WSOFT_RELAY_OK, wsoft_relay_step(&hold, WSOFT_RELAY_OK, true));
  // One poll over the limit: OK stays. Two: HIGH.
  TEST_ASSERT_EQUAL(WSOFT_RELAY_OK, wsoft_relay_step(&hold, WSOFT_RELAY_HIGH, true));
  TEST_ASSERT_EQUAL(WSOFT_RELAY_HIGH, wsoft_relay_step(&hold, WSOFT_RELAY_HIGH, true));
}

void test_a_weight_on_the_limit_does_not_chatter() {
  WsoftRelayHold hold;
  wsoft_relay_step(&hold, WSOFT_RELAY_OK, true);
  wsoft_relay_step(&hold, WSOFT_RELAY_OK, true);
  for (int i = 0; i < 20; i++) {
    TEST_ASSERT_EQUAL(WSOFT_RELAY_OK, wsoft_relay_step(&hold, WSOFT_RELAY_HIGH, true));
    TEST_ASSERT_EQUAL(WSOFT_RELAY_OK, wsoft_relay_step(&hold, WSOFT_RELAY_OK, true));
  }
  // LOW then HIGH is two different asks: neither has two in a row.
  TEST_ASSERT_EQUAL(WSOFT_RELAY_OK, wsoft_relay_step(&hold, WSOFT_RELAY_LOW, true));
  TEST_ASSERT_EQUAL(WSOFT_RELAY_OK, wsoft_relay_step(&hold, WSOFT_RELAY_HIGH, true));
  TEST_ASSERT_EQUAL(WSOFT_RELAY_HIGH, wsoft_relay_step(&hold, WSOFT_RELAY_HIGH, true));
}

void test_off_is_never_delayed() {
  WsoftRelayHold hold;
  wsoft_relay_step(&hold, WSOFT_RELAY_HIGH, true);
  wsoft_relay_step(&hold, WSOFT_RELAY_HIGH, true);
  TEST_ASSERT_EQUAL(WSOFT_RELAY_HIGH, hold.shown);
  // Even on a poll that is not fresh.
  TEST_ASSERT_EQUAL(WSOFT_RELAY_NONE, wsoft_relay_step(&hold, WSOFT_RELAY_NONE, false));
  // And the count starts again from nothing.
  TEST_ASSERT_EQUAL(WSOFT_RELAY_NONE, wsoft_relay_step(&hold, WSOFT_RELAY_HIGH, true));
  TEST_ASSERT_EQUAL(WSOFT_RELAY_HIGH, wsoft_relay_step(&hold, WSOFT_RELAY_HIGH, true));
}

void test_a_failed_poll_is_never_a_weight() {
  WsoftRelayHold hold;
  for (int i = 0; i < 10; i++) TEST_ASSERT_EQUAL(WSOFT_RELAY_NONE, wsoft_relay_step(&hold, WSOFT_RELAY_OK, false));
  wsoft_relay_step(&hold, WSOFT_RELAY_OK, true);
  // A stale poll between two fresh ones neither counts nor resets.
  TEST_ASSERT_EQUAL(WSOFT_RELAY_NONE, wsoft_relay_step(&hold, WSOFT_RELAY_OK, false));
  TEST_ASSERT_EQUAL(WSOFT_RELAY_OK, wsoft_relay_step(&hold, WSOFT_RELAY_OK, true));
  // Shown zone is kept through a stale poll.
  TEST_ASSERT_EQUAL(WSOFT_RELAY_OK, wsoft_relay_step(&hold, WSOFT_RELAY_HIGH, false));
}

void test_stuck_screen_task_goes_stale() {
  TEST_ASSERT_TRUE(wsoft_relay_fresh(1000, 1000));
  TEST_ASSERT_TRUE(wsoft_relay_fresh(1000, 1000 + WSOFT_RELAY_STALE_MS));
  TEST_ASSERT_FALSE(wsoft_relay_fresh(1000, 1001 + WSOFT_RELAY_STALE_MS));
  // millis() wraps after about 49 days.
  TEST_ASSERT_TRUE(wsoft_relay_fresh(0xFFFFFF00u, 0x00000010u));
  TEST_ASSERT_FALSE(wsoft_relay_fresh(0xFFFFFF00u, 0x00000010u + WSOFT_RELAY_STALE_MS));
}

void test_relay_map_rules() {
  TEST_ASSERT_TRUE(wsoft_relay_map_valid(1, 2, 3));
  TEST_ASSERT_TRUE(wsoft_relay_map_valid(4, 3, 1));
  TEST_ASSERT_TRUE(wsoft_relay_map_valid(0, 2, 0));
  TEST_ASSERT_TRUE(wsoft_relay_map_valid(0, 0, 0));
  TEST_ASSERT_FALSE(wsoft_relay_map_valid(1, 1, 3));
  TEST_ASSERT_FALSE(wsoft_relay_map_valid(1, 2, 1));
  TEST_ASSERT_FALSE(wsoft_relay_map_valid(1, 2, 2));
  TEST_ASSERT_FALSE(wsoft_relay_map_valid(5, 2, 3));
  TEST_ASSERT_FALSE(wsoft_relay_map_valid(1, 2, 255));
}

void test_one_relay_per_zone() {
  TEST_ASSERT_EQUAL_HEX8(0x01, wsoft_relay_mask(WSOFT_RELAY_LOW, 1, 2, 3));
  TEST_ASSERT_EQUAL_HEX8(0x02, wsoft_relay_mask(WSOFT_RELAY_OK, 1, 2, 3));
  TEST_ASSERT_EQUAL_HEX8(0x04, wsoft_relay_mask(WSOFT_RELAY_HIGH, 1, 2, 3));
  TEST_ASSERT_EQUAL_HEX8(0x00, wsoft_relay_mask(WSOFT_RELAY_NONE, 1, 2, 3));
  TEST_ASSERT_EQUAL_HEX8(0x08, wsoft_relay_mask(WSOFT_RELAY_LOW, 4, 3, 1));
  // A zone with no relay, and a bad map, switch nothing.
  TEST_ASSERT_EQUAL_HEX8(0x00, wsoft_relay_mask(WSOFT_RELAY_LOW, 0, 2, 3));
  TEST_ASSERT_EQUAL_HEX8(0x00, wsoft_relay_mask(WSOFT_RELAY_OK, 2, 2, 3));
  TEST_ASSERT_EQUAL_HEX8(0x07, wsoft_relay_owned(1, 2, 3));
  TEST_ASSERT_EQUAL_HEX8(0x02, wsoft_relay_owned(0, 2, 0));
  TEST_ASSERT_EQUAL_HEX8(0x00, wsoft_relay_owned(1, 1, 3));
}

void test_zone_words() {
  TEST_ASSERT_EQUAL_STRING("NONE", wsoft_relay_zone_text(WSOFT_RELAY_NONE));
  TEST_ASSERT_EQUAL_STRING("LOW", wsoft_relay_zone_text(WSOFT_RELAY_LOW));
  TEST_ASSERT_EQUAL_STRING("OK", wsoft_relay_zone_text(WSOFT_RELAY_OK));
  TEST_ASSERT_EQUAL_STRING("HIGH", wsoft_relay_zone_text(WSOFT_RELAY_HIGH));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_zone_follows_the_hilo_check);
  RUN_TEST(test_anything_unsafe_is_none);
  RUN_TEST(test_empty_scale_is_none_not_low);
  RUN_TEST(test_a_zone_needs_two_fresh_polls);
  RUN_TEST(test_a_weight_on_the_limit_does_not_chatter);
  RUN_TEST(test_off_is_never_delayed);
  RUN_TEST(test_a_failed_poll_is_never_a_weight);
  RUN_TEST(test_stuck_screen_task_goes_stale);
  RUN_TEST(test_relay_map_rules);
  RUN_TEST(test_one_relay_per_zone);
  RUN_TEST(test_zone_words);
  return UNITY_END();
}
