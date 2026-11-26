// Host tests for the Hub Control room screen push (Hub ADR 0010, contract hub-screen-v1,
// lib/wsoft_core/src/wsoft_hub_screen.cpp). The body and signature vector are the shared fixture
// hub-screen-v1-fixtures.json (next to this file, byte-identical to the Hub's
// server/test/fixtures copy), worked out with Python's hmac module.
// Run: pio test -e native -f test_wsoft_hub_screen   (needs a host g++ on PATH)

#include <unity.h>

#include <stdio.h>
#include <string.h>

#include "wsoft_hub_screen.h"
#include "wsoft_sha256.h"

void setUp() {}
void tearDown() {}

// The model in the fixture (tools/wsoft-ui/ref.js PRESETS[0]).
static const char *MODEL =
    "{\"dark\":true,\"page\":0,\"style\":0,\"wifi_up\":true,\"wifi_bars\":3,\"ip\":\"192.168.0.142\",\"hu"
    "b\":\"HUB\",\"hub_tone\":1,\"link\":\"RS232\",\"link_tone\":1,\"slot\":0,\"slot_title\":\"\",\"slot_"
    "detail\":\"\",\"trade_ok\":false,\"reason\":\"\",\"reason_tone\":0,\"steady\":\"STABLE\",\"steady_to"
    "ne\":1,\"plate_sa\":\"\",\"plate_limits\":\"\",\"audit\":\"AUDIT 0\",\"label\":\"NET\",\"live\":true"
    ",\"weight\":\"50.120\",\"unit\":\"kg\",\"zone\":2,\"product_on\":true,\"word\":\"OK\",\"word_tone\":"
    "1,\"product\":\"MAIZE\",\"target\":\"TARGET 50 KG\",\"low\":\"49\",\"high\":\"51\",\"pointer\":527,"
    "\"gross\":\"50.120 kg\",\"save_line\":\"AUTO SAVE ON\",\"save_look\":1,\"hint\":\"HOLD STILL\",\"zero"
    "_btn\":false,\"zero_line\":\"\",\"title\":\"\",\"message\":\"\",\"tiles\":[{\"id\":\"approval\",\"ca"
    "ption\":\"APPROVAL\",\"value\":\"OFF\",\"look\":0},{\"id\":\"auto\",\"caption\":\"AUTO SAVE\",\"valu"
    "e\":\"ON\",\"look\":1},{\"id\":\"product\",\"caption\":\"PRODUCT\",\"value\":\"MAIZE\",\"look\":1},{"
    "\"id\":\"theme\",\"caption\":\"SCREEN\",\"value\":\"DARK\",\"look\":0},{\"id\":\"wifi\",\"caption\":"
    "\"WI-FI\",\"value\":\"192.168.0.142\",\"look\":0}],\"tile_count\":5,\"pin_count\":0,\"address\":\"19"
    "2.168.4.1\",\"ap_name\":\"Weighsoft-Esp-NEW\"}";

static const char *FIXTURE_BODY_START = "{\"sent_at\":\"2026-11-26T10:00:00Z\",\"screen\":";
static const char *FIXTURE_SIGNATURE = "sha256=77f2bf19333b0e4ba36e2891a94de6a81fe21638bd7f8c5bfdd5179309be370c";

static const char *hex(const uint8_t *data, size_t n) {
  static char out[200];
  for (size_t i = 0; i < n; i++) snprintf(out + i * 2, 3, "%02x", data[i]);
  out[n * 2] = 0;
  return out;
}

void test_body_matches_the_fixture_and_its_signature() {
  char body[WSOFT_SCREEN_BODY_MAX];
  TEST_ASSERT_TRUE(wsoft_screen_push_body("2026-11-26T10:00:00Z", MODEL, body, sizeof body));
  TEST_ASSERT_EQUAL_INT(0, strncmp(body, FIXTURE_BODY_START, strlen(FIXTURE_BODY_START)));
  TEST_ASSERT_EQUAL_UINT32(strlen(FIXTURE_BODY_START) + strlen(MODEL) + 1, strlen(body));
  TEST_ASSERT_EQUAL_CHAR('}', body[strlen(body) - 1]);

  // The request signature over that exact body, as hub.cpp sign() makes it.
  uint8_t key[32];
  const char *secret = "wsoft-screen-fixture-secret";
  wsoft_sha256((const uint8_t *)secret, strlen(secret), key);
  const char *head = "scale-models-hub-v1\nk-screen-fixture\n1795687200\nfedcba9876543210fedcba9876543210\n";
  uint8_t mac[32];
  wsoft_hmac_sha256(key, sizeof key, (const uint8_t *)head, strlen(head), (const uint8_t *)body, strlen(body), mac);
  char signature[80];
  snprintf(signature, sizeof signature, "sha256=%s", hex(mac, 32));
  TEST_ASSERT_EQUAL_STRING(FIXTURE_SIGNATURE, signature);
}

