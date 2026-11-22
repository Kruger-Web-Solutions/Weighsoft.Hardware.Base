// Host tests for the Hub pairing-key reader (lib/wsoft_core/src/wsoft_hub_code.cpp).
// Run: pio test -e native   (needs a host g++ on PATH)
// Fixtures are made like the Hub makes them (registry.ts issuePairingCode), with a fake
// secret of bytes 0..31. They are test data, not real keys.

#include <unity.h>

#include <stdio.h>
#include <string.h>

#include "wsoft_hub_code.h"

static const char *VALID =
    "eyJ2IjoxLCJodWJfdXJsIjoiaHR0cDovLzE5Mi4xNjguMC4xMjo4MDk1LyIsIm9yZ2FuaXNhdGlvbl9pZCI6Im9yZ18xIiwiZGV2aWNlX2lkIjoi"
    "ZGV2XzEiLCJrZXlfaWQiOiJodWJrX3Rlc3QiLCJzZWNyZXQiOiJBQUVDQXdRRkJnY0lDUW9MREEwT0R4QVJFaE1VRlJZWEdCa2FHeHdkSGg4PSJ9";
static const char *HTTPS =
    "eyJ2IjoxLCJodWJfdXJsIjoiaHR0cHM6Ly9odWIuZXhhbXBsZSIsImtleV9pZCI6Imh1YmtfeCIsInNlY3JldCI6IkFBRUNBd1FGQmdjSUNRb0xE"
    "QTBPRHhBUkVoTVVGUllYR0JrYUd4d2RIaDg9In0";
static const char *NO_URL =
    "eyJ2IjoxLCJrZXlfaWQiOiJrIiwic2VjcmV0IjoiQUFFQ0F3UUZCZ2NJQ1FvTERBME9EeEFSRWhNVUZSWVhHQmthR3h3ZEhoOD0ifQ";
static const char *FTP_URL =
    "eyJ2IjoxLCJodWJfdXJsIjoiZnRwOi8veCIsImtleV9pZCI6ImsiLCJzZWNyZXQiOiJBQUVDQXdRRkJnY0lDUW9MREEwT0R4QVJFaE1VRlJZWEdC"
    "a2FHeHdkSGg4PSJ9";
static const char *NO_KEY_ID =
    "eyJ2IjoxLCJodWJfdXJsIjoiaHR0cDovL3giLCJzZWNyZXQiOiJBQUVDQXdRRkJnY0lDUW9MREEwT0R4QVJFaE1VRlJZWEdCa2FHeHdkSGg4PSJ9";
static const char *SHORT_SECRET =
    "eyJ2IjoxLCJodWJfdXJsIjoiaHR0cDovL3giLCJrZXlfaWQiOiJrIiwic2VjcmV0IjoiQUFBQUFBQUFBQUFBQUFBQUFBQUFBQT09In0";
static const char *NOT_JSON = "aGVsbG8gd29ybGQ";

// Backup addresses (14 Nov 2026). A newer Hub adds "hub_urls": every address it listens on.
static const char *TWO_LAN =
    "eyJ2IjoxLCJodWJfdXJsIjoiaHR0cDovLzE5Mi4xNjguMC4xMjo4MDk1IiwiaHViX3VybHMiOlsiaHR0cDovLzE5Mi4xNjguMC4xMjo4MDk1Iiwi"
    "aHR0cDovLzE5Mi4xNjguMS4yOjgwOTUiLCJodHRwOi8vMTAwLjEyMC4xMzkuNDk6ODA5NSJdLCJvcmdhbmlzYXRpb25faWQiOiJvcmdfMSIsImRl"
    "dmljZV9pZCI6ImRldl8xIiwia2V5X2lkIjoiaHVia190ZXN0Iiwic2VjcmV0IjoiQUFFQ0F3UUZCZ2NJQ1FvTERBME9EeEFSRWhNVUZSWVhHQmth"
    "R3h3ZEhoOD0ifQ";
