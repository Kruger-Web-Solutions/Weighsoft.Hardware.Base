// Host tests for weighings sent to the Hub (lib/wsoft_core/src/wsoft_weighing.cpp).
// Run: pio test -e native   (needs a host g++ on PATH)

#include <unity.h>

#include <stdio.h>
#include <string.h>

#include "wsoft_weighing.h"

void setUp() {}
void tearDown() {}

static const char *kg(int32_t value, int decimals, int unit) {
  static char out[24];
  if (!wsoft_weight_kg_text(value, decimals, unit, out, sizeof out)) return "REFUSED";
  return out;
}

void test_kg_text_for_each_unit() {
  TEST_ASSERT_EQUAL_STRING("1520", kg(1520, 0, 1));
  TEST_ASSERT_EQUAL_STRING("1.234", kg(1234, 3, 1));
  TEST_ASSERT_EQUAL_STRING("1.5", kg(1500, 3, 1));   // trailing zeros dropped
  TEST_ASSERT_EQUAL_STRING("1520", kg(1520, 3, 2));  // 1.520 t
  TEST_ASSERT_EQUAL_STRING("15200", kg(152, 1, 2));  // 15.2 t
  TEST_ASSERT_EQUAL_STRING("1.52", kg(1520, 0, 3));  // 1520 g
  TEST_ASSERT_EQUAL_STRING("0.0152", kg(152, 1, 3));  // 15.2 g
  TEST_ASSERT_EQUAL_STRING("-2820", kg(-2820, 0, 1));
}

void test_kg_text_refuses_what_the_hub_would_reject() {
  TEST_ASSERT_EQUAL_STRING("REFUSED", kg(100000, 0, 1));  // over 99999.99999
  TEST_ASSERT_EQUAL_STRING("REFUSED", kg(101000, 3, 2));  // 101 t
  TEST_ASSERT_EQUAL_STRING("REFUSED", kg(1, 3, 3));       // 0.000001 kg, 6 decimals
  TEST_ASSERT_EQUAL_STRING("REFUSED", kg(10, 0, 0));      // unknown unit
  TEST_ASSERT_EQUAL_STRING("99999", kg(99999, 0, 1));
}

static TradeWeight weight(uint32_t seq, int32_t value, int decimals, int unit, int64_t at) {
  TradeWeight w;
  w.valid = true;
  w.seq = seq;
  w.value = value;
  w.decimals = decimals;
  w.unit = unit;
  w.at = at;
  return w;
}

static const WsoftWeighingMeta META = {"6CC8404560F4", "rs232", "wsoft-cyd28", false};

