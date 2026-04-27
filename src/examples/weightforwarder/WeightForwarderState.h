#ifndef WeightForwarderState_h
#define WeightForwarderState_h

#include <StatefulService.h>

enum ForwardProtocol { PROTOCOL_HTTP = 0, PROTOCOL_WS = 1, PROTOCOL_MQTT = 2, PROTOCOL_BLE = 3 };
enum OutputFormat { FMT_STANDARD = 0, FMT_LCD = 1, FMT_TFT = 2, FMT_SERIAL = 3 };

static constexpr int MAX_TARGET_URLS = 5;

// Heartbeat: 0 = disabled (only send on change), otherwise clamped to [HEARTBEAT_MIN_MS, HEARTBEAT_MAX_MS].
// Hard floor of MIN_FORWARD_INTERVAL still applies in the service loop, so a tiny heartbeat
// value can never flood the receiver.
static constexpr uint32_t HEARTBEAT_DEFAULT_MS = 10000;   // 10 seconds
static constexpr uint32_t HEARTBEAT_MIN_MS     = 1000;    // 1 second
static constexpr uint32_t HEARTBEAT_MAX_MS     = 600000;  // 10 minutes

class WeightForwarderState {
 public:
  // Configuration fields (persisted to flash)
  ForwardProtocol protocol;
  String targetUrls[MAX_TARGET_URLS];
  OutputFormat targetFormats[MAX_TARGET_URLS];
  int targetUrlCount;
  String wsUrl;
  String mqttTopic;
  String bleServiceUuid;
  String bleCharUuid;
  bool enabled;
  String authUsername;
  String authPassword;
  uint32_t heartbeatIntervalMs = HEARTBEAT_DEFAULT_MS;  // 0 = disabled, otherwise clamped to [HEARTBEAT_MIN_MS, HEARTBEAT_MAX_MS]
  bool usbEchoEnabled = false;  // when true, every captured scale line is echoed to Serial (USB CDC COM port)

  // Runtime status (not persisted)
  bool connected;
  String lastError;
  unsigned long lastForwardTime;

  static void read(WeightForwarderState& state, JsonObject& root) {
    root["protocol"] = (int)state.protocol;
    JsonArray urls = root.createNestedArray("target_urls");
    JsonArray fmts = root.createNestedArray("target_formats");
    for (int i = 0; i < state.targetUrlCount; i++) {
      urls.add(state.targetUrls[i]);
      fmts.add((int)state.targetFormats[i]);
    }
    root["target_url"] = state.targetUrlCount > 0 ? state.targetUrls[0] : "";
    root["ws_url"] = state.wsUrl;
    root["mqtt_topic"] = state.mqttTopic;
    root["ble_service_uuid"] = state.bleServiceUuid;
    root["ble_char_uuid"] = state.bleCharUuid;
    root["enabled"] = state.enabled;
    root["auth_username"] = state.authUsername;
    root["auth_password"] = state.authPassword;
    root["heartbeat_interval_sec"] = state.heartbeatIntervalMs / 1000;
    root["usb_echo_enabled"] = state.usbEchoEnabled;
    root["connected"] = state.connected;
    root["last_error"] = state.lastError;
    root["last_forward_time"] = state.lastForwardTime;
  }

  static void readConfig(WeightForwarderState& state, JsonObject& root) {
    root["protocol"] = (int)state.protocol;
    JsonArray urls = root.createNestedArray("target_urls");
    JsonArray fmts = root.createNestedArray("target_formats");
    for (int i = 0; i < state.targetUrlCount; i++) {
      urls.add(state.targetUrls[i]);
      fmts.add((int)state.targetFormats[i]);
    }
    root["ws_url"] = state.wsUrl;
    root["mqtt_topic"] = state.mqttTopic;
    root["ble_service_uuid"] = state.bleServiceUuid;
    root["ble_char_uuid"] = state.bleCharUuid;
    root["enabled"] = state.enabled;
    root["auth_username"] = state.authUsername;
    root["auth_password"] = state.authPassword;
    root["heartbeat_interval_sec"] = state.heartbeatIntervalMs / 1000;
    root["usb_echo_enabled"] = state.usbEchoEnabled;
  }

