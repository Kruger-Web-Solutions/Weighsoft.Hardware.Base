// Host tests for Pack-Size Auto-Save (lib/wsoft_core/src/wsoft_pack_set.cpp)
// and its integration with WsoftWeighingSaver.
// Run: pio test -e native -f test_wsoft_pack_set

#include <unity.h>

#include "wsoft_pack_set.h"
#include "wsoft_weighing.h"

void setUp() {}
void tearDown() {}

static WsoftPackSet make_pack_set(bool enabled, int32_t tolerance_g, const int32_t *targets, uint8_t count) {
  WsoftPackSet cfg;
  cfg.enabled = enabled;
  cfg.tolerance_g = tolerance_g;
  cfg.count = count;
  for (uint8_t i = 0; i < count && i < WSOFT_PACK_SET_MAX; i++) cfg.targets_g[i] = targets[i];
  return cfg;
}

void test_defaults_are_disabled_and_zeroed() {
  WsoftPackSet cfg;
  TEST_ASSERT_FALSE(cfg.enabled);
  TEST_ASSERT_EQUAL_UINT8(0, cfg.count);
  TEST_ASSERT_EQUAL_INT32(0, cfg.tolerance_g);
  for (size_t i = 0; i < WSOFT_PACK_SET_MAX; i++) TEST_ASSERT_EQUAL_INT32(0, cfg.targets_g[i]);
  TEST_ASSERT_TRUE(wsoft_pack_set_valid(cfg));

  cfg.enabled = true;
  cfg.count = 2;
  cfg.targets_g[0] = 500;
  cfg.targets_g[1] = 1000;
  cfg.tolerance_g = 20;
  wsoft_pack_set_defaults(&cfg);
  TEST_ASSERT_FALSE(cfg.enabled);
  TEST_ASSERT_EQUAL_UINT8(0, cfg.count);
  TEST_ASSERT_EQUAL_INT32(0, cfg.tolerance_g);
  for (size_t i = 0; i < WSOFT_PACK_SET_MAX; i++) TEST_ASSERT_EQUAL_INT32(0, cfg.targets_g[i]);

  WsoftPackSetMatch m;
  TEST_ASSERT_FALSE(wsoft_pack_set_match(cfg, 500, &m));
  TEST_ASSERT_FALSE(m.matched);
  TEST_ASSERT_EQUAL(WSOFT_HILO_NONE, wsoft_pack_set_hilo_state(cfg, 500));
}

void test_validation_rejects_bad_configs() {
  const int32_t ok_targets[] = {500, 1000, 2000};
  TEST_ASSERT_TRUE(wsoft_pack_set_valid(make_pack_set(true, 20, ok_targets, 3)));

  WsoftPackSet bad = make_pack_set(true, 20, ok_targets, 3);
  bad.count = WSOFT_PACK_SET_MAX + 1;
  TEST_ASSERT_FALSE(wsoft_pack_set_valid(bad));

  bad = make_pack_set(true, -1, ok_targets, 3);
  TEST_ASSERT_FALSE(wsoft_pack_set_valid(bad));

  bad = make_pack_set(true, 20, ok_targets, 3);
  bad.targets_g[1] = 0;
  TEST_ASSERT_FALSE(wsoft_pack_set_valid(bad));

  bad.targets_g[1] = -500;
  TEST_ASSERT_FALSE(wsoft_pack_set_valid(bad));

  bad.targets_g[1] = WSOFT_PACK_SET_GRAMS_MAX + 1;
  TEST_ASSERT_FALSE(wsoft_pack_set_valid(bad));
}

void test_single_target_matching_and_bounds() {
  const int32_t targets[] = {500};
  WsoftPackSet cfg = make_pack_set(true, 20, targets, 1);
  WsoftPackSetMatch m;

  // Exact and boundary matches
  TEST_ASSERT_TRUE(wsoft_pack_set_match(cfg, 500, &m));
  TEST_ASSERT_TRUE(m.matched);
  TEST_ASSERT_EQUAL_INT32(500, m.matched_target_g);
  TEST_ASSERT_EQUAL_INT32(480, m.low_limit_g);
  TEST_ASSERT_EQUAL_INT32(520, m.high_limit_g);
  TEST_ASSERT_EQUAL(WSOFT_HILO_PASS, wsoft_pack_set_hilo_state(cfg, 500));

  TEST_ASSERT_TRUE(wsoft_pack_set_match(cfg, 480, &m));
  TEST_ASSERT_EQUAL(WSOFT_HILO_PASS, wsoft_pack_set_hilo_state(cfg, 480));
  TEST_ASSERT_TRUE(wsoft_pack_set_match(cfg, 520, &m));
  TEST_ASSERT_EQUAL(WSOFT_HILO_PASS, wsoft_pack_set_hilo_state(cfg, 520));

  // Outside tolerance: LOW and HIGH with populated nearest limits
  TEST_ASSERT_FALSE(wsoft_pack_set_match(cfg, 479, &m));
  TEST_ASSERT_FALSE(m.matched);
  TEST_ASSERT_EQUAL_INT32(500, m.matched_target_g);
  TEST_ASSERT_EQUAL_INT32(480, m.low_limit_g);
  TEST_ASSERT_EQUAL_INT32(520, m.high_limit_g);
  TEST_ASSERT_EQUAL(WSOFT_HILO_LOW, wsoft_pack_set_hilo_state(cfg, 479));

  TEST_ASSERT_FALSE(wsoft_pack_set_match(cfg, 521, &m));
  TEST_ASSERT_FALSE(m.matched);
  TEST_ASSERT_EQUAL_INT32(500, m.matched_target_g);
  TEST_ASSERT_EQUAL(WSOFT_HILO_HIGH, wsoft_pack_set_hilo_state(cfg, 521));

  // Empty / negative / unsettled -> NONE
  TEST_ASSERT_FALSE(wsoft_pack_set_match(cfg, 0, &m));
  TEST_ASSERT_EQUAL(WSOFT_HILO_NONE, wsoft_pack_set_hilo_state(cfg, 0));
  TEST_ASSERT_EQUAL(WSOFT_HILO_NONE, wsoft_pack_set_hilo_state(cfg, -10));
  TEST_ASSERT_EQUAL(WSOFT_HILO_NONE, wsoft_pack_set_hilo_state(cfg, 500, false));
}

