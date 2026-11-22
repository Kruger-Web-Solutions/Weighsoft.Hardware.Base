// Host tests for the screen PIN (lib/wsoft_core/src/wsoft_pin.cpp) and the small SHA-256
// it uses (lib/wsoft_core/src/wsoft_sha256.cpp, NIST FIPS 180-2 vectors).
// Run: pio test -e native   (needs a host g++ on PATH)

#include <unity.h>

#include <stdio.h>
#include <string.h>

#include "wsoft_pin.h"
#include "wsoft_sha256.h"

void setUp() {}
void tearDown() {}

static const char *hex(const uint8_t *data, size_t n) {
  static char out[200];
  for (size_t i = 0; i < n; i++) snprintf(out + i * 2, 3, "%02x", data[i]);
  out[n * 2] = 0;
  return out;
}

static const char *sha(const char *text) {
  uint8_t digest[32];
  wsoft_sha256((const uint8_t *)text, strlen(text), digest);
  return hex(digest, 32);
}

void test_sha256_nist_vectors() {
  TEST_ASSERT_EQUAL_STRING("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", sha(""));
  TEST_ASSERT_EQUAL_STRING("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", sha("abc"));
  TEST_ASSERT_EQUAL_STRING("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
                           sha("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"));
}

void test_sha256_in_pieces_and_a_million_a() {
  WsoftSha256 ctx;
  wsoft_sha256_begin(&ctx);
  static char block[1000];
  memset(block, 'a', sizeof block);
  for (int i = 0; i < 1000; i++) wsoft_sha256_add(&ctx, (const uint8_t *)block, sizeof block);
  uint8_t digest[32];
  wsoft_sha256_end(&ctx, digest);
  TEST_ASSERT_EQUAL_STRING("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", hex(digest, 32));
  // 55, 56 and 64 bytes sit on the padding edges.
  memset(block, 'a', 64);
  block[64] = 0;
  block[55] = 0;
  TEST_ASSERT_EQUAL_STRING("9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318", sha(block));
  block[55] = 'a';
  block[56] = 0;
  TEST_ASSERT_EQUAL_STRING("b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a", sha(block));
  block[56] = 'a';
  TEST_ASSERT_EQUAL_STRING("ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb", sha(block));
}

void test_pin_format_is_4_to_6_digits() {
  TEST_ASSERT_TRUE(wsoft_pin_format_ok("1234"));
  TEST_ASSERT_TRUE(wsoft_pin_format_ok("123456"));
  TEST_ASSERT_TRUE(wsoft_pin_format_ok("0000"));
  TEST_ASSERT_FALSE(wsoft_pin_format_ok("123"));
  TEST_ASSERT_FALSE(wsoft_pin_format_ok("1234567"));
  TEST_ASSERT_FALSE(wsoft_pin_format_ok("12a4"));
  TEST_ASSERT_FALSE(wsoft_pin_format_ok(" 1234"));
  TEST_ASSERT_FALSE(wsoft_pin_format_ok(""));
  TEST_ASSERT_FALSE(wsoft_pin_format_ok(nullptr));
}

