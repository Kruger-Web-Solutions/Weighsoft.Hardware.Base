// Host tests for the save-a-weighing rules (lib/wsoft_core/src/save_logic.cpp).
// Run: pio test -e native   (needs a host g++ on PATH)

#include <unity.h>

#include <string.h>

#include "save_logic.h"

static const int64_t CLOCK = 1761900000;  // 31 Oct 2025, past SAVE_CLOCK_VALID_AFTER

struct Rig {
  SaveEngine engine;
  bool auto_on = false;
  uint32_t auto_ms = 3000;
  bool queue_full = false;
  int saves = 0;
  int auto_saves = 0;
};

static SaveInput input(const Rig &rig, uint32_t ms, int32_t value, bool manual = false, bool fresh = true) {
  SaveInput in = {true, fresh, false, value, 1, 1, ms, CLOCK + ms / 1000, rig.auto_on, rig.auto_ms, manual,
                  rig.queue_full};
  return in;
}

static SaveDecision step(Rig &rig, const SaveInput &in) {
  SaveDecision d = rig.engine.update(in);
  if (d.save) {
    rig.saves++;
    if (d.automatic) rig.auto_saves++;
  }
  return d;
}

// Same fresh value every 250 ms (the firmware's poll rate) from start to end inclusive.
static SaveDecision hold(Rig &rig, uint32_t start, uint32_t end, int32_t value) {
  SaveDecision d = {SAVE_MOTION, false, false};
  for (uint32_t ms = start; ms <= end; ms += 250) d = step(rig, input(rig, ms, value));
  return d;
}

void setUp() {}
void tearDown() {}

// ---------- auto-save timer ----------

void test_auto_save_waits_the_full_stable_time() {
  Rig rig;
  rig.auto_on = true;
  rig.auto_ms = 3000;
  hold(rig, 0, 2750, 123);  // N - 250 ms: not yet
  TEST_ASSERT_EQUAL(0, rig.saves);
  SaveDecision d = hold(rig, 3000, 3000, 123);  // exactly N
  TEST_ASSERT_TRUE(d.save);
  TEST_ASSERT_TRUE(d.automatic);
  TEST_ASSERT_EQUAL(1, rig.saves);
}

void test_auto_save_happens_once_per_load() {
  Rig rig;
  rig.auto_on = true;
  hold(rig, 0, 20000, 123);
  TEST_ASSERT_EQUAL(1, rig.saves);
  TEST_ASSERT_EQUAL(SAVE_DONE, hold(rig, 20250, 20250, 123).check);
}

void test_changed_load_without_empty_is_not_saved_again() {
  Rig rig;
  rig.auto_on = true;
  hold(rig, 0, 3000, 123);
  TEST_ASSERT_EQUAL(1, rig.saves);
  hold(rig, 3250, 10000, 456);  // more put on top, never back to empty
  TEST_ASSERT_EQUAL(1, rig.saves);
}

void test_back_to_empty_rearms_the_next_save() {
  Rig rig;
  rig.auto_on = true;
  hold(rig, 0, 3000, 123);
  TEST_ASSERT_EQUAL(SAVE_EMPTY, hold(rig, 3250, 5000, 0).check);
  hold(rig, 5250, 8000, 123);  // same weight again: a new load, 2.75 s
  TEST_ASSERT_EQUAL(1, rig.saves);
  hold(rig, 8250, 8250, 123);
  TEST_ASSERT_EQUAL(2, rig.saves);
  TEST_ASSERT_EQUAL(2, rig.auto_saves);
}

void test_negative_reading_counts_as_empty() {
  Rig rig;
  rig.auto_on = true;
  hold(rig, 0, 3000, 123);
  // One fresh reading below zero re-arms, even before it is stable (same as the trade rules).
  TEST_ASSERT_EQUAL(SAVE_MOTION, hold(rig, 3250, 3250, -2).check);
  hold(rig, 3500, 6500, 123);
  TEST_ASSERT_EQUAL(2, rig.saves);
}

void test_auto_off_never_saves_by_itself() {
  Rig rig;
  TEST_ASSERT_EQUAL(SAVE_OK, hold(rig, 0, 60000, 123).check);
  TEST_ASSERT_EQUAL(0, rig.saves);
}

