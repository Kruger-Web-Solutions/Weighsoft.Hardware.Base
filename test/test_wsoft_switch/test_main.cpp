// Host tests for the switch rules (lib/wsoft_core/src/wsoft_switch.cpp): one check for
// Approval and Auto save, whoever asks (screen, browser, Hub or USB).
// Run: pio test -e native   (needs a host g++ on PATH)

#include <unity.h>

#include <string.h>

#include "wsoft_switch.h"

void setUp() {}
void tearDown() {}

static WsoftSwitchState state(bool approval_on, bool auto_save, bool rs485, bool setup_complete) {
  WsoftSwitchState s;
  s.approval_on = approval_on;
  s.auto_save = auto_save;
  s.rs485 = rs485;
  s.setup_complete = setup_complete;
  return s;
}

void test_names_and_codes() {
  WsoftSwitchWhich which;
  TEST_ASSERT_TRUE(wsoft_switch_parse("approval", &which));
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_APPROVAL, which);
  TEST_ASSERT_TRUE(wsoft_switch_parse("auto_save", &which));
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_AUTO_SAVE, which);
  TEST_ASSERT_FALSE(wsoft_switch_parse("Approval", &which));
  TEST_ASSERT_FALSE(wsoft_switch_parse(nullptr, &which));
  TEST_ASSERT_EQUAL_STRING("approval", wsoft_switch_name(WSOFT_SWITCH_APPROVAL));
  TEST_ASSERT_EQUAL_STRING("auto_save", wsoft_switch_name(WSOFT_SWITCH_AUTO_SAVE));
  TEST_ASSERT_EQUAL_STRING("screen", wsoft_switch_where_text(WSOFT_WHERE_SCREEN));
  TEST_ASSERT_EQUAL_STRING("browser", wsoft_switch_where_text(WSOFT_WHERE_BROWSER));
  TEST_ASSERT_EQUAL_STRING("hub", wsoft_switch_where_text(WSOFT_WHERE_HUB));
  TEST_ASSERT_EQUAL_STRING("usb", wsoft_switch_where_text(WSOFT_WHERE_USB));
  // Contract A2 codes.
  TEST_ASSERT_EQUAL_STRING("ALREADY", wsoft_switch_error_code(WSOFT_SWITCH_ALREADY));
  TEST_ASSERT_EQUAL_STRING("SETUP_INCOMPLETE", wsoft_switch_error_code(WSOFT_SWITCH_SETUP_INCOMPLETE));
  TEST_ASSERT_EQUAL_STRING("REASON_REQUIRED", wsoft_switch_error_code(WSOFT_SWITCH_REASON_REQUIRED));
  TEST_ASSERT_EQUAL_STRING("REASON_TOO_LONG", wsoft_switch_error_code(WSOFT_SWITCH_REASON_TOO_LONG));
  TEST_ASSERT_EQUAL_STRING("BUSY", wsoft_switch_error_code(WSOFT_SWITCH_BUSY));
  TEST_ASSERT_EQUAL_STRING("WHO_REQUIRED", wsoft_switch_error_code(WSOFT_SWITCH_WHO_REQUIRED));
  for (int e = WSOFT_SWITCH_ALREADY; e <= WSOFT_SWITCH_WHO_REQUIRED; e++) {
    const char *code = wsoft_switch_error_code((WsoftSwitchError)e);
    TEST_ASSERT_TRUE(strlen(code) >= 1 && strlen(code) <= 32);
    TEST_ASSERT_TRUE(strlen(wsoft_switch_error_text((WsoftSwitchError)e)) > 0);
  }
  TEST_ASSERT_EQUAL_STRING("", wsoft_switch_error_code(WSOFT_SWITCH_OK));
}

void test_approval_needs_a_reason_both_ways() {
  WsoftSwitchRequest r;
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_REASON_REQUIRED,
                    wsoft_switch_request(WSOFT_SWITCH_APPROVAL, true, "rhyno", WSOFT_WHERE_BROWSER, "", &r));
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_REASON_REQUIRED,
                    wsoft_switch_request(WSOFT_SWITCH_APPROVAL, false, "rhyno", WSOFT_WHERE_BROWSER, "   ", &r));
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_REASON_REQUIRED,
                    wsoft_switch_request(WSOFT_SWITCH_APPROVAL, false, "rhyno", WSOFT_WHERE_HUB, nullptr, &r));
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_OK,
                    wsoft_switch_request(WSOFT_SWITCH_APPROVAL, true, "rhyno", WSOFT_WHERE_BROWSER, "  bench test ", &r));
  TEST_ASSERT_EQUAL_STRING("bench test", r.reason);  // trimmed
  TEST_ASSERT_EQUAL_STRING("rhyno", r.who);
  TEST_ASSERT_TRUE(r.on);
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_APPROVAL, r.which);
  TEST_ASSERT_EQUAL(WSOFT_WHERE_BROWSER, r.where);
}