  // Clamp helper: 0 stays 0 (disabled), otherwise enforce [HEARTBEAT_MIN_MS, HEARTBEAT_MAX_MS].
  static uint32_t clampHeartbeatMs(uint32_t requestedMs) {
    if (requestedMs == 0) return 0;
    if (requestedMs < HEARTBEAT_MIN_MS) return HEARTBEAT_MIN_MS;
    if (requestedMs > HEARTBEAT_MAX_MS) return HEARTBEAT_MAX_MS;
    return requestedMs;
  }

  static StateUpdateResult updateConfig(JsonObject& root, WeightForwarderState& state) {
    state.protocol = (ForwardProtocol)(root["protocol"] | (int)PROTOCOL_HTTP);
    state.targetUrlCount = 0;
    if (root.containsKey("target_urls") && root["target_urls"].is<JsonArray>()) {
      JsonArray arr = root["target_urls"].as<JsonArray>();
      JsonArray fmtArr;
      bool hasFmts = root.containsKey("target_formats") && root["target_formats"].is<JsonArray>();
      if (hasFmts) fmtArr = root["target_formats"].as<JsonArray>();
      int idx = 0;
      for (JsonVariant v : arr) {
        if (state.targetUrlCount >= MAX_TARGET_URLS) break;
        String url = v.as<String>();
        state.targetUrls[state.targetUrlCount] = url;
        state.targetFormats[state.targetUrlCount] = (hasFmts && idx < (int)fmtArr.size())
            ? (OutputFormat)(fmtArr[idx].as<int>()) : FMT_STANDARD;
        state.targetUrlCount++;
        idx++;
      }
    } else if (root.containsKey("target_url")) {
      String url = root["target_url"] | "";
      if (!url.isEmpty()) {
        state.targetUrls[0] = url;
        state.targetFormats[0] = FMT_STANDARD;
        state.targetUrlCount = 1;
      }
    }
    state.wsUrl = root["ws_url"] | "";
    state.mqttTopic = root["mqtt_topic"] | "";
    state.bleServiceUuid = root["ble_service_uuid"] | "";
    state.bleCharUuid = root["ble_char_uuid"] | "";
    state.enabled = root["enabled"] | false;
    state.authUsername = root["auth_username"] | "";
    state.authPassword = root["auth_password"] | "";
    {
      uint32_t requestedMs = (uint32_t)((root["heartbeat_interval_sec"] | (long)(HEARTBEAT_DEFAULT_MS / 1000)) * 1000UL);
      state.heartbeatIntervalMs = clampHeartbeatMs(requestedMs);
    }
    state.usbEchoEnabled = root["usb_echo_enabled"] | false;

    // Validate protocol against feature flags
#if !FT_ENABLED(FT_MQTT)
    if (state.protocol == PROTOCOL_MQTT)
      return StateUpdateResult::ERROR;
#endif
#if !FT_ENABLED(FT_BLE)
    if (state.protocol == PROTOCOL_BLE)
      return StateUpdateResult::ERROR;
#endif

    return StateUpdateResult::CHANGED;
  }

