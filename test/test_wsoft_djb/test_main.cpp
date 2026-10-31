// Host tests for the DJB register map and Modbus frames.
// Run: pio test -e native

#include <unity.h>

#include <string.h>

#include "wsoft_djb.h"

void setUp() {}
void tearDown() {}

static WsoftDjbPlan plan_ok(const char *op, const char *field, int channel, bool has_channel, int32_t value, bool has_value) {
  WsoftDjbPlan plan;
  TEST_ASSERT_TRUE(wsoft_djb_plan(op, field, channel, has_channel, value, has_value, &plan));
  TEST_ASSERT_TRUE(plan.ok);
  return plan;
}

void test_lc1_zero_frame() {
  WsoftDjbPlan plan = plan_ok("write", "ad", 1, true, 0, true);
  TEST_ASSERT_EQUAL_UINT16(41003, plan.doc);
  TEST_ASSERT_EQUAL_UINT16(1002, plan.pdu);
  TEST_ASSERT_EQUAL_UINT8(0x10, plan.command);

  uint8_t frame[16];
  size_t n = wsoft_djb_build(1, &plan, frame, sizeof frame);
  TEST_ASSERT_EQUAL_UINT(13, n);
  char hex[48];
  wsoft_djb_hex(frame, n, hex, sizeof hex);
  TEST_ASSERT_EQUAL_STRING("01 10 03 EA 00 02 04 00 00 00 00 69 68", hex);
}

void test_lc1_zero_ack() {
  const uint8_t ack[] = {0x01, 0x10, 0x03, 0xEA, 0x00, 0x02, 0x60, 0x78};
  int32_t value = 0;
  uint8_t ex = 0;
  TEST_ASSERT_EQUAL_INT(0, wsoft_djb_parse(ack, sizeof ack, 1, 0x10, 1002, &value, &ex));
  TEST_ASSERT_EQUAL_INT32(0, value);
}

void test_channel_addresses() {
  const uint16_t ad[] = {41003, 41013, 41023, 41033, 41043, 41053, 41063, 41073,
                         41503, 41513, 41523, 41533, 41543, 41553, 41563, 41573};
  for (int channel = 1; channel <= 16; channel++) {
    uint16_t doc = 0;
    TEST_ASSERT_TRUE(wsoft_djb_doc(WSOFT_DJB_AD, channel, &doc));
    TEST_ASSERT_EQUAL_UINT16(ad[channel - 1], doc);
    TEST_ASSERT_TRUE(wsoft_djb_doc(WSOFT_DJB_WEIGHT, channel, &doc));
    TEST_ASSERT_EQUAL_UINT16(ad[channel - 1] + 2, doc);
    TEST_ASSERT_TRUE(wsoft_djb_doc(WSOFT_DJB_ALARM, channel, &doc));
    TEST_ASSERT_EQUAL_UINT16(ad[channel - 1] + 4, doc);
  }
  uint16_t doc = 0;
  TEST_ASSERT_FALSE(wsoft_djb_doc(WSOFT_DJB_AD, 0, &doc));
  TEST_ASSERT_FALSE(wsoft_djb_doc(WSOFT_DJB_AD, 17, &doc));
}

void test_parameter_addresses() {
  struct Row {
    const char *name;
    uint16_t doc;
  };
  const Row rows[] = {
      {"unused", 41301},       {"unit", 41303},
      {"decimal", 41305},      {"division", 41307},
      {"stability_range", 41309}, {"stability_time", 41311},
      {"zero_track_limit", 41313}, {"zero_track_time", 41315},
      {"zero_display", 41317}, {"sampling", 41319},
      {"filter1", 41321},      {"filter2", 41323},
      {"address", 41203},      {"baud", 41205},
      {"byte_order", 41207},   {"com1_mode", 41209},
  };
  for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++) {
    WsoftDjbPlan plan = plan_ok("read", rows[i].name, 0, false, 0, false);
    TEST_ASSERT_EQUAL_UINT16(rows[i].doc, plan.doc);
    TEST_ASSERT_EQUAL_UINT16(rows[i].doc - 40001, plan.pdu);
    TEST_ASSERT_EQUAL_UINT8(0x03, plan.command);
    // 1 is inside every limit except byte_order (0 only).
    int32_t value = strcmp(rows[i].name, "byte_order") == 0 ? 0 : 1;
    plan = plan_ok("write", rows[i].name, 0, false, value, true);
    TEST_ASSERT_EQUAL_UINT8(0x10, plan.command);
    TEST_ASSERT_EQUAL_INT32(value, plan.value);
  }
}

