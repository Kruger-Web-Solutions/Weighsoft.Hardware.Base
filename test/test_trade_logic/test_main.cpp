// Host tests for the trade-weight rules (src/trade_logic.cpp).
// Run: pio test -e native   (needs a host g++ on PATH)

#include <unity.h>

#include "trade_logic.h"

static const int64_t CLOCK = 1761400000;  // 2025-10-25, past TRADE_CLOCK_VALID_AFTER

static TradeReading reading(uint32_t ms, int32_t value, bool fresh = true) {
  TradeReading r = {true, true, fresh, false, value, 3, 1, ms, CLOCK + ms / 1000};
  return r;
}

// Feeds the same fresh value every 250 ms (the firmware's poll rate) from start to end inclusive.
static TradeCheck hold(TradeEngine &e, TradeQueue &q, uint32_t start, uint32_t end, int32_t value) {
  TradeCheck c = TRADE_MOTION;
  for (uint32_t ms = start; ms <= end; ms += 250) {
    TradeWeight w;
    c = e.update(reading(ms, value), q.full(), &w);
    if (w.valid) TEST_ASSERT_TRUE(q.push(w));
  }
  return c;
}

void setUp() {}
void tearDown() {}

// ---------- stale reading (Medium finding) ----------

void test_failed_poll_is_reported_stale_and_issues_nothing() {
  TradeEngine e;
  TradeQueue q;
  hold(e, q, 0, 750, 1520);
  TradeWeight w;
  TEST_ASSERT_EQUAL(TRADE_STALE, e.update(reading(1000, 1520, false), false, &w));
  TEST_ASSERT_FALSE(w.valid);
  TEST_ASSERT_FALSE(e.stable());
}

void test_failed_poll_restarts_the_stability_clock() {
  TradeEngine e;
  TradeQueue q;
  hold(e, q, 0, 500, 1520);
  TradeWeight w;
  e.update(reading(750, 1520, false), false, &w);  // carried-over value, same number
  // Without the fix 1000 ms would already count as 1 s unchanged. Fresh readings restart at 1000.
  TEST_ASSERT_EQUAL(TRADE_MOTION, hold(e, q, 1000, 1750, 1520));
  TEST_ASSERT_EQUAL(0, q.size());
  TEST_ASSERT_EQUAL(TRADE_OK, hold(e, q, 2000, 2000, 1520));
  TEST_ASSERT_EQUAL(1, q.size());
}

void test_stable_after_one_second_of_fresh_unchanged_readings() {
  TradeEngine e;
  TradeQueue q;
  TEST_ASSERT_EQUAL(TRADE_MOTION, hold(e, q, 0, 750, 1520));
  TEST_ASSERT_EQUAL(TRADE_OK, hold(e, q, 1000, 1000, 1520));
  TEST_ASSERT_TRUE(e.stable());
  TEST_ASSERT_EQUAL_UINT32(1, q.at(0).seq);
  TEST_ASSERT_EQUAL_INT32(1520, q.at(0).value);
  TEST_ASSERT_EQUAL_INT64(CLOCK + 1, q.at(0).at);
}

// ---------- issuing ----------

void test_same_load_is_issued_once() {
  TradeEngine e;
  TradeQueue q;
  hold(e, q, 0, 5000, 1520);
  TEST_ASSERT_EQUAL(1, q.size());
  TEST_ASSERT_TRUE(e.current());
}

void test_emptying_the_scale_gives_the_same_weight_a_new_seq() {
  TradeEngine e;
  TradeQueue q;
  hold(e, q, 0, 1500, 1520);
  TEST_ASSERT_EQUAL(TRADE_EMPTY, hold(e, q, 1750, 3000, 0));
  hold(e, q, 3250, 5000, 1520);
  TEST_ASSERT_EQUAL(2, q.size());
  TEST_ASSERT_EQUAL_UINT32(2, q.at(1).seq);
}

void test_approval_off_never_issues() {
  TradeEngine e;
  for (uint32_t ms = 0; ms <= 3000; ms += 250) {
    TradeReading r = reading(ms, 1520);
    r.on = false;
    TradeWeight w;
    TEST_ASSERT_EQUAL(TRADE_OFF, e.update(r, false, &w));
    TEST_ASSERT_FALSE(w.valid);
  }
  TEST_ASSERT_EQUAL_UINT32(1, e.next_seq());
}

void test_unset_clock_blocks_trade() {
  TradeEngine e;
  TradeCheck c = TRADE_MOTION;
  for (uint32_t ms = 0; ms <= 2000; ms += 250) {
    TradeReading r = reading(ms, 1520);
    r.now_unix = 1000;
    TradeWeight w;
    c = e.update(r, false, &w);
    TEST_ASSERT_FALSE(w.valid);
  }
  TEST_ASSERT_EQUAL(TRADE_NO_CLOCK, c);
}

void test_alarm_blocks_trade() {
  TradeEngine e;
  TradeCheck c = TRADE_MOTION;
  for (uint32_t ms = 0; ms <= 2000; ms += 250) {
    TradeReading r = reading(ms, 1520);
    r.alarm = true;
    TradeWeight w;
    c = e.update(r, false, &w);
    TEST_ASSERT_FALSE(w.valid);
  }
  TEST_ASSERT_EQUAL(TRADE_ALARM, c);
}

// ---------- queue (High #2) ----------

