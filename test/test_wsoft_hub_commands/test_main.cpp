// Host tests for the Hub command pull (contract B1/B2,
// lib/wsoft_core/src/wsoft_hub_commands.cpp) and HMAC-SHA256 (RFC 4231 vectors).
// The signature vector was worked out with Python's hmac module.
// Run: pio test -e native   (needs a host g++ on PATH)

#include <unity.h>

#include <stdio.h>
#include <string.h>

#include "wsoft_hub_commands.h"
#include "wsoft_sha256.h"

void setUp() {}
void tearDown() {}

static const char *hex(const uint8_t *data, size_t n) {
  static char out[200];
  for (size_t i = 0; i < n; i++) snprintf(out + i * 2, 3, "%02x", data[i]);
  out[n * 2] = 0;
  return out;
}

void test_hmac_rfc4231() {
  uint8_t mac[32];
  uint8_t key1[20];
  memset(key1, 0x0b, sizeof key1);
  wsoft_hmac_sha256(key1, sizeof key1, (const uint8_t *)"Hi ", 3, (const uint8_t *)"There", 5, mac);
  TEST_ASSERT_EQUAL_STRING("b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", hex(mac, 32));
  const char *data2 = "what do ya want for nothing?";
  wsoft_hmac_sha256((const uint8_t *)"Jefe", 4, (const uint8_t *)data2, strlen(data2), nullptr, 0, mac);
  TEST_ASSERT_EQUAL_STRING("5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843", hex(mac, 32));
  uint8_t key6[131];
  memset(key6, 0xaa, sizeof key6);
  const char *data6 = "Test Using Larger Than Block-Size Key - Hash Key First";
  wsoft_hmac_sha256(key6, sizeof key6, (const uint8_t *)data6, strlen(data6), nullptr, 0, mac);
  TEST_ASSERT_EQUAL_STRING("60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54", hex(mac, 32));
}

