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
  s->auto_save = false;
  s->auto_save_stable_s = WSOFT_AUTO_SAVE_DEFAULT_S;
  s->save_ignore_alarm = false;
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

static uint32_t clamp_auto_save_s(int64_t seconds) {
  if (seconds < (int64_t)WSOFT_AUTO_SAVE_MIN_S) return WSOFT_AUTO_SAVE_MIN_S;
  if (seconds > (int64_t)WSOFT_AUTO_SAVE_MAX_S) return WSOFT_AUTO_SAVE_MAX_S;
  return (uint32_t)seconds;
}

WSoftSettingsResult wsoft_settings_update(const WSoftSettingsInput &in, WSoftSettingsData *s) {
  WSoftSettingsData next;
  wsoft_settings_defaults(&next);
  if (in.has_publish_interval_ms) next.publish_interval_ms = clamp_interval(in.publish_interval_ms);
  if (in.has_mqtt_enabled) next.mqtt_enabled = in.mqtt_enabled;
  if (in.has_auto_save) next.auto_save = in.auto_save;
  if (in.has_auto_save_stable_s) next.auto_save_stable_s = clamp_auto_save_s(in.auto_save_stable_s);
  if (in.has_save_ignore_alarm) next.save_ignore_alarm = in.save_ignore_alarm;

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

  bool changed = next.publish_interval_ms != s->publish_interval_ms || next.mqtt_enabled != s->mqtt_enabled ||
                 strcmp(next.mqtt_base_path, s->mqtt_base_path) != 0 || strcmp(next.mqtt_name, s->mqtt_name) != 0 ||
                 strcmp(next.mqtt_unique_id, s->mqtt_unique_id) != 0 || next.auto_save != s->auto_save ||
                 next.auto_save_stable_s != s->auto_save_stable_s || next.save_ignore_alarm != s->save_ignore_alarm;
  *s = next;
  return changed ? WSOFT_SETTINGS_CHANGED : WSOFT_SETTINGS_UNCHANGED;
}