void test_auto_time_below_one_second_is_raised_to_one_second() {
  Rig rig;
  rig.auto_on = true;
  rig.auto_ms = 0;
  hold(rig, 0, 750, 123);
  TEST_ASSERT_EQUAL(0, rig.saves);
  hold(rig, 1000, 1000, 123);
  TEST_ASSERT_EQUAL(1, rig.saves);
}

void test_movement_restarts_the_timer() {
  Rig rig;
  rig.auto_on = true;
  hold(rig, 0, 2500, 123);
  hold(rig, 2750, 2750, 124);   // moved
  hold(rig, 3000, 5500, 124);   // 2.75 s at the new value
  TEST_ASSERT_EQUAL(0, rig.saves);
  hold(rig, 5750, 5750, 124);   // 3 s
  TEST_ASSERT_EQUAL(1, rig.saves);
}

void test_failed_poll_restarts_the_timer() {
  Rig rig;
  rig.auto_on = true;
  hold(rig, 0, 2500, 123);
  SaveDecision d = step(rig, input(rig, 2750, 123, false, false));  // carried-over value
  TEST_ASSERT_EQUAL(SAVE_STALE, d.check);
  TEST_ASSERT_FALSE(d.save);
  hold(rig, 3000, 5750, 123);  // fresh again from 3000: 2.75 s
  TEST_ASSERT_EQUAL(0, rig.saves);
  hold(rig, 6000, 6000, 123);
  TEST_ASSERT_EQUAL(1, rig.saves);
}

void test_unit_or_decimals_change_restarts_the_timer() {
  Rig rig;
  rig.auto_on = true;
  hold(rig, 0, 2750, 123);
  SaveInput in = input(rig, 3000, 123);
  in.decimals = 2;  // same raw number, different weight
  TEST_ASSERT_EQUAL(SAVE_MOTION, step(rig, in).check);
  TEST_ASSERT_EQUAL(0, rig.saves);
}

// ---------- manual SAVE (screen / web) ----------

void test_manual_save_while_moving_is_refused() {
  Rig rig;
  hold(rig, 0, 500, 123);
  SaveDecision d = step(rig, input(rig, 750, 123, true));
  TEST_ASSERT_EQUAL(SAVE_MOTION, d.check);
  TEST_ASSERT_FALSE(d.save);
}

void test_manual_save_after_one_second_stable() {
  Rig rig;
  rig.auto_on = true;
  rig.auto_ms = 10000;  // auto would wait much longer
  hold(rig, 0, 750, 123);
  SaveDecision d = step(rig, input(rig, 1000, 123, true));
  TEST_ASSERT_EQUAL(SAVE_OK, d.check);
  TEST_ASSERT_TRUE(d.save);
  TEST_ASSERT_FALSE(d.automatic);
  // and auto-save does not save the same load again later
  hold(rig, 1250, 20000, 123);
  TEST_ASSERT_EQUAL(1, rig.saves);
}

void test_second_manual_save_of_the_same_load_is_refused() {
  Rig rig;
  hold(rig, 0, 1000, 123);
  TEST_ASSERT_TRUE(step(rig, input(rig, 1250, 123, true)).save);
  SaveDecision d = step(rig, input(rig, 1500, 123, true));
  TEST_ASSERT_EQUAL(SAVE_DONE, d.check);
  TEST_ASSERT_FALSE(d.save);
}

void test_manual_save_on_empty_scale_is_refused() {
  Rig rig;
  hold(rig, 0, 2000, 0);
  SaveDecision d = step(rig, input(rig, 2250, 0, true));
  TEST_ASSERT_EQUAL(SAVE_EMPTY, d.check);
  TEST_ASSERT_FALSE(d.save);
}

// ---------- blocked states ----------

void test_no_signal_blocks() {
  Rig rig;
  hold(rig, 0, 2000, 123);
  SaveInput in = input(rig, 2250, 123, true);
  in.linked = false;
  SaveDecision d = step(rig, in);
  TEST_ASSERT_EQUAL(SAVE_NO_SIGNAL, d.check);
  TEST_ASSERT_FALSE(d.save);
}