void test_request_body() {
  char out[WSOFT_CMD_REQUEST_MAX];
  TEST_ASSERT_TRUE(wsoft_hub_commands_request(0, nullptr, 0, out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("{\"applied_through\":0,\"results\":[]}", out);
  WsoftCmdResult results[2];
  results[0].id = 12;
  results[0].ok = false;
  strcpy(results[0].error, "SETUP_INCOMPLETE");
  results[1].id = 13;
  results[1].ok = true;
  results[1].error[0] = 0;
  TEST_ASSERT_TRUE(wsoft_hub_commands_request(13, results, 2, out, sizeof out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"applied_through\":13,\"results\":[{\"id\":12,\"ok\":false,\"error\":\"SETUP_INCOMPLETE\"},"
      "{\"id\":13,\"ok\":true,\"error\":null}]}",
      out);
  TEST_ASSERT_FALSE(wsoft_hub_commands_request(13, results, 2, out, 40));  // too small
  // The largest body (20 results with 32-character codes) fits the buffer size.
  WsoftCmdResult many[WSOFT_CMD_RESULTS_MAX];
  for (size_t i = 0; i < WSOFT_CMD_RESULTS_MAX; i++) {
    many[i].id = 9007199254740991ULL;
    many[i].ok = false;
    memset(many[i].error, 'E', 32);
    many[i].error[32] = 0;
  }
  TEST_ASSERT_TRUE(wsoft_hub_commands_request(9007199254740991ULL, many, WSOFT_CMD_RESULTS_MAX, out, sizeof out));
  // A bad error code is sent as a safe one, never as raw text.
  WsoftCmdResult odd;
  odd.id = 1;
  odd.ok = false;
  strcpy(odd.error, "bad \"code\"");
  TEST_ASSERT_TRUE(wsoft_hub_commands_request(1, &odd, 1, out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("{\"applied_through\":1,\"results\":[{\"id\":1,\"ok\":false,\"error\":\"REFUSED\"}]}", out);
}

static const char *BODY =
    "{\"commands\":[{\"id\":13,\"switch\":\"approval\",\"on\":true,\"by\":\"admin\",\"reason\":\"bench test\"}]}";

void test_parse_one_command() {
  WsoftHubCommand cmds[WSOFT_CMD_PER_PULL];
  size_t count = 99;
  TEST_ASSERT_TRUE(wsoft_hub_commands_parse(BODY, strlen(BODY), cmds, &count));
  TEST_ASSERT_EQUAL(1, (int)count);
  TEST_ASSERT_EQUAL(13, (int)cmds[0].id);
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_APPROVAL, cmds[0].which);
  TEST_ASSERT_TRUE(cmds[0].on);
  TEST_ASSERT_EQUAL_STRING("admin", cmds[0].by);
  TEST_ASSERT_EQUAL_STRING("bench test", cmds[0].reason);
}

void test_parse_empty_list_spaces_escapes_and_key_order() {
  WsoftHubCommand cmds[WSOFT_CMD_PER_PULL];
  size_t count = 99;
  const char *empty = " { \"commands\" : [ ] } ";
  TEST_ASSERT_TRUE(wsoft_hub_commands_parse(empty, strlen(empty), cmds, &count));
  TEST_ASSERT_EQUAL(0, (int)count);
  const char *two =
      "{\"commands\":[{\"reason\":\"say \\\"hi\\\" \\u0041\",\"by\":\"a\",\"on\":false,\"switch\":\"auto_save\",\"id\":14},"
      "{\"id\":15,\"switch\":\"approval\",\"on\":true,\"by\":\"b\",\"reason\":\"\"}]}";
  TEST_ASSERT_TRUE(wsoft_hub_commands_parse(two, strlen(two), cmds, &count));
  TEST_ASSERT_EQUAL(2, (int)count);
  TEST_ASSERT_EQUAL_STRING("say \"hi\" A", cmds[0].reason);
  TEST_ASSERT_EQUAL(WSOFT_SWITCH_AUTO_SAVE, cmds[0].which);
  TEST_ASSERT_FALSE(cmds[0].on);
  TEST_ASSERT_EQUAL(15, (int)cmds[1].id);
}

static bool parses(const char *body) {
  WsoftHubCommand cmds[WSOFT_CMD_PER_PULL];
  size_t count = 0;
  return wsoft_hub_commands_parse(body, strlen(body), cmds, &count);
}

void test_parse_is_strict() {
  TEST_ASSERT_FALSE(parses(""));
  TEST_ASSERT_FALSE(parses("{}"));
  TEST_ASSERT_FALSE(parses("{\"commands\":[]} x"));
  TEST_ASSERT_FALSE(parses("{\"commands\":[],\"extra\":1}"));
  TEST_ASSERT_FALSE(parses("{\"commands\":[{\"id\":1,\"switch\":\"approval\",\"on\":true,\"by\":\"a\"}]}"));  // no reason
  TEST_ASSERT_FALSE(parses(
      "{\"commands\":[{\"id\":1,\"switch\":\"approval\",\"on\":true,\"by\":\"a\",\"reason\":\"\",\"x\":1}]}"));
  TEST_ASSERT_FALSE(parses(
      "{\"commands\":[{\"id\":1,\"id\":2,\"switch\":\"approval\",\"on\":true,\"by\":\"a\",\"reason\":\"\"}]}"));
  TEST_ASSERT_FALSE(parses("{\"commands\":[{\"id\":1,\"switch\":\"door\",\"on\":true,\"by\":\"a\",\"reason\":\"\"}]}"));
  TEST_ASSERT_FALSE(parses("{\"commands\":[{\"id\":\"1\",\"switch\":\"approval\",\"on\":true,\"by\":\"a\",\"reason\":\"\"}]}"));
  TEST_ASSERT_FALSE(parses("{\"commands\":[{\"id\":0,\"switch\":\"approval\",\"on\":true,\"by\":\"a\",\"reason\":\"\"}]}"));
  TEST_ASSERT_FALSE(parses("{\"commands\":[{\"id\":1.5,\"switch\":\"approval\",\"on\":true,\"by\":\"a\",\"reason\":\"\"}]}"));
  TEST_ASSERT_FALSE(parses(
      "{\"commands\":[{\"id\":9007199254740992,\"switch\":\"approval\",\"on\":true,\"by\":\"a\",\"reason\":\"\"}]}"));
  TEST_ASSERT_FALSE(parses("{\"commands\":[{\"id\":1,\"switch\":\"approval\",\"on\":1,\"by\":\"a\",\"reason\":\"\"}]}"));
  TEST_ASSERT_FALSE(parses("{\"commands\":[{\"id\":1,\"switch\":\"approval\",\"on\":true,\"by\":\"a\",\"reason\":\"x}]}"));
  // More than 10 commands.
  char big[2048] = "{\"commands\":[";
  for (int i = 1; i <= 11; i++) {
    char one[128];
    snprintf(one, sizeof one, "%s{\"id\":%d,\"switch\":\"auto_save\",\"on\":true,\"by\":\"a\",\"reason\":\"\"}",
             i > 1 ? "," : "", i);
    strcat(big, one);
  }
  strcat(big, "]}");
  TEST_ASSERT_FALSE(parses(big));
}

void test_long_text_is_cut_so_the_switch_rules_still_see_it() {
  // A reason over 80 is kept one character too long, so the switch check answers REASON_TOO_LONG.
  char body[600];
  char reason[201];
  memset(reason, 'r', 200);
  reason[200] = 0;
  snprintf(body, sizeof body, "{\"commands\":[{\"id\":1,\"switch\":\"approval\",\"on\":true,\"by\":\"a\",\"reason\":\"%s\"}]}",
           reason);
  WsoftHubCommand cmds[WSOFT_CMD_PER_PULL];
  size_t count = 0;
  TEST_ASSERT_TRUE(wsoft_hub_commands_parse(body, strlen(body), cmds, &count));
  TEST_ASSERT_EQUAL(81, (int)strlen(cmds[0].reason));
  // A non-ASCII character stays visible to the switch check (it refuses it).
  const char *utf = "{\"commands\":[{\"id\":1,\"switch\":\"approval\",\"on\":true,\"by\":\"a\",\"reason\":\"caf\\u00e9\"}]}";
  TEST_ASSERT_TRUE(wsoft_hub_commands_parse(utf, strlen(utf), cmds, &count));
  TEST_ASSERT_TRUE(strlen(cmds[0].reason) == 4 && (unsigned char)cmds[0].reason[3] > 0x7E);
}

static uint8_t KEY[32];

static void make_key() {
  uint8_t secret[32];
  for (int i = 0; i < 32; i++) secret[i] = (uint8_t)i;
  wsoft_sha256(secret, 32, KEY);  // the HMAC key is sha256(pairing secret)
}

static const char *NONCE = "00112233445566778899aabbccddeeff";
static const char *SIG = "sha256=415223bedc9846bc6d1ad6672804b16e272d341685425c2021a62451b3c149e2";

void test_signature_matches_python() {
  make_key();
  char sig[WSOFT_CMD_SIGNATURE_SIZE];
  wsoft_hub_signature(KEY, WSOFT_HUB_COMMAND_PREFIX, "key_abc", "1762549200", NONCE, BODY, strlen(BODY), sig);
  TEST_ASSERT_EQUAL_STRING(SIG, sig);
  TEST_ASSERT_EQUAL_STRING("scale-models-hub-command-v1", WSOFT_HUB_COMMAND_PREFIX);
}

void test_verify_checks_key_nonce_and_signature() {
  make_key();
  size_t n = strlen(BODY);
  TEST_ASSERT_TRUE(wsoft_hub_commands_verify(KEY, "key_abc", NONCE, "key_abc", "1762549200", NONCE, SIG, BODY, n));
  // Another key id, another nonce (a replayed answer), another time, a changed body, a bad signature.
  TEST_ASSERT_FALSE(wsoft_hub_commands_verify(KEY, "key_abd", NONCE, "key_abc", "1762549200", NONCE, SIG, BODY, n));
  TEST_ASSERT_FALSE(wsoft_hub_commands_verify(KEY, "key_abc", "ffffffffffffffffffffffffffffffff", "key_abc",
                                              "1762549200", NONCE, SIG, BODY, n));
  TEST_ASSERT_FALSE(wsoft_hub_commands_verify(KEY, "key_abc", NONCE, "key_abc", "1762549201", NONCE, SIG, BODY, n));
  TEST_ASSERT_FALSE(wsoft_hub_commands_verify(KEY, "key_abc", NONCE, "key_abc", "1762549200", NONCE, SIG, BODY, n - 1));
  TEST_ASSERT_FALSE(wsoft_hub_commands_verify(KEY, "key_abc", NONCE, "key_abc", "1762549200", NONCE,
                                              "sha256=00", BODY, n));
  TEST_ASSERT_FALSE(wsoft_hub_commands_verify(KEY, "key_abc", NONCE, "key_abc", "", NONCE, SIG, BODY, n));
  TEST_ASSERT_FALSE(wsoft_hub_commands_verify(KEY, "key_abc", NONCE, "key_abc", "1762549200", NONCE, nullptr, BODY, n));
  // Signed under the request prefix: refused, so a request can never pass as an answer.
  char other[WSOFT_CMD_SIGNATURE_SIZE];
  wsoft_hub_signature(KEY, "scale-models-hub-v1", "key_abc", "1762549200", NONCE, BODY, n, other);
  TEST_ASSERT_FALSE(wsoft_hub_commands_verify(KEY, "key_abc", NONCE, "key_abc", "1762549200", NONCE, other, BODY, n));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_hmac_rfc4231);
  RUN_TEST(test_request_body);
  RUN_TEST(test_parse_one_command);
  RUN_TEST(test_parse_empty_list_spaces_escapes_and_key_order);
  RUN_TEST(test_parse_is_strict);
  RUN_TEST(test_long_text_is_cut_so_the_switch_rules_still_see_it);
  RUN_TEST(test_signature_matches_python);
  RUN_TEST(test_verify_checks_key_nonce_and_signature);
  return UNITY_END();
}