static const char *TAILSCALE_FIRST =
    "eyJ2IjoxLCJodWJfdXJsIjoiaHR0cDovLzEwMC4xMjAuMTM5LjQ5OjgwOTUiLCJodWJfdXJscyI6WyJodHRwOi8vMTAwLjEyMC4xMzkuNDk6ODA5"
    "NSIsImh0dHA6Ly8xOTIuMTY4LjEuMjo4MDk1LyJdLCJvcmdhbmlzYXRpb25faWQiOiJvcmdfMSIsImRldmljZV9pZCI6ImRldl8xIiwia2V5X2lk"
    "IjoiaHVia190ZXN0Iiwic2VjcmV0IjoiQUFFQ0F3UUZCZ2NJQ1FvTERBME9EeEFSRWhNVUZSWVhHQmthR3h3ZEhoOD0ifQ";
static const char *TAILSCALE_ONLY =
    "eyJ2IjoxLCJodWJfdXJsIjoiaHR0cDovLzEwMC4xMjAuMTM5LjQ5OjgwOTUiLCJvcmdhbmlzYXRpb25faWQiOiJvcmdfMSIsImRldmljZV9pZCI6"
    "ImRldl8xIiwia2V5X2lkIjoiaHVia190ZXN0Iiwic2VjcmV0IjoiQUFFQ0F3UUZCZ2NJQ1FvTERBME9EeEFSRWhNVUZSWVhHQmthR3h3ZEhoOD0ifQ";
static const char *MANY =
    "eyJ2IjoxLCJodWJfdXJsIjoiaHR0cDovLzEwLjAuMC4xIiwiaHViX3VybHMiOlsiaHR0cDovLzEwLjAuMC4xIiwiaHR0cDovLzEwLjAuMC4yIiwi"
    "aHR0cDovLzEwLjAuMC4yIiwiaHR0cDovLzEwLjAuMC4zIiwiZnRwOi8veCIsImh0dHA6Ly8xMC4wLjAuNCIsImh0dHA6Ly8xMC4wLjAuNSJdLCJr"
    "ZXlfaWQiOiJrIiwic2VjcmV0IjoiQUFFQ0F3UUZCZ2NJQ1FvTERBME9EeEFSRWhNVUZSWVhHQmthR3h3ZEhoOD0ifQ";

void setUp() {}
void tearDown() {}

void test_valid_code_gives_url_key_device_and_secret() {
  WsoftHubCode c;
  TEST_ASSERT_EQUAL(WSOFT_HUB_CODE_OK, wsoft_hub_code_parse(VALID, &c));
  TEST_ASSERT_EQUAL_STRING("http://192.168.0.12:8095", c.hub_url);  // trailing '/' dropped
  TEST_ASSERT_EQUAL_STRING("hubk_test", c.key_id);
  TEST_ASSERT_EQUAL_STRING("dev_1", c.device_id);
  for (int i = 0; i < 32; i++) TEST_ASSERT_EQUAL_UINT8(i, c.secret[i]);
}

void test_https_code_without_device_id() {
  WsoftHubCode c;
  TEST_ASSERT_EQUAL(WSOFT_HUB_CODE_OK, wsoft_hub_code_parse(HTTPS, &c));
  TEST_ASSERT_EQUAL_STRING("https://hub.example", c.hub_url);
  TEST_ASSERT_EQUAL_STRING("", c.device_id);
}

void test_pasted_spaces_and_newlines_are_ignored() {
  char pasted[400];
  snprintf(pasted, sizeof pasted, "  \n%s\r\n ", VALID);
  TEST_ASSERT_EQUAL(WSOFT_HUB_CODE_OK, wsoft_hub_code_parse(pasted, nullptr));
}

void test_empty_and_null() {
  TEST_ASSERT_EQUAL(WSOFT_HUB_CODE_EMPTY, wsoft_hub_code_parse(nullptr, nullptr));
  TEST_ASSERT_EQUAL(WSOFT_HUB_CODE_EMPTY, wsoft_hub_code_parse("", nullptr));
  TEST_ASSERT_EQUAL(WSOFT_HUB_CODE_EMPTY, wsoft_hub_code_parse(" \n ", nullptr));
}

void test_too_long() {
  static char longer[WSOFT_HUB_CODE_MAX + 2];
  memset(longer, 'A', WSOFT_HUB_CODE_MAX + 1);
  longer[WSOFT_HUB_CODE_MAX + 1] = 0;
  TEST_ASSERT_EQUAL(WSOFT_HUB_CODE_TOO_LONG, wsoft_hub_code_parse(longer, nullptr));
}

