#ifndef WeightForwarderState_h
#define WeightForwarderState_h

#include <StatefulService.h>

enum ForwardProtocol { PROTOCOL_HTTP = 0, PROTOCOL_WS = 1, PROTOCOL_MQTT = 2, PROTOCOL_BLE = 3 };

class WeightForwarderState {
 public:
  // Configuration fields (persisted to flash)
  ForwardProtocol protocol;
  String targetUrl;       // HTTP: "http://192.168.1.50:8080/weight"
  String wsUrl;           // WS: "ws://192.168.1.50:8080/ws"
  String mqttTopic;       // MQTT: "remote/device/weight"
  String bleServiceUuid;  // BLE: "12340000-e8f2-537e-4f6c-d104768a1234"
  String bleCharUuid;     // BLE: "12340001-e8f2-537e-4f6c-d104768a1234"
  bool enabled;
  bool displayMode;  // true = {"line1","line2"}, false = {"last_line","weight","timestamp"}
  String authUsername;  // Optional: for HTTP POST to protected endpoints (e.g. /rest/display)
  String authPassword;

  // Runtime status (not persisted)
  bool connected;
  String lastError;
  unsigned long lastForwardTime;

  // Full read (includes runtime status for REST/WebSocket)
  static void read(WeightForwarderState& state, JsonObject& root) {
    root["protocol"] = (int)state.protocol;
    root["target_url"] = state.targetUrl;
    root["ws_url"] = state.wsUrl;
    root["mqtt_topic"] = state.mqttTopic;
    root["ble_service_uuid"] = state.bleServiceUuid;
    root["ble_char_uuid"] = state.bleCharUuid;
    root["enabled"] = state.enabled;
    root["display_mode"] = state.displayMode;
    root["auth_username"] = state.authUsername;
    root["auth_password"] = state.authPassword;

    // Runtime status
    root["connected"] = state.connected;
    root["last_error"] = state.lastError;
    root["last_forward_time"] = state.lastForwardTime;
  }

  // Config-only read for FSPersistence (persists to flash)
  static void readConfig(WeightForwarderState& state, JsonObject& root) {
    root["protocol"] = (int)state.protocol;
    root["target_url"] = state.targetUrl;
    root["ws_url"] = state.wsUrl;
    root["mqtt_topic"] = state.mqttTopic;
    root["ble_service_uuid"] = state.bleServiceUuid;
    root["ble_char_uuid"] = state.bleCharUuid;
    root["enabled"] = state.enabled;
    root["display_mode"] = state.displayMode;
    root["auth_username"] = state.authUsername;
    root["auth_password"] = state.authPassword;
  }

  // Config update for FSPersistence (loads from flash)
  static StateUpdateResult updateConfig(JsonObject& root, WeightForwarderState& state) {
    state.protocol = (ForwardProtocol)(root["protocol"] | (int)PROTOCOL_HTTP);
    state.targetUrl = root["target_url"] | "";
    state.wsUrl = root["ws_url"] | "";
    state.mqttTopic = root["mqtt_topic"] | "";
    state.bleServiceUuid = root["ble_service_uuid"] | "";
    state.bleCharUuid = root["ble_char_uuid"] | "";
    state.enabled = root["enabled"] | false;
    state.displayMode = root["display_mode"] | false;
    state.authUsername = root["auth_username"] | "";
    state.authPassword = root["auth_password"] | "";

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

    if (root.containsKey("target_url")) {
      String newUrl = root["target_url"].as<String>();
      if (newUrl != state.targetUrl) {
        state.targetUrl = newUrl;
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

    if (root.containsKey("display_mode")) {
      bool newMode = root["display_mode"];
      if (newMode != state.displayMode) {
        state.displayMode = newMode;
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

    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }
};

#endif
