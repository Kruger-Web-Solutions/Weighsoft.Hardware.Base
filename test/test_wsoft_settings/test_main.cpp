// Host tests for the W-Soft web options (lib/wsoft_core/src/wsoft_settings.cpp).
// Run: pio test -e native   (needs a host g++ on PATH)

#include <unity.h>

#include <string.h>

#include "wsoft_settings.h"

static WSoftSettingsData defaults() {
  WSoftSettingsData s;
  wsoft_settings_defaults(&s);
  return s;
}

void setUp() {}
void tearDown() {}

void test_defaults() {
  WSoftSettingsData s = defaults();
  TEST_ASSERT_EQUAL_UINT32(1000, s.publish_interval_ms);
  TEST_ASSERT_TRUE(s.mqtt_enabled);
  TEST_ASSERT_EQUAL_STRING("weighsoft/wsoft/#{unique_id}", s.mqtt_base_path);
  TEST_ASSERT_EQUAL_STRING("wsoft-#{unique_id}", s.mqtt_name);
  TEST_ASSERT_EQUAL_STRING("wsoft-#{unique_id}", s.mqtt_unique_id);
}

void test_empty_input_restores_defaults_and_reports_unchanged() {
  WSoftSettingsData s = defaults();
  WSoftSettingsInput in;
  TEST_ASSERT_EQUAL(WSOFT_SETTINGS_UNCHANGED, wsoft_settings_update(in, &s));
  TEST_ASSERT_EQUAL_UINT32(1000, s.publish_interval_ms);
}

void test_missing_field_falls_back_to_default() {
  WSoftSettingsData s = defaults();
  s.publish_interval_ms = 5000;
  s.mqtt_enabled = false;
  WSoftSettingsInput in;
  TEST_ASSERT_EQUAL(WSOFT_SETTINGS_CHANGED, wsoft_settings_update(in, &s));
  TEST_ASSERT_EQUAL_UINT32(1000, s.publish_interval_ms);
  TEST_ASSERT_TRUE(s.mqtt_enabled);
}

void test_valid_update_applies_every_field() {
  WSoftSettingsData s = defaults();
  WSoftSettingsInput in;
  in.has_publish_interval_ms = true;
  in.publish_interval_ms = 2500;
  in.has_mqtt_enabled = true;
  in.mqtt_enabled = false;
  in.mqtt_base_path = "site/line1/#{unique_id}";
  in.mqtt_name = "Line 1 screen";
  in.mqtt_unique_id = "line1-screen";
  TEST_ASSERT_EQUAL(WSOFT_SETTINGS_CHANGED, wsoft_settings_update(in, &s));
  TEST_ASSERT_EQUAL_UINT32(2500, s.publish_interval_ms);
  TEST_ASSERT_FALSE(s.mqtt_enabled);
  TEST_ASSERT_EQUAL_STRING("site/line1/#{unique_id}", s.mqtt_base_path);
  TEST_ASSERT_EQUAL_STRING("Line 1 screen", s.mqtt_name);
  TEST_ASSERT_EQUAL_STRING("line1-screen", s.mqtt_unique_id);
}

void test_same_values_are_unchanged() {
  WSoftSettingsData s = defaults();
  WSoftSettingsInput in;
  in.has_publish_interval_ms = true;
  in.publish_interval_ms = 1000;
  in.mqtt_base_path = "weighsoft/wsoft/#{unique_id}";
  TEST_ASSERT_EQUAL(WSOFT_SETTINGS_UNCHANGED, wsoft_settings_update(in, &s));
}

void test_interval_is_clamped_both_ways() {
  WSoftSettingsData s = defaults();
  WSoftSettingsInput in;
  in.has_publish_interval_ms = true;
  in.publish_interval_ms = 10;
  wsoft_settings_update(in, &s);
  TEST_ASSERT_EQUAL_UINT32(WSOFT_PUBLISH_MIN_MS, s.publish_interval_ms);
  in.publish_interval_ms = -5;
  wsoft_settings_update(in, &s);
  TEST_ASSERT_EQUAL_UINT32(WSOFT_PUBLISH_MIN_MS, s.publish_interval_ms);
  in.publish_interval_ms = 99999999;
  wsoft_settings_update(in, &s);
  TEST_ASSERT_EQUAL_UINT32(WSOFT_PUBLISH_MAX_MS, s.publish_interval_ms);
}