void test_body_refuses_what_does_not_fit() {
  char small[40];
  TEST_ASSERT_FALSE(wsoft_screen_push_body("2026-11-26T10:00:00Z", MODEL, small, sizeof small));
  TEST_ASSERT_EQUAL_STRING("", small);
  char body[WSOFT_SCREEN_BODY_MAX];
  TEST_ASSERT_FALSE(wsoft_screen_push_body("", MODEL, body, sizeof body));
  TEST_ASSERT_FALSE(wsoft_screen_push_body("2026-11-26T10:00:00Z", "", body, sizeof body));
  // The largest model the board makes (WSOFT_UI_JSON_MAX - 1 characters) still fits.
  static char big[2560];
  memset(big, 'x', sizeof big - 1);
  big[0] = '{';
  big[sizeof big - 2] = '}';
  big[sizeof big - 1] = 0;
  TEST_ASSERT_TRUE(wsoft_screen_push_body("2026-11-26T10:00:00Z", big, body, sizeof body));
}

void test_hash_tells_a_changed_screen() {
  TEST_ASSERT_EQUAL_UINT32(wsoft_screen_hash(MODEL), wsoft_screen_hash(MODEL));
  TEST_ASSERT_TRUE(wsoft_screen_hash("{\"page\":0}") != wsoft_screen_hash("{\"page\":1}"));
  TEST_ASSERT_EQUAL_UINT32(2166136261u, wsoft_screen_hash(""));
}

void test_first_push_goes_at_once() {
  WsoftScreenPush p;
  TEST_ASSERT_TRUE(wsoft_screen_push_due(p, 0, 1));
  TEST_ASSERT_TRUE(wsoft_screen_push_due(p, 12345, 1));
}

void test_changed_screen_waits_three_seconds_same_screen_thirty() {
  WsoftScreenPush p;
  wsoft_screen_push_result(&p, 10000, 7, 202);
  TEST_ASSERT_FALSE(wsoft_screen_push_due(p, 12999, 8));  // changed, too soon
  TEST_ASSERT_TRUE(wsoft_screen_push_due(p, 13000, 8));   // changed, 3 s
  TEST_ASSERT_FALSE(wsoft_screen_push_due(p, 39999, 7));  // same, under 30 s
  TEST_ASSERT_TRUE(wsoft_screen_push_due(p, 40000, 7));   // same: keep-alive
}

void test_failed_push_tries_the_changed_screen_again() {
  WsoftScreenPush p;
  wsoft_screen_push_result(&p, 10000, 7, 202);
  wsoft_screen_push_result(&p, 20000, 8, -1);  // no answer
  TEST_ASSERT_FALSE(wsoft_screen_push_due(p, 22999, 8));
  TEST_ASSERT_TRUE(wsoft_screen_push_due(p, 23000, 8));  // the Hub still holds 7
  wsoft_screen_push_result(&p, 23000, 8, 401);              // refused: waits a minute
  TEST_ASSERT_FALSE(wsoft_screen_push_due(p, 26000, 8));
  TEST_ASSERT_TRUE(wsoft_screen_push_due(p, 23000 + WSOFT_SCREEN_REFUSED_MS, 8));
  // A board that was never answered keeps trying every 3 s.
  WsoftScreenPush q;
  wsoft_screen_push_result(&q, 5000, 1, 500);
  TEST_ASSERT_FALSE(wsoft_screen_push_due(q, 7999, 1));
  TEST_ASSERT_TRUE(wsoft_screen_push_due(q, 8000, 1));
}

