#include "wsoft_settings.h"

#include <string.h>

static void copy_text(char *dst, size_t capacity, const char *src) {
  strncpy(dst, src, capacity - 1);
  dst[capacity - 1] = '\0';
}

void wsoft_settings_defaults(WSoftSettingsData *s) {
  s->publish_interval_ms = WSOFT_PUBLISH_DEFAULT_MS;
  s->mqtt_enabled = true;
  copy_text(s->mqtt_base_path, sizeof s->mqtt_base_path, WSOFT_DEFAULT_MQTT_BASE_PATH);
  copy_text(s->mqtt_name, sizeof s->mqtt_name, WSOFT_DEFAULT_MQTT_NAME);
  copy_text(s->mqtt_unique_id, sizeof s->mqtt_unique_id, WSOFT_DEFAULT_MQTT_UNIQUE_ID);
  s->source = WSOFT_SOURCE_RS485;
  s->relay_enabled = false;
  s->relay_low = WSOFT_RELAY_DEFAULT_LOW;
  s->relay_ok = WSOFT_RELAY_DEFAULT_OK;
  s->relay_high = WSOFT_RELAY_DEFAULT_HIGH;
  s->relay_writer_follow = false;
}

const char *wsoft_source_text(WSoftSource source) {
  return source == WSOFT_SOURCE_RS232 ? "rs232" : "rs485";
}

bool wsoft_source_parse(const char *text, WSoftSource *source) {
  if (text == nullptr) return false;
  if (strcmp(text, "rs485") == 0) *source = WSOFT_SOURCE_RS485;
  else if (strcmp(text, "rs232") == 0) *source = WSOFT_SOURCE_RS232;
  else return false;
  return true;
}

static bool printable(char c) {
  return c >= 0x21 && c <= 0x7E;
}

bool wsoft_settings_valid_topic(const char *topic) {
  if (topic == nullptr) return false;
  size_t length = strlen(topic);
  if (length == 0 || length > WSOFT_MQTT_PATH_MAX) return false;
  if (topic[0] == '/' || topic[length - 1] == '/') return false;
  for (size_t i = 0; i < length; i++) {
    char c = topic[i];
    if (!printable(c) || c == '+') return false;
    if (c == '#' && topic[i + 1] != '{') return false;
  }
  return true;
}

bool wsoft_settings_valid_id(const char *id) {
  if (id == nullptr) return false;
  size_t length = strlen(id);
  if (length == 0 || length > WSOFT_MQTT_ID_MAX) return false;
  for (size_t i = 0; i < length; i++) {
    if (!printable(id[i])) return false;
  }
  return true;
}

bool wsoft_settings_valid_name(const char *name) {
  if (name == nullptr) return false;
  size_t length = strlen(name);
  if (length == 0 || length > WSOFT_MQTT_ID_MAX || name[0] == ' ' || name[length - 1] == ' ') return false;
  for (size_t i = 0; i < length; i++) {
    if (name[i] != ' ' && !printable(name[i])) return false;
  }
  return true;
}

static uint32_t clamp_interval(int64_t ms) {
  if (ms < (int64_t)WSOFT_PUBLISH_MIN_MS) return WSOFT_PUBLISH_MIN_MS;
  if (ms > (int64_t)WSOFT_PUBLISH_MAX_MS) return WSOFT_PUBLISH_MAX_MS;
  return (uint32_t)ms;
}

static bool relay_number(int64_t n, uint8_t *out) {
  if (n < 0 || n > (int64_t)WSOFT_RELAY_COUNT) return false;
  *out = (uint8_t)n;
  return true;
}

WSoftSettingsResult wsoft_settings_update(const WSoftSettingsInput &in, WSoftSettingsData *s) {
  WSoftSettingsData next;
  wsoft_settings_defaults(&next);
  if (in.has_publish_interval_ms) next.publish_interval_ms = clamp_interval(in.publish_interval_ms);
  if (in.has_mqtt_enabled) next.mqtt_enabled = in.mqtt_enabled;

  if (in.mqtt_base_path != nullptr) {
    if (!wsoft_settings_valid_topic(in.mqtt_base_path)) return WSOFT_SETTINGS_ERROR;
    copy_text(next.mqtt_base_path, sizeof next.mqtt_base_path, in.mqtt_base_path);
  }
  if (in.mqtt_name != nullptr) {
    if (!wsoft_settings_valid_name(in.mqtt_name)) return WSOFT_SETTINGS_ERROR;
    copy_text(next.mqtt_name, sizeof next.mqtt_name, in.mqtt_name);
  }
  if (in.mqtt_unique_id != nullptr) {
    if (!wsoft_settings_valid_id(in.mqtt_unique_id)) return WSOFT_SETTINGS_ERROR;
    copy_text(next.mqtt_unique_id, sizeof next.mqtt_unique_id, in.mqtt_unique_id);
  }
  if (in.source != nullptr && !wsoft_source_parse(in.source, &next.source)) return WSOFT_SETTINGS_ERROR;
  if (in.has_relay_enabled) next.relay_enabled = in.relay_enabled;
  if (in.has_relay_writer_follow) next.relay_writer_follow = in.relay_writer_follow;
  // A relay number has no safe value to clamp to: a wrong one could switch the wrong output.
  if (in.has_relay_low && !relay_number(in.relay_low, &next.relay_low)) return WSOFT_SETTINGS_ERROR;
  if (in.has_relay_ok && !relay_number(in.relay_ok, &next.relay_ok)) return WSOFT_SETTINGS_ERROR;
  if (in.has_relay_high && !relay_number(in.relay_high, &next.relay_high)) return WSOFT_SETTINGS_ERROR;
  if (!wsoft_relay_map_valid(next.relay_low, next.relay_ok, next.relay_high)) return WSOFT_SETTINGS_ERROR;

  bool changed = next.publish_interval_ms != s->publish_interval_ms || next.mqtt_enabled != s->mqtt_enabled ||
                 strcmp(next.mqtt_base_path, s->mqtt_base_path) != 0 || strcmp(next.mqtt_name, s->mqtt_name) != 0 ||
                 strcmp(next.mqtt_unique_id, s->mqtt_unique_id) != 0 || next.source != s->source ||
                 next.relay_enabled != s->relay_enabled || next.relay_low != s->relay_low ||
                 next.relay_ok != s->relay_ok || next.relay_high != s->relay_high ||
                 next.relay_writer_follow != s->relay_writer_follow;
  *s = next;
  return changed ? WSOFT_SETTINGS_CHANGED : WSOFT_SETTINGS_UNCHANGED;
}
