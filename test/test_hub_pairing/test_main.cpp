// Host tests for linking a W-Soft box to the Hub from the web page
// (lib/wsoft_core/src/hub_pairing.cpp).
// Run: pio test -e native   (needs a host g++ on PATH)
//
// The codes below were made exactly like Weighsoft.Indicator.Hub server/src/registry/registry.ts
// does it (base64url of the JSON, secret in plain base64), with a fixed test secret. They are
// not real keys.

#include <unity.h>

#include <string.h>

#include "hub_pairing.h"

// hub_url "http://192.168.1.20:8095/" (trailing slash), device_id dev_wsoft, key hubk_testkey123.
static const char *LAN_CODE =
    "eyJ2IjoxLCJodWJfdXJsIjoiaHR0cDovLzE5Mi4xNjguMS4yMDo4MDk1LyIsIm9yZ2FuaXNhdGlvbl9pZCI6Im9yZ18xIiwiZGV2aWNl"
    "X2lkIjoiZGV2X3dzb2Z0Iiwia2V5X2lkIjoiaHVia190ZXN0a2V5MTIzIiwic2VjcmV0IjoiQVFnUEZoMGtLekk1UUVkT1ZWeGphbkY0"
    "ZjRhTmxKdWlxYkMzdnNYTTA5bz0ifQ";
// Same, hub_url "http://100.120.139.49:8095" (a Tailscale address).
static const char *TAILSCALE_CODE =
    "eyJ2IjoxLCJodWJfdXJsIjoiaHR0cDovLzEwMC4xMjAuMTM5LjQ5OjgwOTUiLCJvcmdhbmlzYXRpb25faWQiOiJvcmdfMSIsImRldmlj"
    "ZV9pZCI6ImRldl93c29mdCIsImtleV9pZCI6Imh1YmtfdGVzdGtleTEyMyIsInNlY3JldCI6IkFRZ1BGaDBrS3pJNVFFZE9WVnhqYW5G"
    "NGY0YU5sSnVpcWJDM3ZzWE0wOW89In0";
// Same as TAILSCALE_CODE but the secret is only 16 bytes.
static const char *SHORT_SECRET_CODE =
    "eyJ2IjoxLCJodWJfdXJsIjoiaHR0cDovLzEwMC4xMjAuMTM5LjQ5OjgwOTUiLCJvcmdhbmlzYXRpb25faWQiOiJvcmdfMSIsImRldmlj"
    "ZV9pZCI6ImRldl93c29mdCIsImtleV9pZCI6Imh1YmtfdGVzdGtleTEyMyIsInNlY3JldCI6IkFBQUFBQUFBQUFBQUFBQUFBQUFBQUE9"
    "PSJ9";

void setUp() {}
void tearDown() {}

void test_real_hub_code_is_accepted_and_fields_are_read() {
  HubPairing p;
  TEST_ASSERT_EQUAL(HUB_CODE_OK, hub_code_parse(LAN_CODE, &p));
  TEST_ASSERT_EQUAL_STRING("http://192.168.1.20:8095", p.hub_url);  // trailing slash removed
  TEST_ASSERT_EQUAL_STRING("hubk_testkey123", p.key_id);
  TEST_ASSERT_EQUAL_STRING("dev_wsoft", p.device_id);
  for (int i = 0; i < 32; i++) TEST_ASSERT_EQUAL_UINT8(i * 7 + 1, p.secret[i]);
}

void test_spaces_and_line_breaks_around_a_pasted_code_are_ignored() {
  char pasted[700];
  strcpy(pasted, "  \r\n");
  strcat(pasted, LAN_CODE);
  strcat(pasted, " \n");
  HubPairing p;
  TEST_ASSERT_EQUAL(HUB_CODE_OK, hub_code_parse(pasted, &p));
  TEST_ASSERT_EQUAL_STRING("hubk_testkey123", p.key_id);
}

void test_empty_and_blank_are_refused() {
  HubPairing p;
  TEST_ASSERT_EQUAL(HUB_CODE_EMPTY, hub_code_parse("", &p));
  TEST_ASSERT_EQUAL(HUB_CODE_EMPTY, hub_code_parse("   \n", &p));
  TEST_ASSERT_EQUAL(HUB_CODE_EMPTY, hub_code_parse(nullptr, &p));
}

void test_cut_off_code_is_refused() {
  char cut[200];
  strncpy(cut, LAN_CODE, 120);
  cut[120] = 0;
  HubPairing p;
  TEST_ASSERT_NOT_EQUAL(HUB_CODE_OK, hub_code_parse(cut, &p));
}

void test_garbage_is_refused() {
  HubPairing p;
  TEST_ASSERT_EQUAL(HUB_CODE_NOT_BASE64, hub_code_parse("this is not a code!", &p));
  // "hello world" in base64: decodes, but is not the Hub's JSON.
  TEST_ASSERT_EQUAL(HUB_CODE_NOT_JSON, hub_code_parse("aGVsbG8gd29ybGQ", &p));
}

void test_too_long_is_refused() {
  char long_code[HUB_CODE_MAX + 10];
  memset(long_code, 'A', sizeof long_code - 1);
  long_code[sizeof long_code - 1] = 0;
  HubPairing p;
  TEST_ASSERT_EQUAL(HUB_CODE_TOO_LONG, hub_code_parse(long_code, &p));
}

void test_short_secret_is_refused() {
  HubPairing p;
  TEST_ASSERT_EQUAL(HUB_CODE_BAD_SECRET, hub_code_parse(SHORT_SECRET_CODE, &p));
}