void test_reason_length_is_1_to_80() {
  WsoftSwitchRequest r;
  char reason[90];
  memset(reason, 'a', 80);
  reason[80] = 0;
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_OK, wsoft_switch_request(WSOFT_SWITCH_APPROVAL, true, "x", WSOFT_WHERE_HUB, reason, &r));
  TEST_ASSERT_EQUAL(80, (int)strlen(r.reason));
  reason[80] = 'a';
  reason[81] = 0;
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_REASON_TOO_LONG,
                    wsoft_switch_request(WSOFT_SWITCH_APPROVAL, true, "x", WSOFT_WHERE_HUB, reason, &r));
  // Auto save: the reason is optional, but still at most 80.
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_REASON_TOO_LONG,
                    wsoft_switch_request(WSOFT_SWITCH_AUTO_SAVE, true, "x", WSOFT_WHERE_HUB, reason, &r));
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_OK, wsoft_switch_request(WSOFT_SWITCH_AUTO_SAVE, true, "x", WSOFT_WHERE_HUB, "", &r));
  TEST_ASSERT_EQUAL_STRING("", r.reason);
}

void test_control_characters_cannot_break_the_log_line() {
  WsoftSwitchRequest r;
  // The reason is printable ASCII (0x20..0x7e) only, as the browser checks it; anything else is refused.
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_REASON_REQUIRED,
                    wsoft_switch_request(WSOFT_SWITCH_APPROVAL, true, "x", WSOFT_WHERE_HUB, "two\nlines", &r));
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_REASON_REQUIRED,
                    wsoft_switch_request(WSOFT_SWITCH_AUTO_SAVE, true, "x", WSOFT_WHERE_HUB, "caf\xc3\xa9", &r));
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_OK,
                    wsoft_switch_request(WSOFT_SWITCH_APPROVAL, true, "bad name\t", WSOFT_WHERE_HUB, "ok ~!", &r));
  TEST_ASSERT_EQUAL_STRING("ok ~!", r.reason);
  TEST_ASSERT_EQUAL_STRING("bad_name", r.who);  // one word, so the log line stays easy to read back
}

void test_who_is_required_and_capped() {
  WsoftSwitchRequest r;
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_WHO_REQUIRED,
                    wsoft_switch_request(WSOFT_SWITCH_AUTO_SAVE, true, "", WSOFT_WHERE_HUB, "", &r));
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_WHO_REQUIRED,
                    wsoft_switch_request(WSOFT_SWITCH_AUTO_SAVE, true, nullptr, WSOFT_WHERE_HUB, "", &r));
  // Up to 64 is kept whole (a Hub username can be that long).
  char who[80];
  memset(who, 'w', 64);
  who[64] = 0;
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_OK, wsoft_switch_request(WSOFT_SWITCH_AUTO_SAVE, true, who, WSOFT_WHERE_HUB, "", &r));
  TEST_ASSERT_EQUAL_STRING(who, r.who);
  // Longer: cut, and the cut is marked with '~' so the log never hides it.
  memset(who, 'w', 70);
  who[70] = 0;
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_OK, wsoft_switch_request(WSOFT_SWITCH_AUTO_SAVE, true, who, WSOFT_WHERE_HUB, "", &r));
  TEST_ASSERT_EQUAL(64, (int)strlen(r.who));
  TEST_ASSERT_EQUAL('~', r.who[63]);
  TEST_ASSERT_EQUAL('w', r.who[62]);
}

void test_already_means_nothing_changed() {
  WsoftSwitchRequest r;
  wsoft_switch_request(WSOFT_SWITCH_APPROVAL, false, "x", WSOFT_WHERE_USB, "why", &r);
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_ALREADY, wsoft_switch_check(r, state(false, false, false, false)));
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_OK, wsoft_switch_check(r, state(true, false, false, false)));
  wsoft_switch_request(WSOFT_SWITCH_AUTO_SAVE, true, "x", WSOFT_WHERE_SCREEN, "", &r);
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_ALREADY, wsoft_switch_check(r, state(false, true, true, false)));
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_OK, wsoft_switch_check(r, state(false, false, true, false)));
}