  // Full update (used by REST/WebSocket API)
  static StateUpdateResult update(JsonObject& root, WeightForwarderState& state) {
    bool changed = false;

    if (root.containsKey("protocol")) {
      ForwardProtocol newProto = (ForwardProtocol)(int)root["protocol"];
      if (newProto != state.protocol) {
        state.protocol = newProto;
        changed = true;
      }
    }

    if (root.containsKey("target_urls") && root["target_urls"].is<JsonArray>()) {
      JsonArray arr = root["target_urls"].as<JsonArray>();
      JsonArray fmtArr;
      bool hasFmts = root.containsKey("target_formats") && root["target_formats"].is<JsonArray>();
      if (hasFmts) fmtArr = root["target_formats"].as<JsonArray>();
      int newCount = 0;
      String newUrls[MAX_TARGET_URLS];
      OutputFormat newFmts[MAX_TARGET_URLS];
      int idx = 0;
      for (JsonVariant v : arr) {
        if (newCount >= MAX_TARGET_URLS) break;
        newUrls[newCount] = v.as<String>();
        newFmts[newCount] = (hasFmts && idx < (int)fmtArr.size())
            ? (OutputFormat)(fmtArr[idx].as<int>()) : FMT_STANDARD;
        newCount++;
        idx++;
      }
      bool urlsChanged = (newCount != state.targetUrlCount);
      if (!urlsChanged) {
        for (int i = 0; i < newCount; i++) {
          if (newUrls[i] != state.targetUrls[i] || newFmts[i] != state.targetFormats[i]) {
            urlsChanged = true; break;
          }
        }
      }
      if (urlsChanged) {
        state.targetUrlCount = newCount;
        for (int i = 0; i < newCount; i++) {
          state.targetUrls[i] = newUrls[i];
          state.targetFormats[i] = newFmts[i];
        }
        changed = true;
      }
    } else if (root.containsKey("target_url")) {
      String newUrl = root["target_url"].as<String>();
      if (state.targetUrlCount != 1 || state.targetUrls[0] != newUrl) {
        state.targetUrls[0] = newUrl;
        state.targetFormats[0] = FMT_STANDARD;
        state.targetUrlCount = newUrl.isEmpty() ? 0 : 1;
        changed = true;
      }
    }

    if (root.containsKey("ws_url")) {
      String newUrl = root["ws_url"].as<String>();
      if (newUrl != state.wsUrl) {
        state.wsUrl = newUrl;
        changed = true;
      }
    }

    if (root.containsKey("mqtt_topic")) {
      String newTopic = root["mqtt_topic"].as<String>();
      if (newTopic != state.mqttTopic) {
        state.mqttTopic = newTopic;
        changed = true;
      }
    }

    if (root.containsKey("ble_service_uuid")) {
      String newUuid = root["ble_service_uuid"].as<String>();
      if (newUuid != state.bleServiceUuid) {
        state.bleServiceUuid = newUuid;
        changed = true;
      }
    }

    if (root.containsKey("ble_char_uuid")) {
      String newUuid = root["ble_char_uuid"].as<String>();
      if (newUuid != state.bleCharUuid) {
        state.bleCharUuid = newUuid;
        changed = true;
      }
    }

    if (root.containsKey("enabled")) {
      bool newEnabled = root["enabled"];
      if (newEnabled != state.enabled) {
        state.enabled = newEnabled;
        changed = true;
      }
    }

    if (root.containsKey("auth_username")) {
      String v = root["auth_username"].as<String>();
      if (v != state.authUsername) {
        state.authUsername = v;
        changed = true;
      }
    }
    if (root.containsKey("auth_password")) {
      String v = root["auth_password"].as<String>();
      if (v != state.authPassword) {
        state.authPassword = v;
        changed = true;
      }
    }

    if (root.containsKey("heartbeat_interval_sec")) {
      uint32_t requestedMs = (uint32_t)((long)root["heartbeat_interval_sec"] * 1000UL);
      uint32_t clamped = clampHeartbeatMs(requestedMs);
      if (clamped != state.heartbeatIntervalMs) {
        state.heartbeatIntervalMs = clamped;
        changed = true;
      }
    }

    if (root.containsKey("usb_echo_enabled")) {
      bool v = root["usb_echo_enabled"];
      if (v != state.usbEchoEnabled) {
        state.usbEchoEnabled = v;
        changed = true;
      }
    }

    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }
};

#endif