void test_alarm_blocks() {
  Rig rig;
  rig.auto_on = true;
  for (uint32_t ms = 0; ms <= 10000; ms += 250) {
    SaveInput in = input(rig, ms, 123, true);
    in.alarm = true;
    TEST_ASSERT_EQUAL(SAVE_ALARM, step(rig, in).check);
  }
  TEST_ASSERT_EQUAL(0, rig.saves);
}

void test_queue_full_refuses_and_keeps_the_load_unsaved() {
  Rig rig;
  rig.auto_on = true;
  rig.queue_full = true;
  TEST_ASSERT_EQUAL(SAVE_QUEUE_FULL, hold(rig, 0, 5000, 123).check);
  TEST_ASSERT_EQUAL(0, rig.saves);
  rig.queue_full = false;  // the Hub took some: the same load is saved now
  hold(rig, 5250, 5250, 123);
  TEST_ASSERT_EQUAL(1, rig.saves);
}

void test_no_clock_blocks() {
  Rig rig;
  hold(rig, 0, 2000, 123);
  SaveInput in = input(rig, 2250, 123, true);
  in.now_unix = 0;
  TEST_ASSERT_EQUAL(SAVE_NO_CLOCK, step(rig, in).check);
  TEST_ASSERT_EQUAL(0, rig.saves);
}

void test_unit_without_kg_meaning_blocks() {
  Rig rig;
  SaveDecision d = {SAVE_MOTION, false, false};
  for (uint32_t ms = 0; ms <= 2000; ms += 250) {
    SaveInput in = input(rig, ms, 123, ms == 2000);
    in.unit = 0;
    d = step(rig, in);
  }
  TEST_ASSERT_EQUAL(SAVE_UNIT, d.check);
  TEST_ASSERT_EQUAL(0, rig.saves);
}

void test_check_texts() {
  TEST_ASSERT_EQUAL_STRING("ok", save_check_text(SAVE_OK));
  TEST_ASSERT_EQUAL_STRING("moving", save_check_text(SAVE_MOTION));
  TEST_ASSERT_EQUAL_STRING("alarm", save_check_text(SAVE_ALARM));
  TEST_ASSERT_EQUAL_STRING("no_signal", save_check_text(SAVE_NO_SIGNAL));
  TEST_ASSERT_EQUAL_STRING("stale", save_check_text(SAVE_STALE));
  TEST_ASSERT_EQUAL_STRING("empty", save_check_text(SAVE_EMPTY));
  TEST_ASSERT_EQUAL_STRING("saved", save_check_text(SAVE_DONE));
  TEST_ASSERT_EQUAL_STRING("queue_full", save_check_text(SAVE_QUEUE_FULL));
  TEST_ASSERT_EQUAL_STRING("no_clock", save_check_text(SAVE_NO_CLOCK));
}

// ---------- queue ----------

static SavedWeighing weighing(uint32_t seq) {
  SavedWeighing w;
  w.valid = true;
  w.seq = seq;
  w.value = 100 + seq;
  w.decimals = 1;
  w.unit = 1;
  w.at = CLOCK;
  return w;
}

void test_queue_refuses_when_full_and_keeps_the_oldest() {
  SaveQueue q;
  for (uint32_t i = 1; i <= SaveQueue::CAPACITY; i++) TEST_ASSERT_TRUE(q.push(weighing(i)));
  TEST_ASSERT_TRUE(q.full());
  TEST_ASSERT_FALSE(q.push(weighing(99)));
  TEST_ASSERT_EQUAL_UINT32(1, q.at(0).seq);
  TEST_ASSERT_EQUAL_UINT32(SaveQueue::CAPACITY, q.at(SaveQueue::CAPACITY - 1).seq);
}

void test_queue_ack_removes_oldest_first_and_wraps() {
  SaveQueue q;
  for (uint32_t i = 1; i <= 5; i++) q.push(weighing(i));
  TEST_ASSERT_EQUAL(2, q.ack_through(2));
  TEST_ASSERT_EQUAL_UINT32(3, q.at(0).seq);
  TEST_ASSERT_EQUAL(0, q.ack_through(2));  // a repeated ack changes nothing
  for (uint32_t i = 6; i <= SaveQueue::CAPACITY + 2; i++) TEST_ASSERT_TRUE(q.push(weighing(i)));
  TEST_ASSERT_TRUE(q.full());
  TEST_ASSERT_EQUAL_UINT32(SaveQueue::CAPACITY + 2, q.at(SaveQueue::CAPACITY - 1).seq);
}

