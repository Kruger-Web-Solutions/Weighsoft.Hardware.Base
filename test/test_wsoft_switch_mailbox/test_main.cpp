// Host tests for the one-request mailbox between the web / Hub tasks and the screen task
// (lib/wsoft_core/src/wsoft_switch_mailbox.cpp). Each caller holds a ticket and only ever
// gets, or withdraws, its own request.
// Run: pio test -e native   (needs a host g++ on PATH)

#include <unity.h>

#include <string.h>

#include "wsoft_switch_mailbox.h"

void setUp() {}
void tearDown() {}

static WsoftSwitchRequest request(bool on) {
  WsoftSwitchRequest r;
  r.which = WSOFT_SWITCH_AUTO_SAVE;
  r.on = on;
  strcpy(r.who, "rhyno");
  return r;
}

void test_one_request_round_trip() {
  WsoftSwitchMailbox box;
  WsoftSwitchRequest taken;
  WsoftSwitchError result = WSOFT_SWITCH_BUSY;
  TEST_ASSERT_FALSE(box.take(&taken));  // nothing waits
  uint32_t t = box.post(request(true));
  TEST_ASSERT_TRUE(0 != t);
  TEST_ASSERT_EQUAL(WsoftSwitchMailbox::QUEUED, box.check(t, &result));
  TEST_ASSERT_TRUE(box.take(&taken));
  TEST_ASSERT_TRUE(taken.on);
  TEST_ASSERT_EQUAL(WsoftSwitchMailbox::RUNNING, box.check(t, &result));
  box.finish(WSOFT_SWITCH_ALREADY);
  TEST_ASSERT_EQUAL(WsoftSwitchMailbox::DONE, box.check(t, &result));
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_ALREADY, result);
  // Handed over once; then the box is free and the old ticket is gone.
  TEST_ASSERT_EQUAL(WsoftSwitchMailbox::GONE, box.check(t, &result));
  TEST_ASSERT_TRUE(0 != box.post(request(false)));
}

void test_second_caller_is_busy_while_one_is_in_the_box() {
  WsoftSwitchMailbox box;
  WsoftSwitchRequest taken;
  WsoftSwitchError result;
  uint32_t a = box.post(request(true));
  TEST_ASSERT_EQUAL(0, box.post(request(false)));  // queued
  box.take(&taken);
  TEST_ASSERT_EQUAL(0, box.post(request(false)));  // running
  box.finish(WSOFT_SWITCH_OK);
  TEST_ASSERT_EQUAL(0, box.post(request(false)));  // done, but A has not read it yet
  TEST_ASSERT_EQUAL(WsoftSwitchMailbox::DONE, box.check(a, &result));
  TEST_ASSERT_TRUE(0 != box.post(request(false)));
}

// The old bug: B's post landed on A's unread result, and A then read B's answer.
void test_a_caller_never_gets_another_callers_result() {
  WsoftSwitchMailbox box;
  WsoftSwitchRequest taken;
  WsoftSwitchError result = WSOFT_SWITCH_OK;
  uint32_t a = box.post(request(true));
  box.take(&taken);
  box.leave(a);  // A gives up while it runs
  box.finish(WSOFT_SWITCH_OK);
  uint32_t b = box.post(request(false));  // done and nobody waits: B may post
  TEST_ASSERT_TRUE(0 != b);
  TEST_ASSERT_TRUE(a != b);
  TEST_ASSERT_EQUAL(WsoftSwitchMailbox::GONE, box.check(a, &result));
  box.take(&taken);
  box.finish(WSOFT_SWITCH_ALREADY);
  TEST_ASSERT_EQUAL(WsoftSwitchMailbox::GONE, box.check(a, &result));
  TEST_ASSERT_EQUAL(WsoftSwitchMailbox::DONE, box.check(b, &result));
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_ALREADY, result);
}

void test_withdraw_only_your_own_queued_request() {
  WsoftSwitchMailbox box;
  WsoftSwitchRequest taken;
  uint32_t a = box.post(request(true));
  TEST_ASSERT_FALSE(box.leave(a + 1));  // not A's ticket: nothing happens
  TEST_ASSERT_TRUE(box.leave(a));       // withdrawn: it is never applied
  TEST_ASSERT_FALSE(box.take(&taken));
  uint32_t b = box.post(request(false));
  TEST_ASSERT_TRUE(0 != b);
  TEST_ASSERT_FALSE(box.leave(a));  // A's old ticket cannot pull B's request
  TEST_ASSERT_TRUE(box.take(&taken));
  TEST_ASSERT_FALSE(taken.on);
  TEST_ASSERT_FALSE(box.leave(b));  // already running: it may still be applied
}

void test_ticket_zero_is_never_handed_out() {
  WsoftSwitchMailbox box(0xFFFFFFFFu);
  WsoftSwitchRequest taken;
  WsoftSwitchError result;
  uint32_t a = box.post(request(true));
  TEST_ASSERT_EQUAL(0xFFFFFFFFu, a);
  box.take(&taken);
  box.finish(WSOFT_SWITCH_OK);
  box.check(a, &result);
  TEST_ASSERT_EQUAL(1u, box.post(request(true)));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_one_request_round_trip);
  RUN_TEST(test_second_caller_is_busy_while_one_is_in_the_box);
  RUN_TEST(test_a_caller_never_gets_another_callers_result);
  RUN_TEST(test_withdraw_only_your_own_queued_request);
  RUN_TEST(test_ticket_zero_is_never_handed_out);
  return UNITY_END();
}