void test_not_a_code() {
  TEST_ASSERT_EQUAL(WSOFT_HUB_CODE_NOT_BASE64, wsoft_hub_code_parse("not a code!", nullptr));
  TEST_ASSERT_EQUAL(WSOFT_HUB_CODE_NOT_BASE64, wsoft_hub_code_parse(NOT_JSON, nullptr));
}

void test_missing_or_bad_parts() {
  TEST_ASSERT_EQUAL(WSOFT_HUB_CODE_NO_URL, wsoft_hub_code_parse(NO_URL, nullptr));
  TEST_ASSERT_EQUAL(WSOFT_HUB_CODE_NO_URL, wsoft_hub_code_parse(FTP_URL, nullptr));
  TEST_ASSERT_EQUAL(WSOFT_HUB_CODE_NO_KEY_ID, wsoft_hub_code_parse(NO_KEY_ID, nullptr));
  TEST_ASSERT_EQUAL(WSOFT_HUB_CODE_BAD_SECRET, wsoft_hub_code_parse(SHORT_SECRET, nullptr));
}

void test_refused_code_leaves_output_untouched() {
  WsoftHubCode c;
  memset(&c, 0x5A, sizeof c);
  TEST_ASSERT_EQUAL(WSOFT_HUB_CODE_BAD_SECRET, wsoft_hub_code_parse(SHORT_SECRET, &c));
  TEST_ASSERT_EQUAL_UINT8(0x5A, c.secret[0]);
  TEST_ASSERT_EQUAL_UINT8(0x5A, (uint8_t)c.hub_url[0]);
}

void test_every_error_has_text() {
  for (int e = WSOFT_HUB_CODE_OK; e <= WSOFT_HUB_CODE_BAD_SECRET; e++) {
    const char *text = wsoft_hub_code_error_text((WsoftHubCodeError)e);
    TEST_ASSERT_TRUE(text != nullptr);
    TEST_ASSERT_TRUE(strlen(text) > 0);
    TEST_ASSERT_TRUE(strcmp(text, "Unknown error.") != 0);
  }
}

void test_old_single_address_code_lists_just_that_address() {
  WsoftHubCode c;
  TEST_ASSERT_EQUAL(WSOFT_HUB_CODE_OK, wsoft_hub_code_parse(VALID, &c));
  TEST_ASSERT_EQUAL_UINT8(1, c.hub_url_count);
  TEST_ASSERT_EQUAL_STRING("http://192.168.0.12:8095", c.hub_urls[0]);
}

void test_lan_addresses_become_backups_tailscale_is_skipped() {
  WsoftHubCode c;
  TEST_ASSERT_EQUAL(WSOFT_HUB_CODE_OK, wsoft_hub_code_parse(TWO_LAN, &c));
  TEST_ASSERT_EQUAL_UINT8(2, c.hub_url_count);
  TEST_ASSERT_EQUAL_STRING("http://192.168.0.12:8095", c.hub_url);
  TEST_ASSERT_EQUAL_STRING("http://192.168.0.12:8095", c.hub_urls[0]);
  TEST_ASSERT_EQUAL_STRING("http://192.168.1.2:8095", c.hub_urls[1]);
}

void test_tailscale_main_address_gives_way_to_a_lan_one() {
  WsoftHubCode c;
  TEST_ASSERT_EQUAL(WSOFT_HUB_CODE_OK, wsoft_hub_code_parse(TAILSCALE_FIRST, &c));
  TEST_ASSERT_EQUAL_UINT8(1, c.hub_url_count);
  TEST_ASSERT_EQUAL_STRING("http://192.168.1.2:8095", c.hub_url);  // trailing '/' dropped
  TEST_ASSERT_EQUAL_STRING("http://192.168.1.2:8095", c.hub_urls[0]);
}

void test_no_lan_address_keeps_the_main_one_as_before() {
  WsoftHubCode c;
  TEST_ASSERT_EQUAL(WSOFT_HUB_CODE_OK, wsoft_hub_code_parse(TAILSCALE_ONLY, &c));
  TEST_ASSERT_EQUAL_UINT8(1, c.hub_url_count);
  TEST_ASSERT_EQUAL_STRING("http://100.120.139.49:8095", c.hub_urls[0]);
}