void test_queue_restore() {
  SavedWeighing slots[SaveQueue::CAPACITY];
  slots[(40 + 0) % SaveQueue::CAPACITY] = weighing(7);
  slots[(40 + 1) % SaveQueue::CAPACITY] = weighing(8);
  SaveQueue q;
  q.restore(40, 2, slots);
  TEST_ASSERT_EQUAL(2, q.size());
  TEST_ASSERT_EQUAL_UINT32(40, q.head());
  TEST_ASSERT_EQUAL_UINT32(7, q.at(0).seq);
  TEST_ASSERT_EQUAL_UINT32(8, q.at(1).seq);
}

// ---------- kg text ----------

static void expect_kg(const char *expected, int32_t value, int decimals, int unit) {
  char out[24];
  TEST_ASSERT_TRUE(save_kg_text(value, decimals, unit, out, sizeof out));
  TEST_ASSERT_EQUAL_STRING(expected, out);
}

static void expect_no_kg(int32_t value, int decimals, int unit) {
  char out[24];
  TEST_ASSERT_FALSE(save_kg_text(value, decimals, unit, out, sizeof out));
}

void test_kg_text_is_exact() {
  expect_kg("12.3", 123, 1, 1);
  expect_kg("1.52", 1520, 3, 1);
  expect_kg("1500", 1500, 0, 1);
  expect_kg("0", 0, 3, 1);
  expect_kg("0.005", 5, 3, 1);
  expect_kg("2500", 25, 1, 2);       // 2.5 t
  expect_kg("12345", 12345, 3, 2);   // 12.345 t
  expect_kg("1.5", 1500, 0, 3);      // 1500 g
  expect_kg("0.0125", 125, 1, 3);    // 12.5 g
  expect_kg("0.00125", 125, 2, 3);   // 1.25 g
  expect_kg("99999.9999", 999999999, 4, 1);
  expect_kg("99999.999", 99999999, 0, 3);   // grams
  expect_kg("99999", 99999, 0, 1);
}

void test_kg_text_refuses_what_the_hub_cannot_store() {
  expect_no_kg(100000, 0, 1);  // 6 digits before the point
  expect_no_kg(1, 3, 3);       // 0.000001 kg: 6 decimals
  expect_no_kg(123, 1, 0);     // no unit
  expect_no_kg(123, 1, 7);     // unknown unit
  expect_no_kg(-5, 0, 1);      // gross is never negative
  expect_no_kg(5, 7, 1);       // decimals out of range
  expect_no_kg(100, 0, 2);     // 100 t
}

// ---------- Hub body ----------

void test_body_matches_the_hub_contract() {
  SavedWeighing w = weighing(5);
  w.value = 123;
  w.decimals = 1;
  w.unit = 1;
  w.at = 1761912000;  // 2025-10-31T12:00:00Z
  w.trade = false;
  w.auto_saved = true;
  char body[512];
  size_t n = save_weighing_body(w, "wsoft-a1b2c3d4e5f6-0badf00d", body, sizeof body);
  TEST_ASSERT_EQUAL(strlen(body), n);
  TEST_ASSERT_EQUAL_STRING(
      "{\"weighing_id\":\"wsoft-a1b2c3d4e5f6-0badf00d-5\",\"committed_at\":\"2025-10-31T12:00:00Z\","
      "\"gross_kg\":12.3,\"tare_kg\":0,\"net_kg\":12.3,\"basis\":\"GS\",\"payload\":{\"source\":\"wsoft\","
      "\"seq\":5,\"value\":123,\"decimals\":1,\"unit\":1,\"trade\":false,\"auto_saved\":true}}",
      body);
}

void test_body_is_refused_without_clock_or_room() {
  SavedWeighing w = weighing(5);
  char body[512];
  w.at = 0;
  TEST_ASSERT_EQUAL(0, save_weighing_body(w, "p", body, sizeof body));
  w = weighing(5);
  char small[40];
  TEST_ASSERT_EQUAL(0, save_weighing_body(w, "p", small, sizeof small));
  TEST_ASSERT_EQUAL_STRING("", small);
}

