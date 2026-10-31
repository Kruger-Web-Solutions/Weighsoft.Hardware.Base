// Host tests for the live W-Soft view (lib/wsoft_core/src/wsoft_view.cpp).
// Run: pio test -e native   (needs a host g++ on PATH)

#include <unity.h>

#include <stdio.h>
#include <string>

#include "wsoft_view.h"

// Builds compact JSON so a whole view can be compared as one string.
class StringSink : public WSoftJsonSink {
 public:
  std::string json = "{";
  void put_null(const char *key) override { field(key, "null"); }
  void put_bool(const char *key, bool value) override { field(key, value ? "true" : "false"); }
  void put_int(const char *key, int64_t value) override {
    char text[24];
    snprintf(text, sizeof text, "%lld", (long long)value);
    field(key, text);
  }
  void put_str(const char *key, const char *value) override { field(key, "\"" + std::string(value) + "\""); }
  std::string done() { return json + "}"; }

 private:
  void field(const char *key, const std::string &value) {
    if (json.size() > 1) json += ",";
    json += "\"" + std::string(key) + "\":" + value;
  }
};

static std::string view(const WSoftSnapshot &s) {
  StringSink sink;
  wsoft_view_write(s, sink);
  return sink.done();
}

static WSoftSnapshot live(int32_t value, int decimals = 3, int unit = 1) {
  WSoftSnapshot s;
  s.valid = true;
  s.display_ready = true;
  s.linked = true;
  s.fresh = true;
  s.value = value;
  s.decimals = decimals;
  s.unit = unit;
  return s;
}

static bool has(const std::string &json, const char *fragment) {
  return json.find(fragment) != std::string::npos;
}

void setUp() {}
void tearDown() {}

// ---------- whole grams ----------

void test_grams_from_kg_with_three_decimals() {
  int64_t g = 0;
  TEST_ASSERT_TRUE(wsoft_weight_grams(1520, 3, 1, &g));
  TEST_ASSERT_EQUAL_INT64(1520, g);
}

void test_grams_from_kg_with_no_decimals() {
  int64_t g = 0;
  TEST_ASSERT_TRUE(wsoft_weight_grams(12, 0, 1, &g));
  TEST_ASSERT_EQUAL_INT64(12000, g);
}

void test_grams_from_tonnes() {
  int64_t g = 0;
  TEST_ASSERT_TRUE(wsoft_weight_grams(1520, 3, 2, &g));
  TEST_ASSERT_EQUAL_INT64(1520000, g);
}

void test_grams_from_grams() {
  int64_t g = 0;
  TEST_ASSERT_TRUE(wsoft_weight_grams(250, 0, 3, &g));
  TEST_ASSERT_EQUAL_INT64(250, g);
}

void test_sub_gram_rounds_half_away_from_zero() {
  int64_t g = 0;
  TEST_ASSERT_TRUE(wsoft_weight_grams(15205, 4, 1, &g));  // 1.5205 kg
  TEST_ASSERT_EQUAL_INT64(1521, g);
  TEST_ASSERT_TRUE(wsoft_weight_grams(-15205, 4, 1, &g));
  TEST_ASSERT_EQUAL_INT64(-1521, g);
  TEST_ASSERT_TRUE(wsoft_weight_grams(15204, 4, 1, &g));
  TEST_ASSERT_EQUAL_INT64(1520, g);
}

void test_negative_weight_stays_negative() {
  int64_t g = 0;
  TEST_ASSERT_TRUE(wsoft_weight_grams(-20, 3, 1, &g));
  TEST_ASSERT_EQUAL_INT64(-20, g);
}

void test_no_unit_has_no_grams() {
  int64_t g = 77;
  TEST_ASSERT_FALSE(wsoft_weight_grams(1520, 3, 0, &g));
  TEST_ASSERT_FALSE(wsoft_weight_grams(1520, 3, 9, &g));
  TEST_ASSERT_FALSE(wsoft_weight_grams(1520, -1, 1, &g));
  TEST_ASSERT_FALSE(wsoft_weight_grams(1520, 7, 1, &g));
  TEST_ASSERT_EQUAL_INT64(77, g);
}

void test_large_value_does_not_overflow() {
  int64_t g = 0;
  TEST_ASSERT_TRUE(wsoft_weight_grams(2147483647, 0, 2, &g));
  TEST_ASSERT_EQUAL_INT64(2147483647000000LL, g);
}

// ---------- weight is null unless the last poll answered ----------

void test_live_reading_is_whole_grams_and_text() {
  std::string j = view(live(1520));
  TEST_ASSERT_TRUE(has(j, "\"weight_g\":1520,"));
  TEST_ASSERT_TRUE(has(j, "\"weight_text\":\"1.520 kg\""));
  TEST_ASSERT_TRUE(has(j, "\"link\":\"live\""));
}

