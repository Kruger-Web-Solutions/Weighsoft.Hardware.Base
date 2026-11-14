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
      "\"alarm_allowed\":false,\"alarm_note\":null,\"alarm_word_hex\":null,\"alarm_changes\":0,"
      "\"alarm_test\":false,\"approval_on\":false,\"for_trade\":false,\"trade_check\":\"approval_off\",\"audit_counter\":0,"
      "\"last_trade_seq\":0,\"trade_queue\":0,\"hub\":\"HUB ONLINE\",\"hub_online\":true,\"polls\":9,"
      "\"source\":\"rs485\",\"role\":\"weigh\",\"notice_code\":null,\"notice_title\":null,\"notice_detail\":null,"
      "\"notice_level\":null}",
      view(s).c_str());
}

// Rhyno, 9 Nov 2026: the web page shows why a save cannot happen, big, with the screen's words.
void test_notice_says_what_stops_a_save() {
  WSoftSnapshot s = live(31200);
  s.save_waiting = WSOFT_SAVE_NEED_ZERO;
  std::string j = view(s);
  TEST_ASSERT_TRUE(has(j, "\"notice_code\":\"NEED_ZERO\""));
  TEST_ASSERT_TRUE(has(j, "\"notice_title\":\"TAKE THE LOAD OFF\""));
  TEST_ASSERT_TRUE(has(j, "\"notice_detail\":\"The scale must show 0 kg first\""));
  TEST_ASSERT_TRUE(has(j, "\"notice_level\":\"stop\""));
  s.save_waiting = WSOFT_SAVE_NOT_STABLE;
  TEST_ASSERT_TRUE(has(view(s), "\"notice_level\":\"wait\""));
  s.save_waiting = WSOFT_SAVE_ALREADY;
  TEST_ASSERT_TRUE(has(view(s), "\"notice_level\":\"info\""));
  s.save_waiting = WSOFT_SAVE_EMPTY;  // an empty scale is not a problem
  TEST_ASSERT_TRUE(has(view(s), "\"notice_code\":null"));
  s.valid = false;  // before the first poll: nothing
  s.save_waiting = WSOFT_SAVE_NEED_ZERO;
  TEST_ASSERT_TRUE(has(view(s), "\"notice_code\":null"));
}

// ---------- RS232 weight text ----------

void test_rs232_source_is_reported() {
  WSoftSnapshot s = live(1520);
  s.rs232 = true;
  TEST_ASSERT_TRUE(has(view(s), "\"source\":\"rs232\""));
}

void test_weight_text_parses_sign_digits_and_decimals() {
  int32_t v = 0;
  int d = -1;
  TEST_ASSERT_TRUE(wsoft_parse_weight_text("12.345", &v, &d));
  TEST_ASSERT_EQUAL_INT32(12345, v);
  TEST_ASSERT_EQUAL_INT(3, d);
  TEST_ASSERT_TRUE(wsoft_parse_weight_text("  -0.50 ", &v, &d));
  TEST_ASSERT_EQUAL_INT32(-50, v);
  TEST_ASSERT_EQUAL_INT(2, d);
  TEST_ASSERT_TRUE(wsoft_parse_weight_text("+1520", &v, &d));
  TEST_ASSERT_EQUAL_INT32(1520, v);
  TEST_ASSERT_EQUAL_INT(0, d);
  TEST_ASSERT_TRUE(wsoft_parse_weight_text("1,5", &v, &d));
  TEST_ASSERT_EQUAL_INT32(15, v);
  TEST_ASSERT_EQUAL_INT(1, d);
  // W113 pads between the sign and the digits ("ST,GS,-   2820kg").
  TEST_ASSERT_TRUE(wsoft_parse_weight_text("-   2820", &v, &d));
  TEST_ASSERT_EQUAL_INT32(-2820, v);
  TEST_ASSERT_EQUAL_INT(0, d);
}

void test_weight_text_rejects_non_weights_without_change() {
  int32_t v = 77;
  int d = 9;
  TEST_ASSERT_FALSE(wsoft_parse_weight_text("", &v, &d));
  TEST_ASSERT_FALSE(wsoft_parse_weight_text("   ", &v, &d));
  TEST_ASSERT_FALSE(wsoft_parse_weight_text("-", &v, &d));
  TEST_ASSERT_FALSE(wsoft_parse_weight_text("12.3kg", &v, &d));
  TEST_ASSERT_FALSE(wsoft_parse_weight_text("1.2.3", &v, &d));
  TEST_ASSERT_FALSE(wsoft_parse_weight_text("--5", &v, &d));
  TEST_ASSERT_FALSE(wsoft_parse_weight_text("1.23456", &v, &d));
  TEST_ASSERT_FALSE(wsoft_parse_weight_text("99999999999", &v, &d));
  TEST_ASSERT_FALSE(wsoft_parse_weight_text(nullptr, &v, &d));
  TEST_ASSERT_EQUAL_INT32(77, v);
  TEST_ASSERT_EQUAL_INT(9, d);
}

