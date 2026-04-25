#include <Arduino.h>
#include <unity.h>
#include <ArduinoJson.h>
#include <examples/serialwriter/SerialWriterForwarderJsonMapping.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

void test_flat_top_level_last_line(void) {
  const char* json = "{\"last_line\":\"hello\"}";
  char line[256];
  char path[32];
  SerialWriterForwarderJsonOutcome o = serialWriterForwarderExtractLineFromJsonBytes(
      "last_line", reinterpret_cast<const uint8_t*>(json), strlen(json), line, sizeof(line), path, sizeof(path));
  TEST_ASSERT_EQUAL_INT((int)SerialWriterForwarderJsonOutcome::Ok, (int)o);
  TEST_ASSERT_EQUAL_STRING("hello", line);
  TEST_ASSERT_EQUAL_STRING("top-level", path);
}

void test_wrapped_payload_last_line(void) {
  const char* json = "{\"type\":\"payload\",\"origin_id\":\"x\",\"payload\":{\"last_line\":\"  ST,GS  \"}}";
  char line[256];
  char path[32];
  SerialWriterForwarderJsonOutcome o = serialWriterForwarderExtractLineFromJsonBytes(
      "last_line", reinterpret_cast<const uint8_t*>(json), strlen(json), line, sizeof(line), path, sizeof(path));
  TEST_ASSERT_EQUAL_INT((int)SerialWriterForwarderJsonOutcome::Ok, (int)o);
  TEST_ASSERT_EQUAL_STRING("ST,GS", line);
  TEST_ASSERT_EQUAL_STRING("payload", path);
}

void test_field_missing(void) {
  const char* json = "{\"type\":\"payload\",\"payload\":{\"other\":\"x\"}}";
  char line[256];
  char path[32];
  SerialWriterForwarderJsonOutcome o = serialWriterForwarderExtractLineFromJsonBytes(
      "last_line", reinterpret_cast<const uint8_t*>(json), strlen(json), line, sizeof(line), path, sizeof(path));
  TEST_ASSERT_EQUAL_INT((int)SerialWriterForwarderJsonOutcome::FieldMissing, (int)o);
}

void test_field_empty_string(void) {
  const char* json = "{\"last_line\":\"\"}";
  char line[256];
  char path[32];
  SerialWriterForwarderJsonOutcome o = serialWriterForwarderExtractLineFromJsonBytes(
      "last_line", reinterpret_cast<const uint8_t*>(json), strlen(json), line, sizeof(line), path, sizeof(path));
  TEST_ASSERT_EQUAL_INT((int)SerialWriterForwarderJsonOutcome::FieldEmpty, (int)o);
}

void test_field_whitespace_only(void) {
  const char* json = "{\"last_line\":\"   \\t  \"}";
  char line[256];
  char path[32];
  SerialWriterForwarderJsonOutcome o = serialWriterForwarderExtractLineFromJsonBytes(
      "last_line", reinterpret_cast<const uint8_t*>(json), strlen(json), line, sizeof(line), path, sizeof(path));
  TEST_ASSERT_EQUAL_INT((int)SerialWriterForwarderJsonOutcome::FieldEmpty, (int)o);
}

void test_invalid_json(void) {
  const char* json = "{not json";
  char line[256];
  char path[32];
  SerialWriterForwarderJsonOutcome o = serialWriterForwarderExtractLineFromJsonBytes(
      "last_line", reinterpret_cast<const uint8_t*>(json), strlen(json), line, sizeof(line), path, sizeof(path));
  TEST_ASSERT_EQUAL_INT((int)SerialWriterForwarderJsonOutcome::ParseError, (int)o);
}

void test_document_path_top_level(void) {
  DynamicJsonDocument doc(256);
  DeserializationError e = deserializeJson(doc, "{\"last_line\":\"abc\"}");
  TEST_ASSERT(e == DeserializationError::Ok);
  char line[256];
  char path[32];
  SerialWriterForwarderJsonOutcome o =
      serialWriterForwarderExtractLineFromDocument("last_line", doc, line, sizeof(line), path, sizeof(path));
  TEST_ASSERT_EQUAL_INT((int)SerialWriterForwarderJsonOutcome::Ok, (int)o);
  TEST_ASSERT_EQUAL_STRING("abc", line);
}

void setup() {
  delay(500);
  UNITY_BEGIN();
  RUN_TEST(test_flat_top_level_last_line);
  RUN_TEST(test_wrapped_payload_last_line);
  RUN_TEST(test_field_missing);
  RUN_TEST(test_field_empty_string);
  RUN_TEST(test_field_whitespace_only);
  RUN_TEST(test_invalid_json);
  RUN_TEST(test_document_path_top_level);
  UNITY_END();
}

void loop() {
  delay(10000);
}
