// Host tests for the board side of the Hub Hi-Lo setup (Hub ADR 0008, contract C1 to C3,
// lib/wsoft_core/src/wsoft_hilo.cpp). The fixture is byte-identical to the Hub's
// server/test/fixtures/hub-hilo-v1-fixtures.json (worked out with openssl).
// Run: pio test -e native   (needs a host g++ on PATH)

#include <unity.h>

#include <stdio.h>
#include <string.h>

#include "wsoft_hilo.h"
#include "wsoft_hub_commands.h"
#include "wsoft_sha256.h"

void setUp() {}
void tearDown() {}

static const char *MAIZE_ANSWER =
    "{\"products\":[{\"code\":\"MAIZE\",\"name\":\"Maize\",\"target_g\":50000,\"low_g\":49000,\"high_g\":51000}],"
    "\"default\":{\"code\":\"MAIZE\",\"set_at\":\"2026-11-08T10:00:00.000Z\"}}";

static bool parse(const char *body, WsoftHiloList *list) { return wsoft_hilo_parse(body, strlen(body), list); }

void test_code_rule() {
  TEST_ASSERT_TRUE(wsoft_hilo_code_ok("MAIZE"));
  TEST_ASSERT_TRUE(wsoft_hilo_code_ok("a-b_9"));
  TEST_ASSERT_TRUE(wsoft_hilo_code_ok("12345678901234567890123456789012"));   // 32
  TEST_ASSERT_FALSE(wsoft_hilo_code_ok("123456789012345678901234567890123"));  // 33
  TEST_ASSERT_FALSE(wsoft_hilo_code_ok(""));
  TEST_ASSERT_FALSE(wsoft_hilo_code_ok(nullptr));
  TEST_ASSERT_FALSE(wsoft_hilo_code_ok("MA IZE"));
  TEST_ASSERT_FALSE(wsoft_hilo_code_ok("MA\"IZE"));
}