void test_older_hub_waits_ten_minutes_a_refusal_one() {
  WsoftScreenPush p;
  wsoft_screen_push_result(&p, 1000, 1, 404);
  TEST_ASSERT_FALSE(wsoft_screen_push_due(p, 1000 + WSOFT_SCREEN_OLD_HUB_MS - 1, 2));
  TEST_ASSERT_TRUE(wsoft_screen_push_due(p, 1000 + WSOFT_SCREEN_OLD_HUB_MS, 2));
  // Too many, too big, not allowed: one minute, not every 3 s.
  const int refused[] = {429, 400, 401, 403, 413};
  for (size_t i = 0; i < sizeof refused / sizeof refused[0]; i++) {
    WsoftScreenPush q;
    wsoft_screen_push_result(&q, 1000, 1, 202);
    wsoft_screen_push_result(&q, 5000, 2, refused[i]);
    TEST_ASSERT_FALSE(wsoft_screen_push_due(q, 5000 + WSOFT_SCREEN_REFUSED_MS - 1, 2));
    TEST_ASSERT_TRUE(wsoft_screen_push_due(q, 5000 + WSOFT_SCREEN_REFUSED_MS, 2));
  }
  // A Hub error (5xx) is not a refusal: the changed screen goes again after 3 s.
  WsoftScreenPush r;
  wsoft_screen_push_result(&r, 1000, 1, 202);
  wsoft_screen_push_result(&r, 5000, 2, 503);
  TEST_ASSERT_TRUE(wsoft_screen_push_due(r, 5000 + WSOFT_SCREEN_MIN_MS, 2));
}

void test_a_wait_never_comes_back_after_a_long_gap() {
  // The board did not ask for 25 days after a 10-minute wait (Hub offline): the wait is over.
  WsoftScreenPush p;
  wsoft_screen_push_result(&p, 1000, 1, 404);
  TEST_ASSERT_TRUE(wsoft_screen_push_due(p, 1000 + 2160000000u, 2));
}

void test_soon_lets_the_board_skip_building_the_json() {
  WsoftScreenPush p;
  TEST_ASSERT_TRUE(wsoft_screen_push_soon(p, 0));  // never tried
  wsoft_screen_push_result(&p, 10000, 7, 202);
  TEST_ASSERT_FALSE(wsoft_screen_push_soon(p, 12999));
  TEST_ASSERT_TRUE(wsoft_screen_push_soon(p, 13000));
  wsoft_screen_push_result(&p, 13000, 7, 404);
  TEST_ASSERT_FALSE(wsoft_screen_push_soon(p, 13000 + WSOFT_SCREEN_MIN_MS));  // still waiting
  TEST_ASSERT_TRUE(wsoft_screen_push_soon(p, 13000 + WSOFT_SCREEN_OLD_HUB_MS));
}

void test_a_try_at_millis_zero_still_counts() {
  WsoftScreenPush p;
  wsoft_screen_push_result(&p, 0, 1, -1);
  TEST_ASSERT_FALSE(wsoft_screen_push_due(p, 100, 1));
  TEST_ASSERT_TRUE(wsoft_screen_push_due(p, WSOFT_SCREEN_MIN_MS, 1));
}

void test_millis_wrap() {
  WsoftScreenPush p;
  wsoft_screen_push_result(&p, 0xFFFFF000u, 7, 202);
  TEST_ASSERT_TRUE(wsoft_screen_push_due(p, 0xFFFFF000u + WSOFT_SCREEN_MIN_MS, 8));  // wraps past 0
  WsoftScreenPush q;
  wsoft_screen_push_result(&q, 0xFFFFF000u, 7, 404);
  TEST_ASSERT_FALSE(wsoft_screen_push_due(q, 1000, 8));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_body_matches_the_fixture_and_its_signature);
  RUN_TEST(test_body_refuses_what_does_not_fit);
  RUN_TEST(test_hash_tells_a_changed_screen);
  RUN_TEST(test_first_push_goes_at_once);
  RUN_TEST(test_changed_screen_waits_three_seconds_same_screen_thirty);
  RUN_TEST(test_failed_push_tries_the_changed_screen_again);
  RUN_TEST(test_older_hub_waits_ten_minutes_a_refusal_one);
  RUN_TEST(test_a_wait_never_comes_back_after_a_long_gap);
  RUN_TEST(test_soon_lets_the_board_skip_building_the_json);
  RUN_TEST(test_a_try_at_millis_zero_still_counts);
  RUN_TEST(test_millis_wrap);
  return UNITY_END();
}