void test_json_matches_the_hub_contract() {
  char out[512];
  TEST_ASSERT_TRUE(wsoft_weighing_json(weight(7, 3120, 0, 1, 1762520400), META, out, sizeof out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"weighing_id\":\"6CC8404560F4-1762520400-7\",\"committed_at\":\"2025-11-07T13:00:00Z\","
      "\"gross_kg\":3120,\"tare_kg\":0,\"net_kg\":3120,\"basis\":\"GS\","
      "\"payload\":{\"seq\":7,\"reading\":\"3120\",\"unit\":\"kg\",\"source\":\"rs232\","
      "\"station_model\":\"wsoft-cyd28\",\"for_trade\":false}}",
      out);
}

void test_json_keeps_decimals_and_for_trade() {
  char out[512];
  WsoftWeighingMeta meta = META;
  meta.for_trade = true;
  TEST_ASSERT_TRUE(wsoft_weighing_json(weight(1, 1234, 3, 1, 1762520400), meta, out, sizeof out));
  TEST_ASSERT_TRUE(strstr(out, "\"gross_kg\":1.234,") != nullptr);
  TEST_ASSERT_TRUE(strstr(out, "\"reading\":\"1.234\"") != nullptr);
  TEST_ASSERT_TRUE(strstr(out, "\"for_trade\":true") != nullptr);
}

void test_json_with_a_tare_sends_gross_tare_net_and_nt() {
  char out[512];
  WsoftWeighingMeta meta = META;
  meta.tared = true;
  meta.tare_value = 1205;  // 120.5 kg, same decimals as the reading
  TEST_ASSERT_TRUE(wsoft_weighing_json(weight(9, 31205, 1, 1, 1762520400), meta, out, sizeof out));
  TEST_ASSERT_TRUE(strstr(out, "\"gross_kg\":3120.5,\"tare_kg\":120.5,\"net_kg\":3000,\"basis\":\"NT\",") != nullptr);
  // A tare bigger than the gross gives a negative net, which the Hub takes.
  TEST_ASSERT_TRUE(wsoft_weighing_json(weight(9, 1000, 1, 1, 1762520400), meta, out, sizeof out));
  TEST_ASSERT_TRUE(strstr(out, "\"net_kg\":-20.5,") != nullptr);
  // A tare of zero or less is not a tare.
  meta.tare_value = 0;
  TEST_ASSERT_FALSE(wsoft_weighing_json(weight(9, 31205, 1, 1, 1762520400), meta, out, sizeof out));
}

void test_json_carries_the_hi_lo_result_on_the_net_weight() {
  char out[900];
  WsoftHiloStamp stamp;
  stamp.on = true;
  strcpy(stamp.code, "MAIZE");
  stamp.target_g = 50000;
  stamp.low_g = 49000;
  stamp.high_g = 51000;
  stamp.state = WSOFT_HILO_PASS;
  WsoftWeighingMeta meta = META;
  meta.tared = true;
  meta.tare_value = 5;  // 0.5 kg bag
  meta.hilo = &stamp;
  TEST_ASSERT_TRUE(wsoft_weighing_json(weight(3, 505, 1, 1, 1762520400), meta, out, sizeof out));
  TEST_ASSERT_TRUE(strstr(out,
                          "\"for_trade\":false},\"hi_lo\":{\"state\":\"PASS\",\"reading_kg\":50,\"target_kg\":50,"
                          "\"low_limit_kg\":49,\"high_limit_kg\":51,\"deadband_kg\":0,\"tolerance\":{\"source\":\"hub-hilo-v1\"},"
                          "\"auto_saved\":false,\"product_code\":\"MAIZE\"}}") != nullptr);
  // No product picked: no block, same body as before.
  stamp.on = false;
  TEST_ASSERT_TRUE(wsoft_weighing_json(weight(3, 505, 1, 1, 1762520400), meta, out, sizeof out));
  TEST_ASSERT_TRUE(strstr(out, "hi_lo") == nullptr);
  TEST_ASSERT_TRUE(strstr(out, "\"for_trade\":false}}") != nullptr);
}

void test_json_refuses_unsendable_weighings() {
  char out[512];
  TEST_ASSERT_FALSE(wsoft_weighing_json(TradeWeight(), META, out, sizeof out));                 // not valid
  TEST_ASSERT_FALSE(wsoft_weighing_json(weight(1, 0, 0, 1, 1762520400), META, out, sizeof out));  // empty
  TEST_ASSERT_FALSE(wsoft_weighing_json(weight(1, 50, 0, 1, 1000), META, out, sizeof out));       // no clock
  TEST_ASSERT_FALSE(wsoft_weighing_json(weight(1, 100000, 0, 1, 1762520400), META, out, sizeof out));
  WsoftWeighingMeta bad = META;
  bad.device_tag = "a\"b";
  TEST_ASSERT_FALSE(wsoft_weighing_json(weight(1, 50, 0, 1, 1762520400), bad, out, sizeof out));
  TEST_ASSERT_FALSE(wsoft_weighing_json(weight(1, 50, 0, 1, 1762520400), META, out, 40));  // too small
}

// Feeds `count` readings 250 ms apart; returns how many weighings were saved.
struct Bench {
  WsoftWeighingSaver saver{1};
  uint32_t t = 0;
  int64_t now = 1762520400;
  TradeWeight issued;
  WsoftSaveResult last = WSOFT_SAVE_NONE;
  bool limits = false;  // a Hi-Lo product of 3000 to 3200 kg is picked

  WsoftHiloState state(int32_t value) const {
    if (!limits) return WSOFT_HILO_NONE;
    return value < 3000 ? WSOFT_HILO_LOW : value > 3200 ? WSOFT_HILO_HIGH : WSOFT_HILO_PASS;
  }

  int feed(int32_t value, int count, bool auto_save, bool linked = true) {
    int saved = 0;
    for (int i = 0; i < count; i++, t += 250) {
      last = saver.update({true, linked, true, false, value, 0, 1, t, now}, auto_save, false, false, &issued,
                           WSOFT_SAVE_NONE, state(value));
      if (last == WSOFT_SAVE_SAVED) saved++;
    }
    return saved;
  }
  WsoftSaveResult tap(int32_t value, bool queue_full = false) {
    t += 250;
    last = saver.update({true, true, true, false, value, 0, 1, t, now}, false, true, queue_full, &issued, WSOFT_SAVE_NONE,
                         state(value));
    return last;
  }
};

void test_auto_save_once_per_settled_load() {
  Bench b;
  b.feed(0, 4, true);
  TEST_ASSERT_EQUAL(1, b.feed(3120, 20, true));
  TEST_ASSERT_EQUAL_UINT32(1, b.saver.last().seq);
  TEST_ASSERT_EQUAL(0, b.feed(3120, 20, true));  // still on the scale: no second save
  b.feed(0, 4, true);                            // emptied
  TEST_ASSERT_EQUAL(1, b.feed(3120, 20, true));  // same weight, new load
  TEST_ASSERT_EQUAL(0, b.feed(3200, 20, true));  // weight changed, but no 0 in between: no save
  b.feed(0, 4, true);
  TEST_ASSERT_EQUAL(1, b.feed(3200, 20, true));
  TEST_ASSERT_EQUAL_UINT32(4, b.saver.next_seq());
}

// Rhyno, 8 Nov 2026: no second weighing (auto save or tap) until the scale went back to 0.
void test_a_second_weighing_needs_the_scale_back_at_zero() {
  Bench b;
  b.feed(0, 4, false);
  b.feed(3120, 8, false);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_SAVED, b.tap(3120));
  b.feed(3500, 8, false);  // more added, never back at 0
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NEED_ZERO, b.tap(3500));
  TEST_ASSERT_EQUAL_STRING("Take load off", wsoft_save_result_text(b.last));
  b.feed(40, 8, false);  // nearly empty is not empty
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NEED_ZERO, b.tap(40));
  b.feed(0, 2, false, false);  // a 0 with no live signal does not count
  b.feed(3500, 8, false);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NEED_ZERO, b.tap(3500));
  b.feed(0, 1, false);  // one live 0 reading is enough
  b.feed(3500, 8, false);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_SAVED, b.tap(3500));
}

