// Host tests for the Wi-Fi update rules (lib/wsoft_core/src/wsoft_wifi_update.cpp): the
// switch is OFF until the board has its own password, never works with Approval ON, and a
// one-copy board (the yellow board) stays on the cable.
// Run: pio test -e native   (needs a host g++ on PATH)

#include <unity.h>

#include <string.h>

#include "wsoft_wifi_update.h"

void setUp() {}
void tearDown() {}

static WsoftWifiUpdateState state(bool two_copies, bool approval_on, bool on, bool password_set) {
  WsoftWifiUpdateState s;
  s.two_copies = two_copies;
  s.approval_on = approval_on;
  s.on = on;
  s.password_set = password_set;
  return s;
}

static WsoftWifiUpdateRequest request(int on, const char *password) {
  WsoftWifiUpdateRequest r;
  r.has_on = on >= 0;
  r.on = on > 0;
  r.password = password;
  return r;
}

static const char *const GOOD = "Blue-Kettle-42x";  // a made-up example, not a real password

void test_default_state_is_off_and_not_listening() {
  WsoftWifiUpdateState s;
  TEST_ASSERT_FALSE(s.on);
  TEST_ASSERT_FALSE(s.password_set);
  TEST_ASSERT_FALSE(wsoft_wifi_update_listening(s));
}

void test_codes_and_texts() {
  TEST_ASSERT_EQUAL_STRING("", wsoft_wifi_update_error_code(WSOFT_WIFI_UPDATE_OK));
  TEST_ASSERT_EQUAL_STRING("ONE_COPY", wsoft_wifi_update_error_code(WSOFT_WIFI_UPDATE_ONE_COPY));
  TEST_ASSERT_EQUAL_STRING("APPROVAL_ON", wsoft_wifi_update_error_code(WSOFT_WIFI_UPDATE_APPROVAL_ON));
  TEST_ASSERT_EQUAL_STRING("PASSWORD_FORMAT", wsoft_wifi_update_error_code(WSOFT_WIFI_UPDATE_PASSWORD_FORMAT));
  TEST_ASSERT_EQUAL_STRING("PASSWORD_FACTORY", wsoft_wifi_update_error_code(WSOFT_WIFI_UPDATE_PASSWORD_FACTORY));
  TEST_ASSERT_EQUAL_STRING("PASSWORD_REQUIRED", wsoft_wifi_update_error_code(WSOFT_WIFI_UPDATE_PASSWORD_REQUIRED));
  TEST_ASSERT_EQUAL_STRING("NOTHING", wsoft_wifi_update_error_code(WSOFT_WIFI_UPDATE_NOTHING));
  TEST_ASSERT_EQUAL_STRING("BUSY", wsoft_wifi_update_error_code(WSOFT_WIFI_UPDATE_BUSY));
  for (int e = WSOFT_WIFI_UPDATE_ONE_COPY; e <= WSOFT_WIFI_UPDATE_BUSY; e++) {
    const char *code = wsoft_wifi_update_error_code((WsoftWifiUpdateError)e);
    TEST_ASSERT_TRUE(strlen(code) >= 1 && strlen(code) <= 32);
    TEST_ASSERT_TRUE(strlen(wsoft_wifi_update_error_text((WsoftWifiUpdateError)e)) > 0);
  }
}

void test_password_rules() {
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_OK, wsoft_wifi_update_password_check(GOOD));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_PASSWORD_FORMAT, wsoft_wifi_update_password_check(nullptr));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_PASSWORD_FORMAT, wsoft_wifi_update_password_check(""));
  // The framework's factory password is refused by name, alone or inside a longer one.
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_PASSWORD_FACTORY, wsoft_wifi_update_password_check("esp-react"));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_PASSWORD_FACTORY, wsoft_wifi_update_password_check("esp-react-esp-react"));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_PASSWORD_FACTORY,
                    wsoft_wifi_update_password_check("change-me-at-least-8-chars"));
  // 4 or more (Rhyno, 21 Nov 2026: "4 + is good for us"; it was 12).
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_PASSWORD_FORMAT, wsoft_wifi_update_password_check("abc"));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_OK, wsoft_wifi_update_password_check("abcd"));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_OK, wsoft_wifi_update_password_check("twelvechars!"));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_PASSWORD_FORMAT, wsoft_wifi_update_password_check("has a space in it"));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_PASSWORD_FORMAT, wsoft_wifi_update_password_check("tab\tinside-here"));
  char longest[WSOFT_WIFI_UPDATE_PASSWORD_MAX + 2];
  memset(longest, 'a', sizeof longest);
  longest[WSOFT_WIFI_UPDATE_PASSWORD_MAX] = 0;
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_OK, wsoft_wifi_update_password_check(longest));
  longest[WSOFT_WIFI_UPDATE_PASSWORD_MAX] = 'a';
  longest[WSOFT_WIFI_UPDATE_PASSWORD_MAX + 1] = 0;
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_PASSWORD_FORMAT, wsoft_wifi_update_password_check(longest));
}