void test_failed_last_poll_is_never_a_weight() {
  WSoftSnapshot s = live(1520);
  s.fresh = false;  // still linked (fewer than 3 fails) but this poll failed
  s.stable = true;
  std::string j = view(s);
  TEST_ASSERT_TRUE(has(j, "\"weight_g\":null"));
  TEST_ASSERT_TRUE(has(j, "\"weight_text\":null"));
  TEST_ASSERT_TRUE(has(j, "\"link\":\"no_reply\""));
  TEST_ASSERT_TRUE(has(j, "\"stable\":false"));
}

void test_not_linked_is_null_weight() {
  WSoftSnapshot s = live(1520);
  s.linked = false;
  s.fresh = false;
  std::string j = view(s);
  TEST_ASSERT_TRUE(has(j, "\"weight_g\":null"));
  TEST_ASSERT_TRUE(has(j, "\"link\":\"no_signal\""));
  TEST_ASSERT_TRUE(has(j, "\"linked\":false"));
}

void test_before_first_poll_is_null_and_starting() {
  WSoftSnapshot s;
  s.linked = true;  // even if a field is set, an unpublished snapshot shows nothing
  s.fresh = true;
  s.value = 5;
  std::string j = view(s);
  TEST_ASSERT_TRUE(has(j, "\"ready\":false"));
  TEST_ASSERT_TRUE(has(j, "\"weight_g\":null"));
  TEST_ASSERT_TRUE(has(j, "\"link\":\"starting\""));
}

void test_unit_none_shows_text_but_null_grams() {
  std::string j = view(live(1520, 3, 0));
  TEST_ASSERT_TRUE(has(j, "\"weight_g\":null"));
  TEST_ASSERT_TRUE(has(j, "\"weight_text\":\"1.520\""));
}

void test_alarm_is_reported_with_weight() {
  WSoftSnapshot s = live(1520);
  s.alarm = true;
  std::string j = view(s);
  TEST_ASSERT_TRUE(has(j, "\"link\":\"alarm\""));
  TEST_ASSERT_TRUE(has(j, "\"alarm\":true"));
}

// ---------- approval is shown, never invented ----------

void test_approval_off_by_default_is_not_for_trade() {
  WSoftSnapshot s;
  std::string j = view(s);
  TEST_ASSERT_TRUE(has(j, "\"approval_on\":false"));
  TEST_ASSERT_TRUE(has(j, "\"for_trade\":false"));
  TEST_ASSERT_TRUE(has(j, "\"trade_check\":\"approval_off\""));
}

void test_trade_ok_is_for_trade_with_seq_and_audit() {
  WSoftSnapshot s = live(1520);
  s.approval_on = true;
  s.stable = true;
  s.trade_check = TRADE_OK;
  s.audit_counter = 4;
  s.last_trade_seq = 17;
  s.trade_queue = 2;
  std::string j = view(s);
  TEST_ASSERT_TRUE(has(j, "\"for_trade\":true"));
  TEST_ASSERT_TRUE(has(j, "\"stable\":true"));
  TEST_ASSERT_TRUE(has(j, "\"audit_counter\":4"));
  TEST_ASSERT_TRUE(has(j, "\"last_trade_seq\":17"));
  TEST_ASSERT_TRUE(has(j, "\"trade_queue\":2"));
}

void test_full_view_field_order_is_stable() {
  WSoftSnapshot s = live(1520);
  s.polls = 9;
  s.hub_online = true;
  snprintf(s.hub_text, sizeof s.hub_text, "%s", "HUB ONLINE");
  TEST_ASSERT_EQUAL_STRING(
      "{\"ready\":true,\"display_ready\":true,\"link\":\"live\",\"linked\":true,\"weight_g\":1520,"
      "\"weight_text\":\"1.520 kg\",\"unit\":\"kg\",\"decimals\":3,\"stable\":false,\"alarm\":false,"
      "\"approval_on\":false,\"for_trade\":false,\"trade_check\":\"approval_off\",\"audit_counter\":0,"
      "\"last_trade_seq\":0,\"trade_queue\":0,\"hub\":\"HUB ONLINE\",\"hub_online\":true,\"polls\":9}",
      view(s).c_str());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_grams_from_kg_with_three_decimals);
  RUN_TEST(test_grams_from_kg_with_no_decimals);
  RUN_TEST(test_grams_from_tonnes);
  RUN_TEST(test_grams_from_grams);
  RUN_TEST(test_sub_gram_rounds_half_away_from_zero);
  RUN_TEST(test_negative_weight_stays_negative);
  RUN_TEST(test_no_unit_has_no_grams);
  RUN_TEST(test_large_value_does_not_overflow);
  RUN_TEST(test_live_reading_is_whole_grams_and_text);
  RUN_TEST(test_failed_last_poll_is_never_a_weight);
  RUN_TEST(test_not_linked_is_null_weight);
  RUN_TEST(test_before_first_poll_is_null_and_starting);
  RUN_TEST(test_unit_none_shows_text_but_null_grams);
  RUN_TEST(test_alarm_is_reported_with_weight);
  RUN_TEST(test_approval_off_by_default_is_not_for_trade);
  RUN_TEST(test_trade_ok_is_for_trade_with_seq_and_audit);
  RUN_TEST(test_full_view_field_order_is_stable);
  return UNITY_END();
}