void test_two_quick_trade_weights_are_both_kept_until_acked() {
  TradeEngine e;
  TradeQueue q;
  hold(e, q, 0, 1500, 1520);
  hold(e, q, 1750, 3500, 2140);  // a second load well inside one 30 s heartbeat
  TEST_ASSERT_EQUAL(2, q.size());
  TEST_ASSERT_EQUAL(1, q.ack_through(1));
  TEST_ASSERT_EQUAL(1, q.size());
  TEST_ASSERT_EQUAL_UINT32(2, q.at(0).seq);
  TEST_ASSERT_EQUAL_INT32(2140, q.at(0).value);
}

void test_full_queue_stops_issuing_without_using_a_seq() {
  TradeEngine e;
  TradeQueue q;
  TradeWeight w;
  w.valid = true;
  for (uint32_t i = 0; i < TradeQueue::CAPACITY; i++) {
    w.seq = 100 + i;
    TEST_ASSERT_TRUE(q.push(w));
  }
  TEST_ASSERT_TRUE(q.full());
  TEST_ASSERT_FALSE(q.push(w));
  TEST_ASSERT_EQUAL(TRADE_QUEUE_FULL, hold(e, q, 0, 2000, 1520));
  TEST_ASSERT_EQUAL_UINT32(1, e.next_seq());
  TEST_ASSERT_EQUAL_UINT32(100, q.at(0).seq);  // nothing overwritten

  q.ack_through(100);
  TEST_ASSERT_EQUAL(TRADE_OK, hold(e, q, 2250, 2250, 1520));
  TEST_ASSERT_EQUAL_UINT32(1, q.at(TradeQueue::CAPACITY - 1).seq);
}

void test_ack_never_drops_unsent_entries() {
  TradeQueue q;
  TradeWeight w;
  w.valid = true;
  for (uint32_t s = 1; s <= 5; s++) {
    w.seq = s;
    q.push(w);
  }
  TEST_ASSERT_EQUAL(0, q.ack_through(0));
  TEST_ASSERT_EQUAL(3, q.ack_through(3));
  TEST_ASSERT_EQUAL(0, q.ack_through(2));  // a late, older ack changes nothing
  TEST_ASSERT_EQUAL(2, q.size());
  TEST_ASSERT_EQUAL_UINT32(4, q.at(0).seq);
}

void test_queue_wraps_and_restores_from_storage() {
  TradeQueue q;
  TradeWeight w;
  w.valid = true;
  for (uint32_t s = 1; s <= 40; s++) {
    w.seq = s;
    TEST_ASSERT_TRUE(q.push(w));
    if (s % 2 == 0) q.ack_through(s - 1);
  }
  TradeWeight slots[TradeQueue::CAPACITY];
  for (size_t i = 0; i < TradeQueue::CAPACITY; i++) slots[i] = TradeWeight();
  for (size_t i = 0; i < q.size(); i++) slots[(q.head() + i) % TradeQueue::CAPACITY] = q.at(i);

  TradeQueue restored;
  restored.restore(q.head(), q.size(), slots);
  TEST_ASSERT_EQUAL(q.size(), restored.size());
  for (size_t i = 0; i < q.size(); i++) TEST_ASSERT_EQUAL_UINT32(q.at(i).seq, restored.at(i).seq);
}

void test_seq_continues_from_storage_after_restart() {
  TradeEngine e(42);
  TradeQueue q;
  hold(e, q, 0, 1500, 1520);
  TEST_ASSERT_EQUAL_UINT32(42, q.at(0).seq);
  TEST_ASSERT_EQUAL_UINT32(43, e.next_seq());
}

// ---------- weight text (shared by screen, serial log and Hub) ----------

void test_format_weight() {
  char out[16];
  format_weight(1520, 3, out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("1.520", out);
  format_weight(-5, 3, out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("-0.005", out);
  format_weight(590516, 1, out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("59051.6", out);
  format_weight(42, 0, out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("42", out);
  format_weight(-2147483647 - 1, 0, out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("-2147483648", out);
}

void test_weight_unit_text() {
  TEST_ASSERT_EQUAL_STRING("kg", weight_unit_text(1));
  TEST_ASSERT_EQUAL_STRING("t", weight_unit_text(2));
  TEST_ASSERT_EQUAL_STRING("g", weight_unit_text(3));
  TEST_ASSERT_EQUAL_STRING("", weight_unit_text(0));
  TEST_ASSERT_EQUAL_STRING("", weight_unit_text(9));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_failed_poll_is_reported_stale_and_issues_nothing);
  RUN_TEST(test_failed_poll_restarts_the_stability_clock);
  RUN_TEST(test_stable_after_one_second_of_fresh_unchanged_readings);
  RUN_TEST(test_same_load_is_issued_once);
  RUN_TEST(test_emptying_the_scale_gives_the_same_weight_a_new_seq);
  RUN_TEST(test_approval_off_never_issues);
  RUN_TEST(test_unset_clock_blocks_trade);
  RUN_TEST(test_alarm_blocks_trade);
  RUN_TEST(test_two_quick_trade_weights_are_both_kept_until_acked);
  RUN_TEST(test_full_queue_stops_issuing_without_using_a_seq);
  RUN_TEST(test_ack_never_drops_unsent_entries);
  RUN_TEST(test_queue_wraps_and_restores_from_storage);
  RUN_TEST(test_seq_continues_from_storage_after_restart);
  RUN_TEST(test_format_weight);
  RUN_TEST(test_weight_unit_text);
  return UNITY_END();
}