void test_after_boot_the_scale_must_show_zero_first() {
  Bench b;  // board just started with a load already on
  TEST_ASSERT_EQUAL(0, b.feed(3120, 20, true));
  b.feed(3120, 8, false);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NEED_ZERO, b.tap(3120));
  b.feed(0, 4, true);
  TEST_ASSERT_EQUAL(1, b.feed(3120, 20, true));
}

void test_auto_save_off_saves_nothing_by_itself() {
  Bench b;
  b.feed(0, 4, false);
  TEST_ASSERT_EQUAL(0, b.feed(3120, 40, false));
}

void test_tap_saves_a_settled_load_once() {
  Bench b;
  b.feed(0, 4, false);
  b.feed(3120, 8, false);  // 2 s: settled
  TEST_ASSERT_EQUAL(WSOFT_SAVE_SAVED, b.tap(3120));
  TEST_ASSERT_EQUAL(3120, b.issued.value);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_ALREADY, b.tap(3120));
  TEST_ASSERT_EQUAL_STRING("Already saved", wsoft_save_result_text(b.last));
}

void test_tap_explains_why_it_cannot_save() {
  Bench b;
  b.feed(0, 8, false);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_EMPTY, b.tap(0));
  b.feed(3120, 1, false);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NOT_STABLE, b.tap(3120));  // only 0.5 s unchanged
  b.feed(3120, 1, false, false);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NOT_STABLE, b.tap(3120));  // stability restarts after a lost signal
  b.feed(3120, 8, false);
  b.now = 1000;  // clock not set
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NO_CLOCK, b.tap(3120));
  b.now = 1762520400;
  TEST_ASSERT_EQUAL(WSOFT_SAVE_FULL, b.tap(3120, true));
  TEST_ASSERT_EQUAL(WSOFT_SAVE_SAVED, b.tap(3120));
}