void test_on_needs_the_boards_own_password() {
  WsoftWifiUpdateChange c;
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_PASSWORD_REQUIRED,
                    wsoft_wifi_update_check(request(1, nullptr), state(true, false, false, false), &c));
  // ON and the password in one save.
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_OK,
                    wsoft_wifi_update_check(request(1, GOOD), state(true, false, false, false), &c));
  TEST_ASSERT_TRUE(c.on);
  TEST_ASSERT_TRUE(c.on_changed);
  TEST_ASSERT_TRUE(c.password_changed);
  // ON with the password already there.
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_OK,
                    wsoft_wifi_update_check(request(1, nullptr), state(true, false, false, true), &c));
  TEST_ASSERT_TRUE(c.on && c.on_changed);
  TEST_ASSERT_FALSE(c.password_changed);
  // A bad password with ON: nothing goes ON.
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_PASSWORD_FORMAT,
                    wsoft_wifi_update_check(request(1, "abc"), state(true, false, false, true), &c));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_PASSWORD_FACTORY,
                    wsoft_wifi_update_check(request(1, "esp-react-esp-react"), state(true, false, false, false), &c));
}

void test_blocked_while_approval_is_on() {
  WsoftWifiUpdateChange c;
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_APPROVAL_ON,
                    wsoft_wifi_update_check(request(1, nullptr), state(true, true, false, true), &c));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_APPROVAL_ON,
                    wsoft_wifi_update_check(request(-1, GOOD), state(true, true, true, true), &c));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_APPROVAL_ON,
                    wsoft_wifi_update_check(request(0, GOOD), state(true, true, true, true), &c));
  // The switch may be ON in flash, but nothing listens while Approval is ON.
  TEST_ASSERT_FALSE(wsoft_wifi_update_listening(state(true, true, true, true)));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_APPROVAL_ON, wsoft_wifi_update_blocked(state(true, true, true, true)));
  // Approval OFF again: it listens again.
  TEST_ASSERT_TRUE(wsoft_wifi_update_listening(state(true, false, true, true)));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_OK, wsoft_wifi_update_blocked(state(true, false, true, true)));
}

void test_off_is_always_allowed() {
  WsoftWifiUpdateChange c;
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_OK,
                    wsoft_wifi_update_check(request(0, nullptr), state(true, true, true, true), &c));
  TEST_ASSERT_FALSE(c.on);
  TEST_ASSERT_TRUE(c.on_changed);
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_OK,
                    wsoft_wifi_update_check(request(0, nullptr), state(false, false, true, true), &c));
  TEST_ASSERT_FALSE(c.on);
  // Already OFF: allowed, changes nothing.
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_OK,
                    wsoft_wifi_update_check(request(0, nullptr), state(true, false, false, false), &c));
  TEST_ASSERT_FALSE(c.on_changed);
  WsoftWifiUpdateEvent event;
  TEST_ASSERT_FALSE(wsoft_wifi_update_event(c, &event));
}

void test_one_copy_board_stays_on_the_cable() {
  WsoftWifiUpdateChange c;
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_ONE_COPY,
                    wsoft_wifi_update_check(request(1, GOOD), state(false, false, false, false), &c));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_ONE_COPY,
                    wsoft_wifi_update_check(request(-1, GOOD), state(false, false, false, false), &c));
  // Even with a switch and password left in flash by other software.
  TEST_ASSERT_FALSE(wsoft_wifi_update_listening(state(false, false, true, true)));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_ONE_COPY, wsoft_wifi_update_blocked(state(false, false, true, true)));
}

void test_listening_needs_everything() {
  TEST_ASSERT_TRUE(wsoft_wifi_update_listening(state(true, false, true, true)));
  TEST_ASSERT_FALSE(wsoft_wifi_update_listening(state(true, false, false, true)));
  TEST_ASSERT_FALSE(wsoft_wifi_update_listening(state(true, false, true, false)));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_PASSWORD_REQUIRED, wsoft_wifi_update_blocked(state(true, false, true, false)));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_OK, wsoft_wifi_update_blocked(state(true, false, false, false)));
}

