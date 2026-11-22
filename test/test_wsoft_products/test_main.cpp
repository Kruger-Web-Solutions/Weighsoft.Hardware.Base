// Host tests for products made on the board (lib/wsoft_core/src/wsoft_products.cpp).
// Run: pio test -e native

#include <unity.h>

#include <stdio.h>
#include <string.h>

#include "wsoft_products.h"

void setUp() {}
void tearDown() {}

static WsoftHiloProduct make(const char *code, const char *name, int32_t low, int32_t target, int32_t high) {
  WsoftHiloProduct p;
  strncpy(p.code, code, WSOFT_HILO_CODE_MAX);
  strncpy(p.name, name, WSOFT_HILO_NAME_MAX);
  p.low_g = low;
  p.target_g = target;
  p.high_g = high;
  return p;
}

static WsoftHiloList hub_with_maize() {
  WsoftHiloList hub;
  hub.products[0] = make("MAIZE", "Maize", 7000000, 7100000, 7200000);
  hub.count = 1;
  return hub;
}

void test_check_rules() {
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_OK, wsoft_product_check(make("ONION5", "Onions", 5000, 5050, 5200)));
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_BAD_CODE, wsoft_product_check(make("ON ION", "Onions", 5000, 5050, 5200)));
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_BAD_CODE, wsoft_product_check(make("", "Onions", 5000, 5050, 5200)));
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_BAD_NAME, wsoft_product_check(make("ONION5", "", 5000, 5050, 5200)));
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_BAD_NAME, wsoft_product_check(make("ONION5", "   ", 5000, 5050, 5200)));
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_BAD_NAME, wsoft_product_check(make("ONION5", "Uie\xc3\xa9", 5000, 5050, 5200)));
  // Low below High, Target between them (equal to a limit is fine), all above zero.
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_OK, wsoft_product_check(make("A", "A", 5000, 5000, 5200)));
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_OK, wsoft_product_check(make("A", "A", 5000, 5200, 5200)));
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_BAD_LIMITS, wsoft_product_check(make("A", "A", 5200, 5200, 5200)));
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_BAD_LIMITS, wsoft_product_check(make("A", "A", 5000, 4999, 5200)));
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_BAD_LIMITS, wsoft_product_check(make("A", "A", 5000, 5201, 5200)));
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_BAD_LIMITS, wsoft_product_check(make("A", "A", 0, 1, 2)));
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_BAD_LIMITS, wsoft_product_check(make("A", "A", 1, 2, WSOFT_HILO_GRAMS_MAX + 1)));
}

void test_add_edit_remove() {
  WsoftHiloList hub = hub_with_maize();
  WsoftLocalProducts local;
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_OK, wsoft_products_put(&local, hub, nullptr, make("ONION5", "Onions", 5000, 5050, 5200)));
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_OK, wsoft_products_put(&local, hub, "", make("POT7", "Potatoes", 7000, 7050, 7250)));
  TEST_ASSERT_EQUAL(2, local.count);
  // Same code again is refused; a Hub code is refused.
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_DUPLICATE, wsoft_products_put(&local, hub, nullptr, make("POT7", "P", 1, 2, 3)));
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_HUB_CODE, wsoft_products_put(&local, hub, nullptr, make("MAIZE", "M", 1, 2, 3)));
  // Edit keeps the place and may rename; renaming onto another board code is refused.
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_OK, wsoft_products_put(&local, hub, "ONION5", make("ONION10", "Onions 10", 10000, 10100, 10300)));
  TEST_ASSERT_EQUAL_STRING("ONION10", local.products[0].code);
  TEST_ASSERT_EQUAL(10100, local.products[0].target_g);
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_DUPLICATE, wsoft_products_put(&local, hub, "ONION10", make("POT7", "x", 1, 2, 3)));
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_OK, wsoft_products_put(&local, hub, "POT7", make("POT7", "Potatoes", 7000, 7060, 7250)));
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_NOT_FOUND, wsoft_products_put(&local, hub, "GONE", make("GONE", "x", 1, 2, 3)));
  // Delete closes the gap.
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_OK, wsoft_products_remove(&local, "ONION10"));
  TEST_ASSERT_EQUAL(1, local.count);
  TEST_ASSERT_EQUAL_STRING("POT7", local.products[0].code);
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_NOT_FOUND, wsoft_products_remove(&local, "ONION10"));
  TEST_ASSERT_TRUE(wsoft_products_valid(local));
}

void test_full_at_20() {
  WsoftHiloList hub;
  WsoftLocalProducts local;
  char code[8];
  for (size_t i = 0; i < WSOFT_LOCAL_MAX; i++) {
    snprintf(code, sizeof code, "P%u", (unsigned)i);
    TEST_ASSERT_EQUAL(WSOFT_PRODUCT_OK, wsoft_products_put(&local, hub, nullptr, make(code, "x", 1, 2, 3)));
  }
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_FULL, wsoft_products_put(&local, hub, nullptr, make("P99", "x", 1, 2, 3)));
  // Editing still works when full.
  TEST_ASSERT_EQUAL(WSOFT_PRODUCT_OK, wsoft_products_put(&local, hub, "P0", make("P0", "y", 1, 2, 3)));
}