void test_no_signal() {
  Bench b;
  b.feed(3120, 8, false, false);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NO_SIGNAL, b.saver.update({true, false, true, false, 3120, 0, 1, b.t, b.now}, false, true, false, &b.issued));
}

void test_a_held_reading_is_not_saved() {
  Bench b;
  b.feed(0, 4, false);
  b.feed(31205, 8, false);  // settled
  b.t += 250;
  // Blanked: refused with its own words, and auto save does nothing.
  TEST_ASSERT_EQUAL(WSOFT_SAVE_BLANK, b.saver.update({true, true, true, false, 31205, 0, 1, b.t, b.now}, true, true,
                                                     false, &b.issued, WSOFT_SAVE_BLANK));
  TEST_ASSERT_FALSE(b.issued.valid);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NONE, b.saver.update({true, true, true, false, 31205, 0, 1, b.t, b.now}, true, false,
                                                    false, &b.issued, WSOFT_SAVE_BLANK));
  // Below Min (only passed in while Approval is ON) and too few stable samples.
  TEST_ASSERT_EQUAL(WSOFT_SAVE_BELOW_MIN, b.saver.update({true, true, true, false, 31205, 0, 1, b.t, b.now}, false,
                                                         true, false, &b.issued, WSOFT_SAVE_BELOW_MIN));
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NOT_STABLE, b.saver.update({true, true, true, false, 31205, 0, 1, b.t, b.now}, false,
                                                          true, false, &b.issued, WSOFT_SAVE_NOT_STABLE));
  // No signal still says no signal, whatever the hold.
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NO_SIGNAL, b.saver.update({true, false, true, false, 31205, 0, 1, b.t, b.now}, false,
                                                         true, false, &b.issued, WSOFT_SAVE_BLANK));
  // Nothing held: it saves.
  b.feed(31205, 8, false);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_SAVED, b.tap(31205));
}

void test_auto_save_only_saves_inside_the_hi_lo_limits() {
  Bench b;
  b.limits = true;
  b.feed(0, 4, true);
  TEST_ASSERT_EQUAL(0, b.feed(2900, 20, true));  // too low: auto save waits
  TEST_ASSERT_EQUAL(0, b.feed(3300, 20, true));  // too high: still waits
  TEST_ASSERT_EQUAL(1, b.feed(3100, 20, true));  // topped up to inside the limits: saved
  TEST_ASSERT_FALSE(b.saver.last_overridden());
  b.feed(0, 4, true);
  TEST_ASSERT_EQUAL(1, b.feed(3000, 20, true));  // exactly at the low limit is inside
  // No product picked: auto save as before, whatever the weight.
  b.limits = false;
  b.feed(0, 4, true);
  TEST_ASSERT_EQUAL(1, b.feed(9999, 20, true));
}