static const uint8_t SALT[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

void test_hash_is_salted_sha256() {
  uint8_t hash[32];
  wsoft_pin_hash(SALT, "1234", hash);
  // sha256(salt bytes || "1234"), worked out with Python's hashlib.
  TEST_ASSERT_EQUAL_STRING("d18fa507126ac611650477696f75c190ea24fef7d70c8719a7a105b1571e244f", hex(hash, 32));
  uint8_t other_salt[16] = {0};
  uint8_t other[32];
  wsoft_pin_hash(other_salt, "1234", other);
  TEST_ASSERT_TRUE(memcmp(hash, other, 32) != 0);
  TEST_ASSERT_TRUE(wsoft_pin_matches(SALT, hash, "1234"));
  TEST_ASSERT_FALSE(wsoft_pin_matches(SALT, hash, "1235"));
  TEST_ASSERT_FALSE(wsoft_pin_matches(SALT, hash, "12345"));
  TEST_ASSERT_FALSE(wsoft_pin_matches(SALT, hash, nullptr));
}

void test_five_wrong_tries_lock_for_300_s() {
  uint8_t hash[32];
  wsoft_pin_hash(SALT, "1234", hash);
  WsoftPinGuard guard;
  uint32_t t = 1000;
  for (int i = 1; i <= 4; i++) {
    TEST_ASSERT_EQUAL(WSOFT_PIN_WRONG, wsoft_pin_try(&guard, true, SALT, hash, "9999", t));
    TEST_ASSERT_EQUAL(5 - i, guard.tries_left());
  }
  TEST_ASSERT_EQUAL(WSOFT_PIN_LOCKED, wsoft_pin_try(&guard, true, SALT, hash, "9999", t));  // the 5th wrong try
  TEST_ASSERT_EQUAL(300, (int)guard.locked_s(t));
  // Locked: even the right PIN is refused, and the try does not count.
  TEST_ASSERT_EQUAL(WSOFT_PIN_LOCKED, wsoft_pin_try(&guard, true, SALT, hash, "1234", t + 1000));
  TEST_ASSERT_EQUAL(299, (int)guard.locked_s(t + 1000));
  TEST_ASSERT_EQUAL(1, (int)guard.locked_s(t + 299999));
  TEST_ASSERT_EQUAL(0, (int)guard.locked_s(t + 300000));
  TEST_ASSERT_EQUAL(WSOFT_PIN_OK, wsoft_pin_try(&guard, true, SALT, hash, "1234", t + 300000));
  TEST_ASSERT_EQUAL(5, guard.tries_left());
}

void test_right_pin_resets_the_count() {
  uint8_t hash[32];
  wsoft_pin_hash(SALT, "1234", hash);
  WsoftPinGuard guard;
  wsoft_pin_try(&guard, true, SALT, hash, "1111", 0);
  wsoft_pin_try(&guard, true, SALT, hash, "1111", 0);
  TEST_ASSERT_EQUAL(2, (int)guard.fails());
  TEST_ASSERT_EQUAL(WSOFT_PIN_OK, wsoft_pin_try(&guard, true, SALT, hash, "1234", 0));
  TEST_ASSERT_EQUAL(0, (int)guard.fails());
}

void test_no_pin_and_bad_format() {
  uint8_t hash[32] = {0};
  WsoftPinGuard guard;
  TEST_ASSERT_EQUAL(WSOFT_PIN_NOT_SET, wsoft_pin_try(&guard, false, SALT, hash, "1234", 0));
  TEST_ASSERT_EQUAL(WSOFT_PIN_WRONG, wsoft_pin_try(&guard, true, SALT, hash, "12", 0));  // counts as a wrong try
  TEST_ASSERT_EQUAL(1, (int)guard.fails());
  TEST_ASSERT_EQUAL_STRING("PIN_WRONG", wsoft_pin_result_code(WSOFT_PIN_WRONG));
  TEST_ASSERT_EQUAL_STRING("PIN_LOCKED", wsoft_pin_result_code(WSOFT_PIN_LOCKED));
  TEST_ASSERT_EQUAL_STRING("PIN_NOT_SET", wsoft_pin_result_code(WSOFT_PIN_NOT_SET));
}

void test_lock_survives_a_restart() {
  // The board keeps the wrong-try count in flash; 5 at boot means locked from boot.
  WsoftPinGuard after_boot(5, 2000);
  TEST_ASSERT_EQUAL(300, (int)after_boot.locked_s(2000));
  WsoftPinGuard two(2, 2000);
  TEST_ASSERT_EQUAL(0, (int)two.locked_s(2000));
  TEST_ASSERT_EQUAL(3, two.tries_left());
}

void test_lock_works_across_millis_wrap() {
  uint8_t hash[32];
  wsoft_pin_hash(SALT, "1234", hash);
  WsoftPinGuard guard;
  uint32_t t = 0xFFFFFF00u;
  for (int i = 0; i < 5; i++) wsoft_pin_try(&guard, true, SALT, hash, "0000", t);
  TEST_ASSERT_EQUAL(300, (int)guard.locked_s(t + 200));  // after the wrap: still about 300 s
  TEST_ASSERT_EQUAL(0, (int)guard.locked_s(t + 300000));
}

void test_pin_pad_entry() {
  WsoftPinPad pad;
  TEST_ASSERT_EQUAL_STRING("", pad.masked());
  for (char c = '1'; c <= '8'; c++) pad.press(c);  // max 6 digits
  TEST_ASSERT_EQUAL_STRING("123456", pad.entry());
  TEST_ASSERT_EQUAL_STRING("******", pad.masked());
  pad.del();
  TEST_ASSERT_EQUAL_STRING("12345", pad.entry());
  pad.press('x');  // not a digit: ignored
  TEST_ASSERT_EQUAL_STRING("12345", pad.entry());
  pad.clear();
  TEST_ASSERT_EQUAL_STRING("", pad.entry());
  pad.del();  // nothing to delete
  TEST_ASSERT_EQUAL_STRING("", pad.entry());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_sha256_nist_vectors);
  RUN_TEST(test_sha256_in_pieces_and_a_million_a);
  RUN_TEST(test_pin_format_is_4_to_6_digits);
  RUN_TEST(test_hash_is_salted_sha256);
  RUN_TEST(test_five_wrong_tries_lock_for_300_s);
  RUN_TEST(test_right_pin_resets_the_count);
  RUN_TEST(test_no_pin_and_bad_format);
  RUN_TEST(test_lock_survives_a_restart);
  RUN_TEST(test_lock_works_across_millis_wrap);
  RUN_TEST(test_pin_pad_entry);
  return UNITY_END();
}
