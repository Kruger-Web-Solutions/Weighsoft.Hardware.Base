// Host tests for the Hub Date header reader (lib/wsoft_core/src/wsoft_http_date.cpp).
// Run: pio test -e native   (needs a host g++ on PATH)

#include <unity.h>

#include "wsoft_http_date.h"

void setUp() {}
void tearDown() {}

void test_hub_date_is_read_as_utc_seconds() {
  time_t t = 0;
  TEST_ASSERT_TRUE(wsoft_parse_http_date("Sat, 31 Oct 2026 08:05:56 GMT", &t));
  TEST_ASSERT_EQUAL_INT64(1793433956LL, (long long)t);
  TEST_ASSERT_TRUE(wsoft_parse_http_date("Thu, 01 Jan 2026 00:00:00 GMT", &t));
  TEST_ASSERT_EQUAL_INT64(1767225600LL, (long long)t);
  TEST_ASSERT_TRUE(wsoft_parse_http_date("Tue, 29 Feb 2028 12:00:00 GMT", &t));
  TEST_ASSERT_EQUAL_INT64(1835438400LL, (long long)t);
  TEST_ASSERT_TRUE(wsoft_parse_http_date("Wed, 18 Nov 2026 23:59:59 GMT", &t));
  TEST_ASSERT_EQUAL_INT64(1795046399LL, (long long)t);
}

void test_bad_dates_are_refused() {
  time_t t = 123;
  TEST_ASSERT_FALSE(wsoft_parse_http_date("", &t));
  TEST_ASSERT_FALSE(wsoft_parse_http_date(nullptr, &t));
  TEST_ASSERT_FALSE(wsoft_parse_http_date("Sat, 31 Oct 2026 08:05:56 GMT", nullptr));
  TEST_ASSERT_FALSE(wsoft_parse_http_date("Sat, 31 Foo 2026 08:05:56 GMT", &t));
  TEST_ASSERT_FALSE(wsoft_parse_http_date("Sat, 31 Oct 2026 08:05:56 PST", &t));
  TEST_ASSERT_FALSE(wsoft_parse_http_date("Sat, 31 Oct 1999 08:05:56 GMT", &t));  // before any real Hub
  TEST_ASSERT_FALSE(wsoft_parse_http_date("Sat, 31 Oct 2026 24:00:00 GMT", &t));
  TEST_ASSERT_FALSE(wsoft_parse_http_date("Sat, 00 Oct 2026 08:05:56 GMT", &t));
  TEST_ASSERT_FALSE(wsoft_parse_http_date("2026-10-31T08:05:56Z", &t));
  TEST_ASSERT_EQUAL_INT64(123LL, (long long)t);  // left alone on failure
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_hub_date_is_read_as_utc_seconds);
  RUN_TEST(test_bad_dates_are_refused);
  return UNITY_END();
}
