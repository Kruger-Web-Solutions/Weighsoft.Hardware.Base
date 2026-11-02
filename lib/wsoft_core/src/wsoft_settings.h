#pragma once

// Options owned by the W-Soft web services (not by the W-Soft firmware itself).
// Pure C++ so the defaults and validation run in host tests; WSoftSettingsService
// maps JSON to WSoftSettingsInput and back.
//
// Deliberately NOT here: approval ON/OFF (USB technician only), the Hub pairing code
// (Hub tab, hub_pairing.h + WSoftHubService), Wi-Fi and OTA (framework settings pages).

#include <stddef.h>
#include <stdint.h>

static const uint32_t WSOFT_PUBLISH_MIN_MS = 250;  // the screen polls the DJB every 250 ms
static const uint32_t WSOFT_PUBLISH_MAX_MS = 60000;
static const uint32_t WSOFT_PUBLISH_DEFAULT_MS = 1000;

// Auto-save: a load held still this many seconds is saved once (save_logic.h).
static const uint32_t WSOFT_AUTO_SAVE_MIN_S = 1;
static const uint32_t WSOFT_AUTO_SAVE_MAX_S = 30;
static const uint32_t WSOFT_AUTO_SAVE_DEFAULT_S = 3;

static const size_t WSOFT_MQTT_PATH_MAX = 96;  // + "/status" stays under the framework's 128-byte topic cap
static const size_t WSOFT_MQTT_ID_MAX = 48;

// Defaults may hold #{unique_id} / #{platform}; the service expands them when it uses them.
static const char *const WSOFT_DEFAULT_MQTT_BASE_PATH = "weighsoft/wsoft/#{unique_id}";
static const char *const WSOFT_DEFAULT_MQTT_NAME = "wsoft-#{unique_id}";
static const char *const WSOFT_DEFAULT_MQTT_UNIQUE_ID = "wsoft-#{unique_id}";

struct WSoftSettingsData {
  uint32_t publish_interval_ms;
  bool mqtt_enabled;
  char mqtt_base_path[WSOFT_MQTT_PATH_MAX + 1];
  char mqtt_name[WSOFT_MQTT_ID_MAX + 1];
  char mqtt_unique_id[WSOFT_MQTT_ID_MAX + 1];
  bool auto_save;              // default off
  uint32_t auto_save_stable_s;  // 1..30, default 3
  // Bench testing only: let a save through while the DJB reports an alarm. Ignored while
  // approval is ON (a trade weight is never taken from a box in alarm). Default off.
  bool save_ignore_alarm;
};

// One field per option. A missing field (has_* false or a null string) takes its default:
// the update is a full replace, like the framework's own settings services.
struct WSoftSettingsInput {
  bool has_publish_interval_ms = false;
  int64_t publish_interval_ms = 0;
  bool has_mqtt_enabled = false;
  bool mqtt_enabled = false;
  const char *mqtt_base_path = nullptr;
  const char *mqtt_name = nullptr;
  const char *mqtt_unique_id = nullptr;
  bool has_auto_save = false;
  bool auto_save = false;
  bool has_auto_save_stable_s = false;
  int64_t auto_save_stable_s = 0;
  bool has_save_ignore_alarm = false;
  bool save_ignore_alarm = false;
};

enum WSoftSettingsResult { WSOFT_SETTINGS_UNCHANGED, WSOFT_SETTINGS_CHANGED, WSOFT_SETTINGS_ERROR };

void wsoft_settings_defaults(WSoftSettingsData *s);

// Validates and applies. On ERROR nothing in *s changes. The publish interval is
// clamped into [MIN, MAX] (the branch's style for numbers), and so is the auto-save time; a bad topic or name is
// an ERROR because there is no safe value to clamp it to.
WSoftSettingsResult wsoft_settings_update(const WSoftSettingsInput &in, WSoftSettingsData *s);

// MQTT base path: 1..96 chars, printable ASCII, no spaces, no '+', and '#' only as the
// start of a #{...} placeholder; must not start or end with '/'.
bool wsoft_settings_valid_topic(const char *topic);
// Unique id: 1..48 printable ASCII chars, no spaces.
bool wsoft_settings_valid_id(const char *id);
// Display name: 1..48 printable ASCII chars; inner spaces allowed, no leading/trailing space.
bool wsoft_settings_valid_name(const char *name);