void test_link_and_format_limits() {
  struct Row {
    const char *name;
    int32_t low_bad;
    int32_t low_ok;
    int32_t high_ok;
    int32_t high_bad;
  };
  const Row rows[] = {
      {"unit", -1, 0, 3, 4},       {"decimal", -1, 0, 4, 5}, {"address", 0, 1, 10, 11},
      {"baud", -1, 0, 2, 3},       {"byte_order", -1, 0, 0, 1}, {"com1_mode", 0, 1, 1, 2},
  };
  for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++) {
    WsoftDjbPlan plan;
    TEST_ASSERT_FALSE(wsoft_djb_plan("write", rows[i].name, 0, false, rows[i].low_bad, true, &plan));
    TEST_ASSERT_EQUAL_STRING("range", plan.error);
    TEST_ASSERT_FALSE(wsoft_djb_plan("write", rows[i].name, 0, false, rows[i].high_bad, true, &plan));
    TEST_ASSERT_EQUAL_STRING("range", plan.error);
    TEST_ASSERT_TRUE(wsoft_djb_plan("write", rows[i].name, 0, false, rows[i].low_ok, true, &plan));
    TEST_ASSERT_TRUE(wsoft_djb_plan("write", rows[i].name, 0, false, rows[i].high_ok, true, &plan));
    // Reads are never range-checked.
    TEST_ASSERT_TRUE(wsoft_djb_plan("read", rows[i].name, 0, false, rows[i].high_bad, true, &plan));
  }
  // Fields without a known code range stay raw.
  WsoftDjbPlan plan;
  TEST_ASSERT_TRUE(wsoft_djb_plan("write", "filter2", 0, false, 1024, true, &plan));
}

void test_weight_and_alarm_are_read_only() {
  WsoftDjbPlan plan;
  TEST_ASSERT_FALSE(wsoft_djb_plan("write", "weight", 1, true, 0, true, &plan));
  TEST_ASSERT_EQUAL_STRING("read_only", plan.error);
  TEST_ASSERT_FALSE(wsoft_djb_plan("write", "alarm", 1, true, 0, true, &plan));
  TEST_ASSERT_EQUAL_STRING("read_only", plan.error);
  TEST_ASSERT_TRUE(wsoft_djb_plan("read", "weight", 1, true, 0, false, &plan));
  TEST_ASSERT_EQUAL_UINT16(41005, plan.doc);
  TEST_ASSERT_EQUAL_UINT16(1004, plan.pdu);
}

void test_ad_range_and_missing_value() {
  WsoftDjbPlan plan;
  TEST_ASSERT_FALSE(wsoft_djb_plan("write", "ad", 1, true, 0, false, &plan));
  TEST_ASSERT_EQUAL_STRING("need_value", plan.error);
  TEST_ASSERT_FALSE(wsoft_djb_plan("write", "ad", 1, true, 1000001, true, &plan));
  TEST_ASSERT_EQUAL_STRING("ad_range", plan.error);
  TEST_ASSERT_TRUE(wsoft_djb_plan("write", "ad", 2, true, 1000000, true, &plan));
  TEST_ASSERT_EQUAL_UINT16(41013, plan.doc);
  // 0 is zero calibration, CW > 0 load calibration; a negative CW means nothing.
  TEST_ASSERT_FALSE(wsoft_djb_plan("write", "ad", 1, true, -1, true, &plan));
  TEST_ASSERT_EQUAL_STRING("ad_range", plan.error);
}