// ---------- auto-save ----------

void test_auto_save_defaults_off_with_three_seconds() {
  WSoftSettingsData s = defaults();
  TEST_ASSERT_FALSE(s.auto_save);
  TEST_ASSERT_EQUAL_UINT32(3, s.auto_save_stable_s);
}

void test_save_ignore_alarm_defaults_off_and_applies() {
  WSoftSettingsData s = defaults();
  TEST_ASSERT_FALSE(s.save_ignore_alarm);
  WSoftSettingsInput in;
  in.has_save_ignore_alarm = true;
  in.save_ignore_alarm = true;
  TEST_ASSERT_EQUAL(WSOFT_SETTINGS_CHANGED, wsoft_settings_update(in, &s));
  TEST_ASSERT_TRUE(s.save_ignore_alarm);
  WSoftSettingsInput none;
  TEST_ASSERT_EQUAL(WSOFT_SETTINGS_CHANGED, wsoft_settings_update(none, &s));
  TEST_ASSERT_FALSE(s.save_ignore_alarm);
}

void test_auto_save_update_applies_and_reports_changed() {
  WSoftSettingsData s = defaults();
  WSoftSettingsInput in;
  in.has_auto_save = true;
  in.auto_save = true;
  in.has_auto_save_stable_s = true;
  in.auto_save_stable_s = 7;
  TEST_ASSERT_EQUAL(WSOFT_SETTINGS_CHANGED, wsoft_settings_update(in, &s));
  TEST_ASSERT_TRUE(s.auto_save);
  TEST_ASSERT_EQUAL_UINT32(7, s.auto_save_stable_s);
  TEST_ASSERT_EQUAL(WSOFT_SETTINGS_UNCHANGED, wsoft_settings_update(in, &s));
}

void test_auto_save_time_is_clamped_both_ways() {
  WSoftSettingsData s = defaults();
  WSoftSettingsInput in;
  in.has_auto_save_stable_s = true;
  in.auto_save_stable_s = 0;
  wsoft_settings_update(in, &s);
  TEST_ASSERT_EQUAL_UINT32(WSOFT_AUTO_SAVE_MIN_S, s.auto_save_stable_s);
  in.auto_save_stable_s = -3;
  wsoft_settings_update(in, &s);
  TEST_ASSERT_EQUAL_UINT32(1, s.auto_save_stable_s);
  in.auto_save_stable_s = 31;
  wsoft_settings_update(in, &s);
  TEST_ASSERT_EQUAL_UINT32(WSOFT_AUTO_SAVE_MAX_S, s.auto_save_stable_s);
  in.auto_save_stable_s = 30;
  wsoft_settings_update(in, &s);
  TEST_ASSERT_EQUAL_UINT32(30, s.auto_save_stable_s);
}

void test_missing_auto_save_fields_fall_back_to_defaults() {
  WSoftSettingsData s = defaults();
  s.auto_save = true;
  s.auto_save_stable_s = 12;
  WSoftSettingsInput in;
  TEST_ASSERT_EQUAL(WSOFT_SETTINGS_CHANGED, wsoft_settings_update(in, &s));
  TEST_ASSERT_FALSE(s.auto_save);
  TEST_ASSERT_EQUAL_UINT32(3, s.auto_save_stable_s);
}

void test_bad_topic_keeps_the_old_auto_save() {
  WSoftSettingsData s = defaults();
  s.auto_save = true;
  s.auto_save_stable_s = 9;
  WSoftSettingsInput in;
  in.has_auto_save = true;
  in.auto_save = false;
  in.mqtt_base_path = "a/+/b";
  TEST_ASSERT_EQUAL(WSOFT_SETTINGS_ERROR, wsoft_settings_update(in, &s));
  TEST_ASSERT_TRUE(s.auto_save);
  TEST_ASSERT_EQUAL_UINT32(9, s.auto_save_stable_s);
}