void test_tap_outside_the_limits_needs_a_second_tap() {
  Bench b;
  b.limits = true;
  b.feed(0, 4, false);
  b.feed(3300, 20, false);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_OUT_OF_LIMITS, b.tap(3300));
  TEST_ASSERT_EQUAL(WSOFT_SAVE_SAVED, b.tap(3300));  // second tap within 5 s: the operator overrides
  TEST_ASSERT_TRUE(b.saver.last_overridden());
  TEST_ASSERT_EQUAL(WSOFT_SAVE_ALREADY, b.tap(3300));

  // Too late: the second tap after 5 s only arms again.
  b.feed(0, 4, false);
  b.feed(2900, 20, false);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_OUT_OF_LIMITS, b.tap(2900));
  b.t += WSOFT_OVERRIDE_MS;
  TEST_ASSERT_EQUAL(WSOFT_SAVE_OUT_OF_LIMITS, b.tap(2900));
  TEST_ASSERT_EQUAL(WSOFT_SAVE_SAVED, b.tap(2900));

  // The weight changed between the taps: the override does not carry over.
  b.feed(0, 4, false);
  b.feed(2900, 20, false);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_OUT_OF_LIMITS, b.tap(2900));
  b.feed(3350, 20, false);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_OUT_OF_LIMITS, b.tap(3350));

  // Inside the limits one tap saves, not marked as an override.
  b.feed(0, 4, false);
  b.feed(3100, 20, false);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_SAVED, b.tap(3100));
  TEST_ASSERT_FALSE(b.saver.last_overridden());
}

void test_every_result_has_words() {
  for (int r = WSOFT_SAVE_SAVED; r <= WSOFT_SAVE_OUT_OF_LIMITS; r++) {
    size_t length = strlen(wsoft_save_result_text((WsoftSaveResult)r));
    TEST_ASSERT_TRUE(length > 0 && length <= 16);
  }
}

// Rhyno, 9 Nov 2026: the reason a save cannot happen is shown big over the weight, all the
// time, not only after a tap. These pin which reason wins.
void test_notice_after_boot_says_take_the_load_off_even_while_moving() {
  Bench b;  // board started with a load already on, and it still wobbles
  b.feed(3120, 1, true);
  b.feed(3125, 1, true);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NEED_ZERO, b.saver.waiting());
  // A tap while it moves gives the same real reason, not "Wait: moving".
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NEED_ZERO, b.tap(3130));
  b.feed(3130, 40, true);  // settled for 10 s: still the same reason, nothing saved
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NEED_ZERO, b.saver.waiting());
  b.feed(0, 2, true);  // emptied: no notice
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NONE, b.saver.waiting());
  TEST_ASSERT_EQUAL(1, b.feed(3120, 20, true));
  TEST_ASSERT_EQUAL(WSOFT_SAVE_ALREADY, b.saver.waiting());  // saved: take it off for the next
  b.feed(3500, 4, true);  // more added on top
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NEED_ZERO, b.saver.waiting());
}

void test_notice_moving_only_after_three_seconds() {
  Bench b;
  b.feed(0, 4, false);
  for (int i = 0; i < 11; i++) b.feed(3000 + i, 1, false);  // 2.75 s of movement
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NONE, b.saver.waiting());
  b.feed(3020, 1, false);
  b.feed(3021, 1, false);  // past 3 s
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NOT_STABLE, b.saver.waiting());
  b.feed(3021, 8, false);  // settled: a tap would save, so no notice
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NONE, b.saver.waiting());
}