void test_multi_target_nearest_match_and_gaps() {
  const int32_t targets[] = {500, 1000, 2000};
  WsoftPackSet cfg = make_pack_set(true, 20, targets, 3);
  WsoftPackSetMatch m;

  // 495 g -> nearest 500 g -> PASS
  TEST_ASSERT_TRUE(wsoft_pack_set_match(cfg, 495, &m));
  TEST_ASSERT_EQUAL_INT32(500, m.matched_target_g);
  TEST_ASSERT_EQUAL(WSOFT_HILO_PASS, wsoft_pack_set_hilo_state(cfg, 495));

  // 530 g -> nearest 500 g -> HIGH (outside [480, 520])
  TEST_ASSERT_FALSE(wsoft_pack_set_match(cfg, 530, &m));
  TEST_ASSERT_EQUAL_INT32(500, m.matched_target_g);
  TEST_ASSERT_EQUAL(WSOFT_HILO_HIGH, wsoft_pack_set_hilo_state(cfg, 530));

  // 970 g -> nearest 1000 g -> LOW (outside [980, 1020])
  TEST_ASSERT_FALSE(wsoft_pack_set_match(cfg, 970, &m));
  TEST_ASSERT_EQUAL_INT32(1000, m.matched_target_g);
  TEST_ASSERT_EQUAL_INT32(980, m.low_limit_g);
  TEST_ASSERT_EQUAL_INT32(1020, m.high_limit_g);
  TEST_ASSERT_EQUAL(WSOFT_HILO_LOW, wsoft_pack_set_hilo_state(cfg, 970));

  // 1015 g -> nearest 1000 g -> PASS
  TEST_ASSERT_TRUE(wsoft_pack_set_match(cfg, 1015, &m));
  TEST_ASSERT_EQUAL_INT32(1000, m.matched_target_g);
  TEST_ASSERT_EQUAL(WSOFT_HILO_PASS, wsoft_pack_set_hilo_state(cfg, 1015));

  // 1980 g -> nearest 2000 g -> PASS
  TEST_ASSERT_TRUE(wsoft_pack_set_match(cfg, 1980, &m));
  TEST_ASSERT_EQUAL_INT32(2000, m.matched_target_g);
  TEST_ASSERT_EQUAL(WSOFT_HILO_PASS, wsoft_pack_set_hilo_state(cfg, 1980));

  // 2050 g -> nearest 2000 g -> HIGH
  TEST_ASSERT_FALSE(wsoft_pack_set_match(cfg, 2050, &m));
  TEST_ASSERT_EQUAL_INT32(2000, m.matched_target_g);
  TEST_ASSERT_EQUAL(WSOFT_HILO_HIGH, wsoft_pack_set_hilo_state(cfg, 2050));

  // Disabled -> NONE even when inside a target
  cfg.enabled = false;
  TEST_ASSERT_FALSE(wsoft_pack_set_match(cfg, 1000, &m));
  TEST_ASSERT_EQUAL(WSOFT_HILO_NONE, wsoft_pack_set_hilo_state(cfg, 1000));
}

void test_unit_conversion_check() {
  const int32_t targets[] = {500, 1000, 2000};
  WsoftPackSet cfg = make_pack_set(true, 20, targets, 3);
  WsoftPackSetMatch m;

  TEST_ASSERT_EQUAL(WSOFT_HILO_PASS, wsoft_pack_set_check(cfg, 500, 3, 1, true, &m));   // 0.500 kg
  TEST_ASSERT_EQUAL_INT32(500, m.matched_target_g);
  TEST_ASSERT_EQUAL(WSOFT_HILO_PASS, wsoft_pack_set_check(cfg, 101, 2, 1, true, &m));   // 1.01 kg = 1010 g
  TEST_ASSERT_EQUAL_INT32(1000, m.matched_target_g);
  TEST_ASSERT_EQUAL(WSOFT_HILO_HIGH, wsoft_pack_set_check(cfg, 2050, 0, 3, true, &m));  // 2050 g
  TEST_ASSERT_EQUAL(WSOFT_HILO_PASS, wsoft_pack_set_check(cfg, 2, 3, 2, true, &m));     // 0.002 t = 2000 g
  TEST_ASSERT_EQUAL(WSOFT_HILO_NONE, wsoft_pack_set_check(cfg, 500, 3, 9, true, &m));   // unknown unit
  TEST_ASSERT_EQUAL(WSOFT_HILO_NONE, wsoft_pack_set_check(cfg, 500, 3, 1, false, &m));  // unsettled
}

