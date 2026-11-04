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
  return UNITY_END();
}