void test_rs485_needs_a_complete_setup_to_switch_approval_on() {
  WsoftSwitchRequest on;
  wsoft_switch_request(WSOFT_SWITCH_APPROVAL, true, "x", WSOFT_WHERE_BROWSER, "why", &on);
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_SETUP_INCOMPLETE, wsoft_switch_check(on, state(false, false, true, false)));
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_OK, wsoft_switch_check(on, state(false, false, true, true)));
  // RS232: the indicator owns the setup.
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_OK, wsoft_switch_check(on, state(false, false, false, false)));
  // Switching OFF is never blocked by the setup.
  WsoftSwitchRequest off;
  wsoft_switch_request(WSOFT_SWITCH_APPROVAL, false, "x", WSOFT_WHERE_BROWSER, "why", &off);
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_OK, wsoft_switch_check(off, state(true, false, true, false)));
  // Auto save never needs the setup.
  WsoftSwitchRequest autos;
  wsoft_switch_request(WSOFT_SWITCH_AUTO_SAVE, true, "x", WSOFT_WHERE_HUB, "", &autos);
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_OK, wsoft_switch_check(autos, state(false, false, true, false)));
  // A1 "blocked".
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_SETUP_INCOMPLETE, wsoft_switch_blocked(state(false, false, true, false)));
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_OK, wsoft_switch_blocked(state(false, false, true, true)));
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_OK, wsoft_switch_blocked(state(false, false, false, false)));
}

void test_last_change_wins() {
  // Two places switch one after the other: each is checked against the state the first one left.
  WsoftSwitchState s = state(false, false, false, false);
  WsoftSwitchRequest a, b;
  wsoft_switch_request(WSOFT_SWITCH_AUTO_SAVE, true, "hubadmin", WSOFT_WHERE_HUB, "", &a);
  wsoft_switch_request(WSOFT_SWITCH_AUTO_SAVE, false, "screen", WSOFT_WHERE_SCREEN, "", &b);
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_OK, wsoft_switch_check(a, s));
  s.auto_save = a.on;
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_OK, wsoft_switch_check(b, s));
  s.auto_save = b.on;
  TEST_ASSERT_FALSE(s.auto_save);
}

void test_log_line_format() {
  char line[WSOFT_SWITCH_LOG_MAX];
  TEST_ASSERT_TRUE(wsoft_switch_log_line(4, "2026-11-07T21:00:00Z", false, true, "rhyno", WSOFT_WHERE_BROWSER,
                                         "bench test", line, sizeof line));
  TEST_ASSERT_EQUAL_STRING("4 2026-11-07T21:00:00Z OFF->ON by=rhyno where=browser why=bench test", line);
  TEST_ASSERT_TRUE(wsoft_switch_log_line(5, "no-clock", true, false, "PIN", WSOFT_WHERE_SCREEN, "on the screen", line,
                                         sizeof line));
  TEST_ASSERT_EQUAL_STRING("5 no-clock ON->OFF by=PIN where=screen why=on the screen", line);
  // The longest line still fits.
  char who[WSOFT_SWITCH_WHO_MAX + 1], reason[WSOFT_SWITCH_REASON_MAX + 1];
  memset(who, 'w', WSOFT_SWITCH_WHO_MAX);
  who[WSOFT_SWITCH_WHO_MAX] = 0;
  memset(reason, 'r', WSOFT_SWITCH_REASON_MAX);
  reason[WSOFT_SWITCH_REASON_MAX] = 0;
  TEST_ASSERT_TRUE(wsoft_switch_log_line(4294967295u, "2026-11-07T21:00:00Z", false, true, who, WSOFT_WHERE_BROWSER,
                                         reason, line, sizeof line));
  TEST_ASSERT_FALSE(wsoft_switch_log_line(4, "x", false, true, "rhyno", WSOFT_WHERE_HUB, "why", line, 20));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_names_and_codes);
  RUN_TEST(test_approval_needs_a_reason_both_ways);
  RUN_TEST(test_reason_length_is_1_to_80);
  RUN_TEST(test_control_characters_cannot_break_the_log_line);
  RUN_TEST(test_who_is_required_and_capped);
  RUN_TEST(test_already_means_nothing_changed);
  RUN_TEST(test_rs485_needs_a_complete_setup_to_switch_approval_on);
  RUN_TEST(test_last_change_wins);
  RUN_TEST(test_log_line_format);
  return UNITY_END();
}