// The weigh board's feed for a Writer: the number only, and nothing when there is no live weight.
void test_feed_weight_is_the_number_or_nothing() {
  char w[24];
  TEST_ASSERT_TRUE(wsoft_feed_weight(live(12345), w, sizeof w));
  TEST_ASSERT_EQUAL_STRING("12.345", w);
  WSoftSnapshot lost = live(12345);
  lost.fresh = false;  // a failed poll is never a weight
  TEST_ASSERT_FALSE(wsoft_feed_weight(lost, w, sizeof w));
  TEST_ASSERT_EQUAL_STRING("", w);
  TEST_ASSERT_FALSE(wsoft_feed_weight(WSoftSnapshot(), w, sizeof w));
}

static std::string feed(const WSoftSnapshot &s) {
  StringSink sink;
  wsoft_feed_write(s, sink);
  return sink.done();
}

// The weigh board's feed for a Writer: the keys a Writer already reads, plus unit and stable.
void test_feed_has_line_weight_unit_stable_and_time() {
  WSoftSnapshot s = live(12345);
  s.stable = true;
  s.updated_ms = 777;
  TEST_ASSERT_EQUAL_STRING(
      "{\"last_line\":\"12.345 kg\",\"weight\":\"12.345\",\"unit\":\"kg\",\"stable\":true,\"timestamp\":777}",
      feed(s).c_str());
}

// No live weight: an empty line, so a Writer sends nothing out of its port.
void test_feed_without_live_weight_sends_nothing() {
  WSoftSnapshot lost = live(12345);
  lost.fresh = false;
  lost.stable = true;
  std::string json = feed(lost);
  TEST_ASSERT_TRUE(has(json, "\"last_line\":\"\""));
  TEST_ASSERT_TRUE(has(json, "\"weight\":null"));
  TEST_ASSERT_TRUE(has(json, "\"stable\":false"));
}

// A tare on the weigh board: the Writer gets the net, as the screen shows it; the board's own
// view keeps the gross in weight_g and weight_text.
void test_feed_sends_net_when_tared() {
  WSoftSnapshot s = live(12345);
  s.stable = true;
  s.updated_ms = 5;
  s.tared = true;
  s.net_value = 10345;  // 2 kg tare
  char w[24];
  TEST_ASSERT_TRUE(wsoft_feed_weight(s, w, sizeof w));
  TEST_ASSERT_EQUAL_STRING("10.345", w);
  TEST_ASSERT_EQUAL_STRING(
      "{\"last_line\":\"10.345 kg\",\"weight\":\"10.345\",\"unit\":\"kg\",\"stable\":true,\"timestamp\":5}",
      feed(s).c_str());
  TEST_ASSERT_TRUE(has(view(s), "\"weight_text\":\"12.345 kg\""));
  // Tare off: the gross, even if an old net is still in the snapshot.
  s.tared = false;
  TEST_ASSERT_TRUE(has(feed(s), "\"weight\":\"12.345\""));
  // No live weight: still nothing, tare or not.
  s.tared = true;
  s.fresh = false;
  TEST_ASSERT_TRUE(has(feed(s), "\"weight\":null"));
}

void test_role_is_weigh_or_writer() {
  WSoftSnapshot s = live(1520);
  TEST_ASSERT_TRUE(has(view(s), "\"role\":\"weigh\""));
  s.writer = true;
  TEST_ASSERT_TRUE(has(view(s), "\"role\":\"writer\""));
}


// Rhyno, 14 Nov 2026: the EEPROM alarm is allowed but still shown on the status page.
void test_allowed_alarm_is_flagged_not_blocking() {
  WSoftSnapshot s = live(1520);
  s.alarm_allowed = true;
  s.alarm_word_valid = true;
  s.alarm_word = 0x80000000u;
  s.alarm_changes = 2;
  std::string j = view(s);
  TEST_ASSERT_TRUE(has(j, "\"link\":\"live\""));
  TEST_ASSERT_TRUE(has(j, "\"alarm\":false"));
  TEST_ASSERT_TRUE(has(j, "\"alarm_allowed\":true"));
  TEST_ASSERT_TRUE(has(j, "\"alarm_note\":\"EEPROM alarm allowed (supplier: factory flag), monitoring\""));
  TEST_ASSERT_TRUE(has(j, "\"alarm_word_hex\":\"80000000\""));
  TEST_ASSERT_TRUE(has(j, "\"alarm_changes\":2"));
}

// Rhyno, 14 Nov 2026: the alarm test switch shows on the live page while it is on.
void test_alarm_test_switch_shows_on_the_page() {
  WSoftSnapshot s = live(1520);
  TEST_ASSERT_TRUE(has(view(s), "\"alarm_test\":false"));
  s.alarm_test = true;
  TEST_ASSERT_TRUE(has(view(s), "\"alarm_test\":true"));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_alarm_test_switch_shows_on_the_page);
  RUN_TEST(test_allowed_alarm_is_flagged_not_blocking);
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
  RUN_TEST(test_rs232_source_is_reported);
  RUN_TEST(test_notice_says_what_stops_a_save);
  RUN_TEST(test_weight_text_parses_sign_digits_and_decimals);
  RUN_TEST(test_weight_text_rejects_non_weights_without_change);
  RUN_TEST(test_feed_weight_is_the_number_or_nothing);
  RUN_TEST(test_feed_has_line_weight_unit_stable_and_time);
  RUN_TEST(test_feed_without_live_weight_sends_nothing);
  RUN_TEST(test_feed_sends_net_when_tared);
  RUN_TEST(test_role_is_weigh_or_writer);
  return UNITY_END();
}