void test_request_body() {
  char out[WSOFT_HILO_REQUEST_MAX];
  TEST_ASSERT_TRUE(wsoft_hilo_request(nullptr, out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("{\"current\":null}", out);
  TEST_ASSERT_TRUE(wsoft_hilo_request("", out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("{\"current\":null}", out);
  TEST_ASSERT_TRUE(wsoft_hilo_request("MAIZE", out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("{\"current\":\"MAIZE\"}", out);
  TEST_ASSERT_TRUE(wsoft_hilo_request("12345678901234567890123456789012", out, sizeof out));
  TEST_ASSERT_FALSE(wsoft_hilo_request("bad\"code", out, sizeof out));
  TEST_ASSERT_FALSE(wsoft_hilo_request("MAIZE", out, 10));  // too small
}

void test_parse_fixture_answer() {
  WsoftHiloList list;
  TEST_ASSERT_TRUE(parse(MAIZE_ANSWER, &list));
  TEST_ASSERT_EQUAL(1, (int)list.count);
  TEST_ASSERT_EQUAL_STRING("MAIZE", list.products[0].code);
  TEST_ASSERT_EQUAL_STRING("Maize", list.products[0].name);
  TEST_ASSERT_EQUAL(50000, list.products[0].target_g);
  TEST_ASSERT_EQUAL(49000, list.products[0].low_g);
  TEST_ASSERT_EQUAL(51000, list.products[0].high_g);
  TEST_ASSERT_TRUE(list.has_default);
  TEST_ASSERT_EQUAL_STRING("MAIZE", list.default_code);
  TEST_ASSERT_EQUAL_STRING("2026-11-08T10:00:00.000Z", list.default_set_at);
}

void test_parse_empty_list_null_default_spaces_and_key_order() {
  WsoftHiloList list;
  TEST_ASSERT_TRUE(parse(" { \"default\" : null , \"products\" : [ ] } ", &list));
  TEST_ASSERT_EQUAL(0, (int)list.count);
  TEST_ASSERT_FALSE(list.has_default);
  TEST_ASSERT_TRUE(parse("{\"products\":[{\"high_g\":3,\"low_g\":1,\"name\":\"\",\"target_g\":2,\"code\":\"X\"}],"
                         "\"default\":{\"set_at\":\"t\",\"code\":\"X\"}}",
                         &list));
  TEST_ASSERT_EQUAL(1, (int)list.count);
  TEST_ASSERT_EQUAL(2, list.products[0].target_g);
}

void test_long_name_is_cut() {
  WsoftHiloList list;
  TEST_ASSERT_TRUE(parse("{\"products\":[{\"code\":\"X\",\"name\":\"Yellow maize grade one sixty kilo bag\","
                         "\"target_g\":2,\"low_g\":1,\"high_g\":3}],\"default\":null}",
                         &list));
  TEST_ASSERT_EQUAL(WSOFT_HILO_NAME_MAX, (int)strlen(list.products[0].name));
  TEST_ASSERT_EQUAL('~', list.products[0].name[WSOFT_HILO_NAME_MAX - 1]);
}

void test_parse_is_strict() {
  WsoftHiloList list;
  const char *bad[] = {
      "",
      "{}",
      "{\"products\":[]}",                                    // default missing
      "{\"default\":null}",                                   // products missing
      "{\"products\":[],\"default\":null,\"extra\":1}",       // unknown key
      "{\"products\":[],\"default\":null,\"default\":null}",  // twice
      "{\"products\":[],\"default\":null} x",                 // trailing text
      "{\"products\":[{\"code\":\"X\",\"name\":\"\",\"target_g\":2,\"low_g\":1}],\"default\":null}",  // high missing
      "{\"products\":[{\"code\":\"X\",\"name\":\"\",\"target_g\":2,\"low_g\":3,\"high_g\":4}],\"default\":null}",  // low>target
      "{\"products\":[{\"code\":\"X\",\"name\":\"\",\"target_g\":5,\"low_g\":3,\"high_g\":4}],\"default\":null}",  // target>high
      "{\"products\":[{\"code\":\"X\",\"name\":\"\",\"target_g\":2.5,\"low_g\":1,\"high_g\":3}],\"default\":null}",  // fraction
      "{\"products\":[{\"code\":\"X\",\"name\":\"\",\"target_g\":-2,\"low_g\":1,\"high_g\":3}],\"default\":null}",  // negative
      "{\"products\":[{\"code\":\"X\",\"name\":\"\",\"target_g\":2,\"low_g\":1,\"high_g\":1000000001}],\"default\":null}",
      "{\"products\":[{\"code\":\"X Y\",\"name\":\"\",\"target_g\":2,\"low_g\":1,\"high_g\":3}],\"default\":null}",
      "{\"products\":[{\"code\":\"X\",\"name\":\"\",\"target_g\":2,\"low_g\":1,\"high_g\":3,\"spare\":1}],\"default\":null}",
      "{\"products\":[],\"default\":{\"code\":\"X\"}}",       // set_at missing
      "{\"products\":[],\"default\":{\"code\":\"X\",\"set_at\":\"\"}}",
      "{\"products\":[],\"default\":{\"code\":\"X\",\"set_at\":\"12345678901234567890123456789012\"}}",  // 32 > 31
      "{\"products\":[],\"default\":true}",
  };
  for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
    list.count = 7;
    if (parse(bad[i], &list)) {
      printf("  accepted: %s\n", bad[i]);
      TEST_ASSERT_TRUE(false);
    }
    TEST_ASSERT_EQUAL(0, (int)list.count);  // nothing kept from a bad answer
  }
}

void test_at_most_50_products() {
  char body[8000];
  size_t n = (size_t)snprintf(body, sizeof body, "{\"products\":[");
  for (int i = 0; i < 51; i++) {
    n += (size_t)snprintf(body + n, sizeof body - n, "%s{\"code\":\"P%02d\",\"name\":\"\",\"target_g\":2,\"low_g\":1,\"high_g\":3}",
                          i ? "," : "", i);
    if (i == 49) {
      char fifty[8000];
      snprintf(fifty, sizeof fifty, "%s],\"default\":null}", body);
      WsoftHiloList list;
      TEST_ASSERT_TRUE(parse(fifty, &list));
      TEST_ASSERT_EQUAL(50, (int)list.count);
    }
  }
  snprintf(body + n, sizeof body - n, "],\"default\":null}");
  WsoftHiloList list;
  TEST_ASSERT_FALSE(parse(body, &list));
}

static WsoftHiloList two_products() {
  WsoftHiloList list;
  parse("{\"products\":[{\"code\":\"BEANS\",\"name\":\"Beans\",\"target_g\":10000,\"low_g\":9900,\"high_g\":10100},"
        "{\"code\":\"MAIZE\",\"name\":\"Maize\",\"target_g\":50000,\"low_g\":49000,\"high_g\":51000}],"
        "\"default\":{\"code\":\"MAIZE\",\"set_at\":\"2026-11-08T10:00:00.000Z\"}}",
        &list);
  return list;
}

void test_default_is_applied_once_then_operator_wins() {
  WsoftHiloList list = two_products();
  WsoftHiloPick pick;
  TEST_ASSERT_TRUE(wsoft_hilo_apply(list, &pick));
  TEST_ASSERT_EQUAL_STRING("MAIZE", pick.current);
  // The operator picks beans on the screen; the same default must not undo that.
  strcpy(pick.current, "BEANS");
  TEST_ASSERT_FALSE(wsoft_hilo_apply(list, &pick));
  TEST_ASSERT_EQUAL_STRING("BEANS", pick.current);
  // A new default from the Hub (new set_at) wins again: last change wins.
  strcpy(list.default_set_at, "2026-11-08T11:00:00.000Z");
  TEST_ASSERT_TRUE(wsoft_hilo_apply(list, &pick));
  TEST_ASSERT_EQUAL_STRING("MAIZE", pick.current);
}

void test_product_gone_from_the_list_is_cleared() {
  WsoftHiloList list = two_products();
  WsoftHiloPick pick;
  wsoft_hilo_apply(list, &pick);
  strcpy(pick.current, "ONIONS");
  TEST_ASSERT_TRUE(wsoft_hilo_apply(list, &pick));
  TEST_ASSERT_EQUAL_STRING("", pick.current);
  // A default for a product the list does not hold is not applied.
  WsoftHiloPick fresh;
  strcpy(list.default_code, "ONIONS");
  strcpy(list.default_set_at, "later");
  TEST_ASSERT_FALSE(wsoft_hilo_apply(list, &fresh));
  TEST_ASSERT_EQUAL_STRING("", fresh.current);
}

void test_next_cycles_none_first_last_none() {
  WsoftHiloList list = two_products();
  TEST_ASSERT_EQUAL_STRING("BEANS", wsoft_hilo_next(list, ""));
  TEST_ASSERT_EQUAL_STRING("MAIZE", wsoft_hilo_next(list, "BEANS"));
  TEST_ASSERT_EQUAL_STRING("", wsoft_hilo_next(list, "MAIZE"));
  TEST_ASSERT_EQUAL_STRING("BEANS", wsoft_hilo_next(list, "GONE"));
  WsoftHiloList empty;
  TEST_ASSERT_EQUAL_STRING("", wsoft_hilo_next(empty, ""));
  TEST_ASSERT_TRUE(wsoft_hilo_find(list, "MAIZE") == &list.products[1]);
  TEST_ASSERT_TRUE(wsoft_hilo_find(list, "") == nullptr);
}

void test_same_compares_fields_not_bytes() {
  WsoftHiloList a = two_products();
  WsoftHiloList b = two_products();
  TEST_ASSERT_TRUE(wsoft_hilo_same(a, b));
  b.products[0].name[10] = 'x';  // after the end of "Beans": not part of the name
  TEST_ASSERT_TRUE(wsoft_hilo_same(a, b));
  b.products[1].high_g++;
  TEST_ASSERT_FALSE(wsoft_hilo_same(a, b));
  b = two_products();
  strcpy(b.default_set_at, "later");
  TEST_ASSERT_FALSE(wsoft_hilo_same(a, b));
  b = two_products();
  b.count = 1;
  TEST_ASSERT_FALSE(wsoft_hilo_same(a, b));
}

void test_check_is_exact_at_the_limits() {
  WsoftHiloList list = two_products();
  const WsoftHiloProduct &maize = list.products[1];  // 49 to 51 kg
  TEST_ASSERT_EQUAL(WSOFT_HILO_LOW, wsoft_hilo_check(maize, 48999, 3, 1));
  TEST_ASSERT_EQUAL(WSOFT_HILO_PASS, wsoft_hilo_check(maize, 49000, 3, 1));
  TEST_ASSERT_EQUAL(WSOFT_HILO_PASS, wsoft_hilo_check(maize, 51000, 3, 1));
  TEST_ASSERT_EQUAL(WSOFT_HILO_HIGH, wsoft_hilo_check(maize, 51001, 3, 1));
  TEST_ASSERT_EQUAL(WSOFT_HILO_PASS, wsoft_hilo_check(maize, 50, 0, 1));    // 50 kg
  TEST_ASSERT_EQUAL(WSOFT_HILO_HIGH, wsoft_hilo_check(maize, 512, 1, 1));   // 51.2 kg
  TEST_ASSERT_EQUAL(WSOFT_HILO_LOW, wsoft_hilo_check(maize, 4899, 2, 1));   // 48.99 kg
  TEST_ASSERT_EQUAL(WSOFT_HILO_PASS, wsoft_hilo_check(maize, 50000, 0, 3)); // grams
  TEST_ASSERT_EQUAL(WSOFT_HILO_HIGH, wsoft_hilo_check(maize, 1, 0, 2));     // 1 t
  TEST_ASSERT_EQUAL(WSOFT_HILO_LOW, wsoft_hilo_check(maize, -50000, 3, 1));
  TEST_ASSERT_EQUAL(WSOFT_HILO_NONE, wsoft_hilo_check(maize, 50, 0, 9));    // unknown unit
  TEST_ASSERT_EQUAL(WSOFT_HILO_NONE, wsoft_hilo_check(maize, 50, 10, 1));
  TEST_ASSERT_EQUAL(WSOFT_HILO_NONE, wsoft_hilo_check(maize, 50, -1, 1));
}

void test_words() {
  TEST_ASSERT_EQUAL_STRING("LOW", wsoft_hilo_state_wire(WSOFT_HILO_LOW));
  TEST_ASSERT_EQUAL_STRING("PASS", wsoft_hilo_state_wire(WSOFT_HILO_PASS));
  TEST_ASSERT_EQUAL_STRING("HIGH", wsoft_hilo_state_wire(WSOFT_HILO_HIGH));
  TEST_ASSERT_EQUAL_STRING("", wsoft_hilo_state_wire(WSOFT_HILO_NONE));
  TEST_ASSERT_EQUAL_STRING("TOO LOW", wsoft_hilo_state_words(WSOFT_HILO_LOW));
  TEST_ASSERT_EQUAL_STRING("OK", wsoft_hilo_state_words(WSOFT_HILO_PASS));
  TEST_ASSERT_EQUAL_STRING("TOO HIGH", wsoft_hilo_state_words(WSOFT_HILO_HIGH));
}

void test_weighing_hi_lo_block() {
  WsoftHiloStamp s;
  s.on = true;
  strcpy(s.code, "MAIZE");
  s.target_g = 50000;
  s.low_g = 49900;
  s.high_g = 51000;
  s.state = WSOFT_HILO_HIGH;
  s.auto_saved = true;
  char out[400];
  TEST_ASSERT_TRUE(wsoft_hilo_json(s, "51.2", out, sizeof out));
  TEST_ASSERT_EQUAL_STRING(
      ",\"hi_lo\":{\"state\":\"HIGH\",\"reading_kg\":51.2,\"target_kg\":50,\"low_limit_kg\":49.9,"
      "\"high_limit_kg\":51,\"deadband_kg\":0,\"tolerance\":{\"source\":\"hub-hilo-v1\"},"
      "\"auto_saved\":true,\"product_code\":\"MAIZE\"}",
      out);
  s.overridden = true;  // outside the limits, saved by the operator's second tap
  TEST_ASSERT_TRUE(wsoft_hilo_json(s, "51.2", out, sizeof out));
  TEST_ASSERT_TRUE(strstr(out, "\"tolerance\":{\"source\":\"hub-hilo-v1\",\"operator_override\":true}") != nullptr);
  s.overridden = false;
  TEST_ASSERT_FALSE(wsoft_hilo_json(s, "51.2", out, 40));  // too small
  TEST_ASSERT_FALSE(wsoft_hilo_json(s, "", out, sizeof out));
  s.state = WSOFT_HILO_NONE;
  TEST_ASSERT_FALSE(wsoft_hilo_json(s, "51.2", out, sizeof out));
  s.state = WSOFT_HILO_PASS;
  s.on = false;
  TEST_ASSERT_FALSE(wsoft_hilo_json(s, "51.2", out, sizeof out));
}

void test_hub_fixture_both_ways() {
  const char *secret = "wsoft-hilo-fixture-secret";
  uint8_t key[32];
  wsoft_sha256((const uint8_t *)secret, strlen(secret), key);
  const char *nonce = "0123456789abcdef0123456789abcdef";

  char body[WSOFT_HILO_REQUEST_MAX];
  TEST_ASSERT_TRUE(wsoft_hilo_request(nullptr, body, sizeof body));
  char sig[WSOFT_CMD_SIGNATURE_SIZE];
  wsoft_hub_signature(key, "scale-models-hub-v1", "k-hilo-fixture", "1794132000", nonce, body, strlen(body), sig);
  TEST_ASSERT_EQUAL_STRING("sha256=18724ec588e7220032d8d4c5c7198825c0c3c95a8b79936b0c0e93344c83121b", sig);

  const char *good = "sha256=e4d16d7ddce7ad8b1e8de90d61e86d84b08916b9f6a967fad7a53f9d3687242d";
  TEST_ASSERT_TRUE(wsoft_hub_verify_answer(key, WSOFT_HUB_HILO_PREFIX, "k-hilo-fixture", nonce, "k-hilo-fixture",
                                           "1794132005", nonce, good, MAIZE_ANSWER, strlen(MAIZE_ANSWER)));
  // The command prefix must not verify a Hi-Lo answer (each answer type has its own prefix).
  TEST_ASSERT_FALSE(wsoft_hub_commands_verify(key, "k-hilo-fixture", nonce, "k-hilo-fixture", "1794132005", nonce,
                                              good, MAIZE_ANSWER, strlen(MAIZE_ANSWER)));
  TEST_ASSERT_FALSE(wsoft_hub_verify_answer(key, WSOFT_HUB_HILO_PREFIX, "k-hilo-fixture", nonce, "k-hilo-fixture",
                                            "1794132005", "ffffffffffffffffffffffffffffffff", good, MAIZE_ANSWER,
                                            strlen(MAIZE_ANSWER)));
  WsoftHiloList list;
  TEST_ASSERT_TRUE(parse(MAIZE_ANSWER, &list));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_code_rule);
  RUN_TEST(test_request_body);
  RUN_TEST(test_parse_fixture_answer);
  RUN_TEST(test_parse_empty_list_null_default_spaces_and_key_order);
  RUN_TEST(test_long_name_is_cut);
  RUN_TEST(test_parse_is_strict);
  RUN_TEST(test_at_most_50_products);
  RUN_TEST(test_default_is_applied_once_then_operator_wins);
  RUN_TEST(test_product_gone_from_the_list_is_cleared);
  RUN_TEST(test_next_cycles_none_first_last_none);
  RUN_TEST(test_same_compares_fields_not_bytes);
  RUN_TEST(test_check_is_exact_at_the_limits);
  RUN_TEST(test_words);
  RUN_TEST(test_weighing_hi_lo_block);
  RUN_TEST(test_hub_fixture_both_ways);
  return UNITY_END();
}