void test_notice_for_signal_clock_full_and_limits() {
  Bench b;
  b.feed(0, 4, false, false);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NO_SIGNAL, b.saver.waiting());
  b.now = 1000;  // clock not set: shown even with an empty scale
  b.feed(0, 4, false);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NO_CLOCK, b.saver.waiting());
  b.now = 1762520400;
  b.feed(0, 4, false);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_NONE, b.saver.waiting());  // empty scale: nothing to say
  b.t += 250;
  b.saver.update({true, true, true, false, 3120, 0, 1, b.t, b.now}, false, false, true, &b.issued);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_FULL, b.saver.waiting());
  b.limits = true;
  b.feed(3300, 8, true);  // auto save waits outside the limits: say so
  TEST_ASSERT_EQUAL(WSOFT_SAVE_OUT_OF_LIMITS, b.saver.waiting());
  b.t += 250;
  b.saver.update({true, true, true, false, 3300, 0, 1, b.t, b.now}, false, false, false, &b.issued, WSOFT_SAVE_BLANK,
                 WSOFT_HILO_HIGH);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_BLANK, b.saver.waiting());
}

void test_every_notice_fits_the_screens() {
  WsoftSaveNotice n;
  TEST_ASSERT_FALSE(wsoft_save_notice(WSOFT_SAVE_NONE, &n));
  TEST_ASSERT_FALSE(wsoft_save_notice(WSOFT_SAVE_SAVED, &n));
  TEST_ASSERT_FALSE(wsoft_save_notice(WSOFT_SAVE_EMPTY, &n));
  for (int r = WSOFT_SAVE_SAVED; r <= WSOFT_SAVE_OUT_OF_LIMITS; r++) {
    if (r == WSOFT_SAVE_EMPTY || r == WSOFT_SAVE_SAVED) continue;
    TEST_ASSERT_TRUE(wsoft_save_notice((WsoftSaveResult)r, &n));
    TEST_ASSERT_TRUE(strlen(n.title) > 0 && strlen(n.title) <= 20);
    TEST_ASSERT_TRUE(strlen(n.detail) > 0 && strlen(n.detail) <= 32);
    TEST_ASSERT_TRUE(strlen(n.code) > 0);
  }
  TEST_ASSERT_TRUE(wsoft_save_notice(WSOFT_SAVE_NEED_ZERO, &n));
  TEST_ASSERT_EQUAL_STRING("TAKE THE LOAD OFF", n.title);
  TEST_ASSERT_EQUAL_STRING("The scale must show 0 kg first", n.detail);
  TEST_ASSERT_EQUAL(WSOFT_NOTICE_STOP, n.level);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_kg_text_for_each_unit);
  RUN_TEST(test_kg_text_refuses_what_the_hub_would_reject);
  RUN_TEST(test_json_matches_the_hub_contract);
  RUN_TEST(test_json_keeps_decimals_and_for_trade);
  RUN_TEST(test_json_with_a_tare_sends_gross_tare_net_and_nt);
  RUN_TEST(test_json_carries_the_hi_lo_result_on_the_net_weight);
  RUN_TEST(test_json_refuses_unsendable_weighings);
  RUN_TEST(test_auto_save_once_per_settled_load);
  RUN_TEST(test_a_second_weighing_needs_the_scale_back_at_zero);
  RUN_TEST(test_after_boot_the_scale_must_show_zero_first);
  RUN_TEST(test_auto_save_off_saves_nothing_by_itself);
  RUN_TEST(test_tap_saves_a_settled_load_once);
  RUN_TEST(test_tap_explains_why_it_cannot_save);
  RUN_TEST(test_no_signal);
  RUN_TEST(test_a_held_reading_is_not_saved);
  RUN_TEST(test_auto_save_only_saves_inside_the_hi_lo_limits);
  RUN_TEST(test_tap_outside_the_limits_needs_a_second_tap);
  RUN_TEST(test_every_result_has_words);
  RUN_TEST(test_notice_after_boot_says_take_the_load_off_even_while_moving);
  RUN_TEST(test_notice_moving_only_after_three_seconds);
  RUN_TEST(test_notice_for_signal_clock_full_and_limits);
  RUN_TEST(test_every_notice_fits_the_screens);
  return UNITY_END();
}