void test_read_roundtrip_and_exception() {
  WsoftDjbPlan plan = plan_ok("read", "weight", 1, true, 0, false);
  uint8_t frame[16];
  TEST_ASSERT_EQUAL_UINT(8, wsoft_djb_build(1, &plan, frame, sizeof frame));
  TEST_ASSERT_EQUAL_UINT8(0x01, frame[0]);
  TEST_ASSERT_EQUAL_UINT8(0x03, frame[1]);
  TEST_ASSERT_EQUAL_UINT8(0x03, frame[2]);
  TEST_ASSERT_EQUAL_UINT8(0xEC, frame[3]);
  TEST_ASSERT_EQUAL_UINT16(wsoft_djb_crc16(frame, 6), (uint16_t)(frame[6] | (frame[7] << 8)));

  uint8_t reply[9] = {0x01, 0x03, 0x04, 0xFF, 0xFF, 0xFF, 0x38, 0, 0};
  uint16_t crc = wsoft_djb_crc16(reply, 7);
  reply[7] = (uint8_t)crc;
  reply[8] = (uint8_t)(crc >> 8);
  int32_t value = 0;
  uint8_t ex = 0;
  TEST_ASSERT_EQUAL_INT(0, wsoft_djb_parse(reply, sizeof reply, 1, 0x03, 1004, &value, &ex));
  TEST_ASSERT_EQUAL_INT32(-200, value);

  uint8_t bad[5] = {0x01, 0x90, 0x02, 0, 0};
  crc = wsoft_djb_crc16(bad, 3);
  bad[3] = (uint8_t)crc;
  bad[4] = (uint8_t)(crc >> 8);
  TEST_ASSERT_EQUAL_INT(3, wsoft_djb_parse(bad, sizeof bad, 1, 0x10, 1002, &value, &ex));
  TEST_ASSERT_EQUAL_UINT8(0x02, ex);
}

void test_alarm_bits_are_on_the_high_word() {
  WsoftDjbAlarmBits bits;
  wsoft_djb_alarm_bits((int32_t)0x80000000u, &bits);
  TEST_ASSERT_TRUE(bits.eeprom);
  TEST_ASSERT_FALSE(bits.over_range);
  TEST_ASSERT_FALSE(bits.parameter_changed);

  wsoft_djb_alarm_bits((int32_t)0xC0000000u, &bits);
  TEST_ASSERT_TRUE(bits.eeprom);
  TEST_ASSERT_TRUE(bits.over_range);
  TEST_ASSERT_FALSE(bits.adc_failure);

  wsoft_djb_alarm_bits((int32_t)0x00010000u, &bits);
  TEST_ASSERT_TRUE(bits.parameter_changed);
  TEST_ASSERT_FALSE(bits.software_match);
  TEST_ASSERT_FALSE(bits.eeprom);

  wsoft_djb_alarm_bits((int32_t)0x00020000u, &bits);
  TEST_ASSERT_TRUE(bits.software_match);
  TEST_ASSERT_FALSE(bits.parameter_changed);

  wsoft_djb_alarm_bits((int32_t)0x20000000u, &bits);
  TEST_ASSERT_TRUE(bits.adc_failure);

  wsoft_djb_alarm_bits(1, &bits);
  TEST_ASSERT_FALSE(bits.parameter_changed);
  TEST_ASSERT_FALSE(bits.software_match);
  TEST_ASSERT_FALSE(bits.adc_failure);
  TEST_ASSERT_FALSE(bits.over_range);
  TEST_ASSERT_FALSE(bits.eeprom);
}

void test_sampling_stays_a_raw_long() {
  WsoftDjbPlan plan = plan_ok("write", "sampling", 0, false, 0, true);
  TEST_ASSERT_EQUAL_UINT16(41319, plan.doc);
  TEST_ASSERT_EQUAL_UINT16(1318, plan.pdu);
  TEST_ASSERT_EQUAL_INT32(0, plan.value);
  uint8_t frame[16];
  TEST_ASSERT_EQUAL_UINT(13, wsoft_djb_build(1, &plan, frame, sizeof frame));
  TEST_ASSERT_EQUAL_UINT8(0, frame[7]);
  TEST_ASSERT_EQUAL_UINT8(0, frame[8]);
  TEST_ASSERT_EQUAL_UINT8(0, frame[9]);
  TEST_ASSERT_EQUAL_UINT8(0, frame[10]);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_lc1_zero_frame);
  RUN_TEST(test_lc1_zero_ack);
  RUN_TEST(test_channel_addresses);
  RUN_TEST(test_parameter_addresses);
  RUN_TEST(test_link_and_format_limits);
  RUN_TEST(test_weight_and_alarm_are_read_only);
  RUN_TEST(test_ad_range_and_missing_value);
  RUN_TEST(test_read_roundtrip_and_exception);
  RUN_TEST(test_alarm_bits_are_on_the_high_word);
  RUN_TEST(test_sampling_stays_a_raw_long);
  return UNITY_END();
}