static void expect_topic_rejected(const char *topic) {
  WSoftSettingsData s = defaults();
  s.publish_interval_ms = 4000;
  WSoftSettingsInput in;
  in.has_publish_interval_ms = true;
  in.publish_interval_ms = 2000;
  in.mqtt_base_path = topic;
  TEST_ASSERT_EQUAL_MESSAGE(WSOFT_SETTINGS_ERROR, wsoft_settings_update(in, &s), topic);
  // nothing changes on error, not even the valid fields
  TEST_ASSERT_EQUAL_UINT32(4000, s.publish_interval_ms);
  TEST_ASSERT_EQUAL_STRING("weighsoft/wsoft/#{unique_id}", s.mqtt_base_path);
}

void test_bad_topics_are_rejected_without_change() {
  expect_topic_rejected("");
  expect_topic_rejected("a/+/b");
  expect_topic_rejected("a/#");
  expect_topic_rejected("a b");
  expect_topic_rejected("/leading");
  expect_topic_rejected("trailing/");
  char long_topic[WSOFT_MQTT_PATH_MAX + 2];
  memset(long_topic, 'a', sizeof long_topic - 1);
  long_topic[sizeof long_topic - 1] = '\0';
  expect_topic_rejected(long_topic);
}

void test_topic_at_max_length_is_accepted() {
  char topic[WSOFT_MQTT_PATH_MAX + 1];
  memset(topic, 'a', sizeof topic - 1);
  topic[sizeof topic - 1] = '\0';
  TEST_ASSERT_TRUE(wsoft_settings_valid_topic(topic));
  TEST_ASSERT_TRUE(wsoft_settings_valid_topic("weighsoft/#{platform}/#{unique_id}"));
}

void test_bad_names_are_rejected() {
  WSoftSettingsData s = defaults();
  WSoftSettingsInput in;
  in.mqtt_name = "";
  TEST_ASSERT_EQUAL(WSOFT_SETTINGS_ERROR, wsoft_settings_update(in, &s));
  WSoftSettingsInput in2;
  in2.mqtt_unique_id = "has space";
  TEST_ASSERT_EQUAL(WSOFT_SETTINGS_ERROR, wsoft_settings_update(in2, &s));
  WSoftSettingsInput in4;
  in4.mqtt_name = "tab\there";
  TEST_ASSERT_EQUAL(WSOFT_SETTINGS_ERROR, wsoft_settings_update(in4, &s));
  in4.mqtt_name = " padded";
  TEST_ASSERT_EQUAL(WSOFT_SETTINGS_ERROR, wsoft_settings_update(in4, &s));
  char long_id[WSOFT_MQTT_ID_MAX + 2];
  memset(long_id, 'x', sizeof long_id - 1);
  long_id[sizeof long_id - 1] = '\0';
  WSoftSettingsInput in3;
  in3.mqtt_unique_id = long_id;
  TEST_ASSERT_EQUAL(WSOFT_SETTINGS_ERROR, wsoft_settings_update(in3, &s));
  TEST_ASSERT_EQUAL_STRING("wsoft-#{unique_id}", s.mqtt_unique_id);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_defaults);
  RUN_TEST(test_empty_input_restores_defaults_and_reports_unchanged);
  RUN_TEST(test_missing_field_falls_back_to_default);
  RUN_TEST(test_valid_update_applies_every_field);
  RUN_TEST(test_same_values_are_unchanged);
  RUN_TEST(test_interval_is_clamped_both_ways);
  RUN_TEST(test_bad_topics_are_rejected_without_change);
  RUN_TEST(test_topic_at_max_length_is_accepted);
  RUN_TEST(test_bad_names_are_rejected);
  RUN_TEST(test_auto_save_defaults_off_with_three_seconds);
  RUN_TEST(test_auto_save_update_applies_and_reports_changed);
  RUN_TEST(test_auto_save_time_is_clamped_both_ways);
  RUN_TEST(test_missing_auto_save_fields_fall_back_to_defaults);
  RUN_TEST(test_save_ignore_alarm_defaults_off_and_applies);
  RUN_TEST(test_bad_topic_keeps_the_old_auto_save);
  return UNITY_END();
}