void test_empty_request_and_password_only() {
  WsoftWifiUpdateChange c;
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_NOTHING,
                    wsoft_wifi_update_check(request(-1, nullptr), state(true, false, false, false), &c));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_OK,
                    wsoft_wifi_update_check(request(-1, GOOD), state(true, false, true, true), &c));
  TEST_ASSERT_TRUE(c.on);  // the switch keeps its value
  TEST_ASSERT_FALSE(c.on_changed);
  TEST_ASSERT_TRUE(c.password_changed);
}

void test_events() {
  WsoftWifiUpdateChange c;
  WsoftWifiUpdateEvent event;
  wsoft_wifi_update_check(request(1, GOOD), state(true, false, false, false), &c);
  TEST_ASSERT_TRUE(wsoft_wifi_update_event(c, &event));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_EVENT_ON_PASSWORD, event);
  wsoft_wifi_update_check(request(1, nullptr), state(true, false, false, true), &c);
  TEST_ASSERT_TRUE(wsoft_wifi_update_event(c, &event));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_EVENT_ON, event);
  wsoft_wifi_update_check(request(0, nullptr), state(true, false, true, true), &c);
  TEST_ASSERT_TRUE(wsoft_wifi_update_event(c, &event));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_EVENT_OFF, event);
  wsoft_wifi_update_check(request(-1, GOOD), state(true, false, true, true), &c);
  TEST_ASSERT_TRUE(wsoft_wifi_update_event(c, &event));
  TEST_ASSERT_EQUAL(WSOFT_WIFI_UPDATE_EVENT_PASSWORD, event);
}

void test_log_line_never_holds_a_password() {
  char line[WSOFT_WIFI_UPDATE_LOG_MAX];
  TEST_ASSERT_TRUE(wsoft_wifi_update_log_line(9, "2026-11-14T18:00:00Z", WSOFT_WIFI_UPDATE_EVENT_INSTALLED, "wifi",
                                              "0.7.0", line, sizeof line));
  TEST_ASSERT_EQUAL_STRING("9 2026-11-14T18:00:00Z WIFI-UPDATE INSTALLED by=wifi was=0.7.0", line);
  TEST_ASSERT_TRUE(wsoft_wifi_update_log_line(10, "no-clock", WSOFT_WIFI_UPDATE_EVENT_ON_PASSWORD, "rhyno page",
                                              "0.7.0", line, sizeof line));
  TEST_ASSERT_EQUAL_STRING("10 no-clock WIFI-UPDATE ON+NEW-PASSWORD by=rhyno_page was=0.7.0", line);
  TEST_ASSERT_TRUE(wsoft_wifi_update_log_line(11, "no-clock", WSOFT_WIFI_UPDATE_EVENT_OFF, "", "0.7.0", line,
                                              sizeof line));
  TEST_ASSERT_EQUAL_STRING("11 no-clock WIFI-UPDATE OFF by=unknown was=0.7.0", line);
  TEST_ASSERT_TRUE(wsoft_wifi_update_log_line(12, "no-clock", WSOFT_WIFI_UPDATE_EVENT_PASSWORD, nullptr, nullptr,
                                              line, sizeof line));
  TEST_ASSERT_EQUAL_STRING("12 no-clock WIFI-UPDATE NEW-PASSWORD by=unknown was=", line);
  // The longest line fits the buffer the board uses.
  char who[200];
  memset(who, 'w', sizeof who);
  who[sizeof who - 1] = 0;
  TEST_ASSERT_TRUE(wsoft_wifi_update_log_line(4294967295u, "2026-11-14T18:00:00Z",
                                              WSOFT_WIFI_UPDATE_EVENT_ON_PASSWORD, who,
                                              "0.7.0-very-long-version-name", line, sizeof line));
  TEST_ASSERT_TRUE(strlen(line) < WSOFT_WIFI_UPDATE_LOG_MAX);
  char tiny[8];
  TEST_ASSERT_FALSE(wsoft_wifi_update_log_line(1, "no-clock", WSOFT_WIFI_UPDATE_EVENT_ON, "a", "b", tiny, sizeof tiny));
  TEST_ASSERT_EQUAL_STRING("", tiny);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_default_state_is_off_and_not_listening);
  RUN_TEST(test_codes_and_texts);
  RUN_TEST(test_password_rules);
  RUN_TEST(test_on_needs_the_boards_own_password);
  RUN_TEST(test_blocked_while_approval_is_on);
  RUN_TEST(test_off_is_always_allowed);
  RUN_TEST(test_one_copy_board_stays_on_the_cable);
  RUN_TEST(test_listening_needs_everything);
  RUN_TEST(test_empty_request_and_password_only);
  RUN_TEST(test_events);
  RUN_TEST(test_log_line_never_holds_a_password);
  return UNITY_END();
}