struct PackBench {
  WsoftWeighingSaver saver{1};
  uint32_t t = 0;
  int64_t now = 1762520400;
  TradeWeight issued;
  WsoftSaveResult last = WSOFT_SAVE_NONE;
  WsoftPackSet cfg;
  int32_t tare = 0;

  int feed(int32_t value, int count, bool auto_save, int decimals = 3, int unit = 1) {
    int saved = 0;
    for (int i = 0; i < count; i++, t += 250) {
      WsoftHiloState state = wsoft_pack_set_check(cfg, value - tare, decimals, unit);
      last = saver.update({true, true, true, false, value, decimals, unit, t, now}, auto_save, false, false, &issued,
                          WSOFT_SAVE_NONE, state);
      if (last == WSOFT_SAVE_SAVED) saved++;
    }
    return saved;
  }

  WsoftSaveResult tap(int32_t value, int decimals = 3, int unit = 1) {
    t += 250;
    WsoftHiloState state = wsoft_pack_set_check(cfg, value - tare, decimals, unit);
    last = saver.update({true, true, true, false, value, decimals, unit, t, now}, false, true, false, &issued,
                        WSOFT_SAVE_NONE, state);
    return last;
  }
};

void test_saver_integration_auto_save_rejects_outside_and_saves_matched() {
  const int32_t targets[] = {500, 1000, 2000};
  PackBench b;
  b.cfg = make_pack_set(true, 20, targets, 3);

  b.feed(0, 4, true);
  // Out of range LOW (450 g vs 500 g +/- 20 g): Auto-Save waits, no countdown.
  TEST_ASSERT_EQUAL(0, b.feed(450, 20, true));
  TEST_ASSERT_EQUAL(0, b.saver.countdown());
  TEST_ASSERT_EQUAL(WSOFT_SAVE_OUT_OF_LIMITS, b.saver.waiting());

  // Out of range HIGH (550 g): still waits.
  TEST_ASSERT_EQUAL(0, b.feed(550, 20, true));
  TEST_ASSERT_EQUAL(WSOFT_SAVE_OUT_OF_LIMITS, b.saver.waiting());

  // Adjusted to 510 g (within 500 +/- 20 g): counts down and auto-saves once.
  TEST_ASSERT_EQUAL(0, b.feed(510, 1, true));
  TEST_ASSERT_EQUAL(3, b.saver.countdown());
  TEST_ASSERT_EQUAL(1, b.feed(510, 19, true));
  TEST_ASSERT_EQUAL_INT32(510, b.saver.last().value);
  TEST_ASSERT_FALSE(b.saver.last_overridden());

  // Tare subtracted before pack matching: 100 g tare + 1010 g net = 1110 g gross -> matches 1000 g.
  b.tare = 100;
  b.feed(0, 4, true);
  TEST_ASSERT_EQUAL(1, b.feed(1110, 20, true));
  TEST_ASSERT_EQUAL_INT32(1110, b.saver.last().value);

  // Manual tap outside limits requires second tap override; Auto-Save still never saves it.
  b.tare = 0;
  b.feed(0, 4, false);
  b.feed(1400, 8, false);
  TEST_ASSERT_EQUAL(WSOFT_SAVE_OUT_OF_LIMITS, b.tap(1400));
  TEST_ASSERT_EQUAL(WSOFT_SAVE_SAVED, b.tap(1400));
  TEST_ASSERT_TRUE(b.saver.last_overridden());
}

void test_saver_integration_disabled_saves_any_settled_weight() {
  const int32_t targets[] = {500, 1000, 2000};
  PackBench b;
  b.cfg = make_pack_set(false, 20, targets, 3);

  b.feed(0, 4, true);
  TEST_ASSERT_EQUAL(1, b.feed(450, 20, true));  // would be LOW if enabled, saves freely when disabled
  TEST_ASSERT_EQUAL_INT32(450, b.saver.last().value);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_defaults_are_disabled_and_zeroed);
  RUN_TEST(test_validation_rejects_bad_configs);
  RUN_TEST(test_single_target_matching_and_bounds);
  RUN_TEST(test_multi_target_nearest_match_and_gaps);
  RUN_TEST(test_unit_conversion_check);
  RUN_TEST(test_saver_integration_auto_save_rejects_outside_and_saves_matched);
  RUN_TEST(test_saver_integration_disabled_saves_any_settled_weight);
  return UNITY_END();
}