void test_missing_url_or_key_is_refused() {
  HubPairing p;
  // {"hub_url":"ftp://x","key_id":"k","secret":"..."} -> base64url
  TEST_ASSERT_EQUAL(HUB_CODE_NO_URL, hub_code_parse("eyJodWJfdXJsIjoiZnRwOi8veCIsImtleV9pZCI6ImsifQ", &p));
  // {"hub_url":"http://192.168.1.5"}
  TEST_ASSERT_EQUAL(HUB_CODE_NO_KEY, hub_code_parse("eyJodWJfdXJsIjoiaHR0cDovLzE5Mi4xNjguMS41In0", &p));
}

void test_url_hints() {
  TEST_ASSERT_EQUAL(HUB_URL_FINE, hub_url_hint("http://192.168.1.20:8095"));
  TEST_ASSERT_EQUAL(HUB_URL_FINE, hub_url_hint("https://hub.example.com"));
  TEST_ASSERT_EQUAL(HUB_URL_FINE, hub_url_hint("http://100.20.1.1"));  // public 100.x, not Tailscale
  TEST_ASSERT_EQUAL(HUB_URL_TAILSCALE, hub_url_hint("http://100.120.139.49:8095"));
  TEST_ASSERT_EQUAL(HUB_URL_TAILSCALE, hub_url_hint("http://100.64.0.1"));
  TEST_ASSERT_EQUAL(HUB_URL_LOCALHOST, hub_url_hint("http://localhost:8095"));
  TEST_ASSERT_EQUAL(HUB_URL_LOCALHOST, hub_url_hint("http://127.0.0.1:8095"));
  TEST_ASSERT_TRUE(strlen(hub_url_hint_text(HUB_URL_TAILSCALE)) > 0);
  TEST_ASSERT_EQUAL_STRING("", hub_url_hint_text(HUB_URL_FINE));

  HubPairing p;
  TEST_ASSERT_EQUAL(HUB_CODE_OK, hub_code_parse(TAILSCALE_CODE, &p));
  TEST_ASSERT_EQUAL(HUB_URL_TAILSCALE, hub_url_hint(p.hub_url));
}

void test_base64_both_alphabets_and_padding() {
  uint8_t out[8];
  size_t len = 0;
  TEST_ASSERT_TRUE(hub_base64url_decode("-_8", 3, out, sizeof out, &len));  // base64url
  TEST_ASSERT_EQUAL(2, len);
  TEST_ASSERT_EQUAL_UINT8(0xFB, out[0]);
  TEST_ASSERT_EQUAL_UINT8(0xFF, out[1]);
  TEST_ASSERT_TRUE(hub_base64url_decode("+/8=", 4, out, sizeof out, &len));  // plain base64
  TEST_ASSERT_EQUAL(2, len);
  TEST_ASSERT_FALSE(hub_base64url_decode("A", 1, out, sizeof out, &len));     // 6 bits alone
  TEST_ASSERT_FALSE(hub_base64url_decode("AA=A", 4, out, sizeof out, &len));  // data after padding
  TEST_ASSERT_FALSE(hub_base64url_decode("AAAAAAAAAAAA", 12, out, 4, &len));  // does not fit
}

void test_http_date_from_the_hub_sets_the_clock() {
  time_t t = 0;
  TEST_ASSERT_TRUE(hub_parse_http_date("Sat, 31 Oct 2026 08:05:56 GMT", &t));
  TEST_ASSERT_EQUAL_INT64(1793433956LL, (long long)t);
  TEST_ASSERT_TRUE(hub_parse_http_date("Thu, 01 Jan 2026 00:00:00 GMT", &t));
  TEST_ASSERT_EQUAL_INT64(1767225600LL, (long long)t);
  TEST_ASSERT_TRUE(hub_parse_http_date("Thu, 29 Feb 2028 12:00:00 GMT", &t));
  TEST_ASSERT_EQUAL_INT64(1835438400LL, (long long)t);
}

void test_bad_http_dates_are_refused() {
  time_t t = 0;
  TEST_ASSERT_FALSE(hub_parse_http_date("", &t));
  TEST_ASSERT_FALSE(hub_parse_http_date(nullptr, &t));
  TEST_ASSERT_FALSE(hub_parse_http_date("Sat, 31 Foo 2026 08:05:56 GMT", &t));
  TEST_ASSERT_FALSE(hub_parse_http_date("Sat, 31 Oct 2026 08:05:56 PST", &t));
  TEST_ASSERT_FALSE(hub_parse_http_date("Sat, 31 Oct 1999 08:05:56 GMT", &t));  // before any real Hub
  TEST_ASSERT_FALSE(hub_parse_http_date("2026-10-31T08:05:56Z", &t));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_real_hub_code_is_accepted_and_fields_are_read);
  RUN_TEST(test_spaces_and_line_breaks_around_a_pasted_code_are_ignored);
  RUN_TEST(test_empty_and_blank_are_refused);
  RUN_TEST(test_cut_off_code_is_refused);
  RUN_TEST(test_garbage_is_refused);
  RUN_TEST(test_too_long_is_refused);
  RUN_TEST(test_short_secret_is_refused);
  RUN_TEST(test_missing_url_or_key_is_refused);
  RUN_TEST(test_url_hints);
  RUN_TEST(test_base64_both_alphabets_and_padding);
  RUN_TEST(test_http_date_from_the_hub_sets_the_clock);
  RUN_TEST(test_bad_http_dates_are_refused);
  return UNITY_END();
}