void test_list_drops_doubles_and_bad_entries_and_stops_at_four() {
  WsoftHubCode c;
  TEST_ASSERT_EQUAL(WSOFT_HUB_CODE_OK, wsoft_hub_code_parse(MANY, &c));
  TEST_ASSERT_EQUAL_UINT8(WSOFT_HUB_URLS_MAX, c.hub_url_count);
  TEST_ASSERT_EQUAL_STRING("http://10.0.0.1", c.hub_urls[0]);
  TEST_ASSERT_EQUAL_STRING("http://10.0.0.2", c.hub_urls[1]);
  TEST_ASSERT_EQUAL_STRING("http://10.0.0.3", c.hub_urls[2]);
  TEST_ASSERT_EQUAL_STRING("http://10.0.0.4", c.hub_urls[3]);
}

void test_lan_check() {
  TEST_ASSERT_TRUE(wsoft_hub_url_is_lan("http://192.168.1.13:8090"));
  TEST_ASSERT_TRUE(wsoft_hub_url_is_lan("http://100.10.0.1"));    // 100.x outside 100.64/10
  TEST_ASSERT_TRUE(wsoft_hub_url_is_lan("https://hub.example"));
  TEST_ASSERT_FALSE(wsoft_hub_url_is_lan("http://100.64.0.1"));
  TEST_ASSERT_FALSE(wsoft_hub_url_is_lan("http://100.127.255.1:8090"));
  TEST_ASSERT_FALSE(wsoft_hub_url_is_lan("http://localhost:8095"));
  TEST_ASSERT_FALSE(wsoft_hub_url_is_lan("http://127.0.0.1"));
  TEST_ASSERT_FALSE(wsoft_hub_url_is_lan(nullptr));
}

void test_failover_moves_only_on_no_answer_and_wraps() {
  WsoftHubFailover f;
  wsoft_hub_failover_begin(&f, 3);
  TEST_ASSERT_EQUAL_UINT8(0, f.active);
  TEST_ASSERT_FALSE(wsoft_hub_failover_result(&f, 202));
  TEST_ASSERT_FALSE(wsoft_hub_failover_result(&f, 401));  // an answer, even a refusal: the address works
  TEST_ASSERT_FALSE(wsoft_hub_failover_result(&f, 503));
  TEST_ASSERT_EQUAL_UINT8(0, f.active);
  TEST_ASSERT_TRUE(wsoft_hub_failover_result(&f, -1));    // refused / timed out
  TEST_ASSERT_EQUAL_UINT8(1, f.active);
  TEST_ASSERT_TRUE(wsoft_hub_failover_result(&f, 0));
  TEST_ASSERT_EQUAL_UINT8(2, f.active);
  TEST_ASSERT_TRUE(wsoft_hub_failover_result(&f, -11));
  TEST_ASSERT_EQUAL_UINT8(0, f.active);                    // wraps around
}

void test_failover_with_one_address_never_moves() {
  WsoftHubFailover f;
  wsoft_hub_failover_begin(&f, 0);
  TEST_ASSERT_EQUAL_UINT8(1, f.count);
  TEST_ASSERT_FALSE(wsoft_hub_failover_result(&f, -1));
  TEST_ASSERT_EQUAL_UINT8(0, f.active);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_valid_code_gives_url_key_device_and_secret);
  RUN_TEST(test_https_code_without_device_id);
  RUN_TEST(test_pasted_spaces_and_newlines_are_ignored);
  RUN_TEST(test_empty_and_null);
  RUN_TEST(test_too_long);
  RUN_TEST(test_not_a_code);
  RUN_TEST(test_missing_or_bad_parts);
  RUN_TEST(test_refused_code_leaves_output_untouched);
  RUN_TEST(test_every_error_has_text);
  RUN_TEST(test_old_single_address_code_lists_just_that_address);
  RUN_TEST(test_lan_addresses_become_backups_tailscale_is_skipped);
  RUN_TEST(test_tailscale_main_address_gives_way_to_a_lan_one);
  RUN_TEST(test_no_lan_address_keeps_the_main_one_as_before);
  RUN_TEST(test_list_drops_doubles_and_bad_entries_and_stops_at_four);
  RUN_TEST(test_lan_check);
  RUN_TEST(test_failover_moves_only_on_no_answer_and_wraps);
  RUN_TEST(test_failover_with_one_address_never_moves);
  return UNITY_END();
}