void test_weighing_id_is_prefix_and_seq() {
  char id[64];
  save_weighing_id("wsoft-aa-01", 42, id, sizeof id);
  TEST_ASSERT_EQUAL_STRING("wsoft-aa-01-42", id);
}

// ---------- Hub answers ----------

void test_send_outcomes() {
  TEST_ASSERT_EQUAL(SAVE_SEND_DELIVERED, save_send_outcome(201));
  TEST_ASSERT_EQUAL(SAVE_SEND_DELIVERED, save_send_outcome(200));  // duplicate weighing_id
  TEST_ASSERT_EQUAL(SAVE_SEND_RETRY, save_send_outcome(-1));
  TEST_ASSERT_EQUAL(SAVE_SEND_RETRY, save_send_outcome(409));
  TEST_ASSERT_EQUAL(SAVE_SEND_RETRY, save_send_outcome(429));
  TEST_ASSERT_EQUAL(SAVE_SEND_RETRY, save_send_outcome(503));
  TEST_ASSERT_EQUAL(SAVE_SEND_REJECTED, save_send_outcome(400));
  TEST_ASSERT_EQUAL(SAVE_SEND_REJECTED, save_send_outcome(401));
  TEST_ASSERT_EQUAL(SAVE_SEND_REJECTED, save_send_outcome(404));
}

void test_retry_waits_back_off_and_cap() {
  TEST_ASSERT_EQUAL_UINT32(1000, save_retry_ms(SAVE_SEND_DELIVERED, 0));
  TEST_ASSERT_EQUAL_UINT32(5000, save_retry_ms(SAVE_SEND_RETRY, 1));
  TEST_ASSERT_EQUAL_UINT32(10000, save_retry_ms(SAVE_SEND_RETRY, 2));
  TEST_ASSERT_EQUAL_UINT32(40000, save_retry_ms(SAVE_SEND_RETRY, 4));
  TEST_ASSERT_EQUAL_UINT32(60000, save_retry_ms(SAVE_SEND_RETRY, 5));
  TEST_ASSERT_EQUAL_UINT32(60000, save_retry_ms(SAVE_SEND_RETRY, 1000));
  TEST_ASSERT_EQUAL_UINT32(60000, save_retry_ms(SAVE_SEND_REJECTED, 1));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_auto_save_waits_the_full_stable_time);
  RUN_TEST(test_auto_save_happens_once_per_load);
  RUN_TEST(test_changed_load_without_empty_is_not_saved_again);
  RUN_TEST(test_back_to_empty_rearms_the_next_save);
  RUN_TEST(test_negative_reading_counts_as_empty);
  RUN_TEST(test_auto_off_never_saves_by_itself);
  RUN_TEST(test_auto_time_below_one_second_is_raised_to_one_second);
  RUN_TEST(test_movement_restarts_the_timer);
  RUN_TEST(test_failed_poll_restarts_the_timer);
  RUN_TEST(test_unit_or_decimals_change_restarts_the_timer);
  RUN_TEST(test_manual_save_while_moving_is_refused);
  RUN_TEST(test_manual_save_after_one_second_stable);
  RUN_TEST(test_second_manual_save_of_the_same_load_is_refused);
  RUN_TEST(test_manual_save_on_empty_scale_is_refused);
  RUN_TEST(test_no_signal_blocks);
  RUN_TEST(test_alarm_blocks);
  RUN_TEST(test_queue_full_refuses_and_keeps_the_load_unsaved);
  RUN_TEST(test_no_clock_blocks);
  RUN_TEST(test_unit_without_kg_meaning_blocks);
  RUN_TEST(test_check_texts);
  RUN_TEST(test_queue_refuses_when_full_and_keeps_the_oldest);
  RUN_TEST(test_queue_ack_removes_oldest_first_and_wraps);
  RUN_TEST(test_queue_restore);
  RUN_TEST(test_kg_text_is_exact);
  RUN_TEST(test_kg_text_refuses_what_the_hub_cannot_store);
  RUN_TEST(test_body_matches_the_hub_contract);
  RUN_TEST(test_body_is_refused_without_clock_or_room);
  RUN_TEST(test_weighing_id_is_prefix_and_seq);
  RUN_TEST(test_send_outcomes);
  RUN_TEST(test_retry_waits_back_off_and_cap);
  return UNITY_END();
}