void test_find_count_next_hub_first() {
  WsoftHiloList hub = hub_with_maize();
  WsoftLocalProducts local;
  wsoft_products_put(&local, hub, nullptr, make("ONION5", "Onions", 5000, 5050, 5200));
  wsoft_products_put(&local, hub, nullptr, make("POT7", "Potatoes", 7000, 7050, 7250));
  TEST_ASSERT_EQUAL(3, wsoft_products_count(hub, local));
  TEST_ASSERT_EQUAL_STRING("MAIZE", wsoft_products_next(hub, local, ""));
  TEST_ASSERT_EQUAL_STRING("ONION5", wsoft_products_next(hub, local, "MAIZE"));
  TEST_ASSERT_EQUAL_STRING("POT7", wsoft_products_next(hub, local, "ONION5"));
  TEST_ASSERT_EQUAL_STRING("", wsoft_products_next(hub, local, "POT7"));
  TEST_ASSERT_EQUAL_STRING("MAIZE", wsoft_products_next(hub, local, "GONE"));
  TEST_ASSERT_NOT_NULL(wsoft_products_find(hub, local, "POT7"));
  TEST_ASSERT_NULL(wsoft_products_find(hub, local, ""));
  // The Hub later adds POT7: the Hub one wins and the board one is hidden.
  hub.products[1] = make("POT7", "Hub potatoes", 6000, 6100, 6200);
  hub.count = 2;
  TEST_ASSERT_EQUAL(3, wsoft_products_count(hub, local));
  TEST_ASSERT_EQUAL(6100, wsoft_products_find(hub, local, "POT7")->target_g);
  TEST_ASSERT_TRUE(wsoft_products_hidden(hub, local.products[1]));
  TEST_ASSERT_EQUAL_STRING("ONION5", wsoft_products_next(hub, local, "POT7"));
  TEST_ASSERT_EQUAL_STRING("", wsoft_products_next(hub, local, "ONION5"));
}

void test_no_products_at_all() {
  WsoftHiloList hub;
  WsoftLocalProducts local;
  TEST_ASSERT_EQUAL(0, wsoft_products_count(hub, local));
  TEST_ASSERT_EQUAL_STRING("", wsoft_products_next(hub, local, ""));
  // Board products only (no Hub): they still cycle.
  wsoft_products_put(&local, hub, nullptr, make("BEET2", "Beetroot", 2000, 2020, 2080));
  TEST_ASSERT_EQUAL_STRING("BEET2", wsoft_products_next(hub, local, ""));
  TEST_ASSERT_EQUAL_STRING("", wsoft_products_next(hub, local, "BEET2"));
}

void test_apply_keeps_a_board_pick() {
  WsoftHiloList hub = hub_with_maize();
  WsoftLocalProducts local;
  wsoft_products_put(&local, hub, nullptr, make("ONION5", "Onions", 5000, 5050, 5200));
  WsoftHiloPick pick;
  strcpy(pick.current, "ONION5");
  // A fresh Hub list without a new default keeps the board pick.
  TEST_ASSERT_FALSE(wsoft_products_apply(hub, local, &pick));
  TEST_ASSERT_EQUAL_STRING("ONION5", pick.current);
  // The plain Hub rule would have cleared it: that is why the board uses this one.
  WsoftHiloPick plain = pick;
  TEST_ASSERT_TRUE(wsoft_hilo_apply(hub, &plain));
  TEST_ASSERT_EQUAL_STRING("", plain.current);
  // A new Hub default still wins once (last change wins).
  hub.has_default = true;
  strcpy(hub.default_code, "MAIZE");
  strcpy(hub.default_set_at, "2026-11-13T20:00:00.000Z");
  TEST_ASSERT_TRUE(wsoft_products_apply(hub, local, &pick));
  TEST_ASSERT_EQUAL_STRING("MAIZE", pick.current);
  // A deleted board product is cleared.
  strcpy(pick.current, "ONION5");
  wsoft_products_remove(&local, "ONION5");
  TEST_ASSERT_TRUE(wsoft_products_apply(hub, local, &pick));
  TEST_ASSERT_EQUAL_STRING("", pick.current);
}

void test_valid_refuses_bad_stored_bytes() {
  WsoftLocalProducts local;
  TEST_ASSERT_TRUE(wsoft_products_valid(local));
  local.count = WSOFT_LOCAL_MAX + 1;
  TEST_ASSERT_FALSE(wsoft_products_valid(local));
  local = WsoftLocalProducts();
  local.products[0] = make("A", "A", 1, 2, 3);
  local.products[1] = make("A", "B", 1, 2, 3);
  local.count = 2;
  TEST_ASSERT_FALSE(wsoft_products_valid(local));
  local.products[1] = make("B", "B", 3, 2, 1);
  TEST_ASSERT_FALSE(wsoft_products_valid(local));
}

void test_words() {
  TEST_ASSERT_EQUAL_STRING("HUB_CODE", wsoft_product_result_code(WSOFT_PRODUCT_HUB_CODE));
  TEST_ASSERT_EQUAL_STRING("That code belongs to the Hub. Pick another code.",
                           wsoft_product_result_text(WSOFT_PRODUCT_HUB_CODE));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_check_rules);
  RUN_TEST(test_add_edit_remove);
  RUN_TEST(test_full_at_20);
  RUN_TEST(test_find_count_next_hub_first);
  RUN_TEST(test_no_products_at_all);
  RUN_TEST(test_apply_keeps_a_board_pick);
  RUN_TEST(test_valid_refuses_bad_stored_bytes);
  RUN_TEST(test_words);
  return UNITY_END();
}
