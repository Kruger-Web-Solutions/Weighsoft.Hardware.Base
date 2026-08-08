#ifndef LiveWeightState_h
#define LiveWeightState_h

#include <StatefulService.h>

#define LIVE_WEIGHT_DEFAULT_BAUD 9600
#define LIVE_WEIGHT_MIN_BAUD 300
#define LIVE_WEIGHT_MAX_BAUD 2000000
#define LIVE_WEIGHT_DEFAULT_REGEX "([+-]?[0-9]+[\\.,]?[0-9]*)"

// How the board currently expects weight to arrive
enum LiveWeightSource : uint8_t {
  LIVE_WEIGHT_SOURCE_SERIAL = 0,  // RS-232 via UART0 / MAX3232 header
  LIVE_WEIGHT_SOURCE_WIFI = 1,    // REST / WebSocket / MQTT over WiFi
  LIVE_WEIGHT_SOURCE_RS485 = 2    // Stub — needs RS-485 transceiver hardware
};

class LiveWeightState {
 public:
  // Live reading
  String weight;
  String lastLine;
  unsigned long timestamp;
  String activeSource;  // "serial" | "wifi" | "rs485" | "none"
  String statusMessage;

  // Config (persisted)
  LiveWeightSource source;
  uint32_t baudrate;
  String regexPattern;
  // RS-485 stub fields (persisted for UI, not used yet)
  bool rs485Enabled;
  uint8_t rs485Address;

  static const char* sourceToString(LiveWeightSource s) {
    switch (s) {
      case LIVE_WEIGHT_SOURCE_SERIAL:
        return "serial";
      case LIVE_WEIGHT_SOURCE_WIFI:
        return "wifi";
      case LIVE_WEIGHT_SOURCE_RS485:
        return "rs485";
      default:
        return "none";
    }
  }

  static LiveWeightSource sourceFromValue(int v) {
    if (v == (int)LIVE_WEIGHT_SOURCE_SERIAL) {
      return LIVE_WEIGHT_SOURCE_SERIAL;
    }
    if (v == (int)LIVE_WEIGHT_SOURCE_RS485) {
      return LIVE_WEIGHT_SOURCE_RS485;
    }
    return LIVE_WEIGHT_SOURCE_WIFI;
  }

  static void read(LiveWeightState& state, JsonObject& root) {
    root["weight"] = state.weight;
    root["last_line"] = state.lastLine;
    root["timestamp"] = state.timestamp;
    root["active_source"] = state.activeSource;
    root["status_message"] = state.statusMessage;
    root["source"] = (int)state.source;
    root["source_name"] = sourceToString(state.source);
    root["baud_rate"] = state.baudrate;
    root["regex_pattern"] = state.regexPattern;
    root["rs485_enabled"] = state.rs485Enabled;
    root["rs485_address"] = state.rs485Address;
    root["rs485_ready"] = false;
  }

  static void readConfig(LiveWeightState& state, JsonObject& root) {
    root["source"] = (int)state.source;
    root["baud_rate"] = state.baudrate;
    root["regex_pattern"] = state.regexPattern;
    root["rs485_enabled"] = state.rs485Enabled;
    root["rs485_address"] = state.rs485Address;
  }

  static StateUpdateResult updateConfig(JsonObject& root, LiveWeightState& state) {
    state.source = sourceFromValue(root["source"] | (int)LIVE_WEIGHT_SOURCE_WIFI);
    uint32_t baud = root["baud_rate"] | LIVE_WEIGHT_DEFAULT_BAUD;
    if (baud < LIVE_WEIGHT_MIN_BAUD || baud > LIVE_WEIGHT_MAX_BAUD) {
      baud = LIVE_WEIGHT_DEFAULT_BAUD;
    }
    state.baudrate = baud;
    state.regexPattern = root["regex_pattern"] | LIVE_WEIGHT_DEFAULT_REGEX;
    state.rs485Enabled = root["rs485_enabled"] | false;
    state.rs485Address = root["rs485_address"] | (uint8_t)1;
    return StateUpdateResult::CHANGED;
  }

  static StateUpdateResult update(JsonObject& root, LiveWeightState& state) {
    bool changed = false;

    if (root.containsKey("source")) {
      LiveWeightSource s = sourceFromValue((int)root["source"]);
      if (s != state.source) {
        state.source = s;
        changed = true;
      }
    }

    if (root.containsKey("baud_rate")) {
      uint32_t v = root["baud_rate"];
      if (v >= LIVE_WEIGHT_MIN_BAUD && v <= LIVE_WEIGHT_MAX_BAUD && v != state.baudrate) {
        state.baudrate = v;
        changed = true;
      }
    }

    if (root.containsKey("regex_pattern")) {
      String v = root["regex_pattern"].as<String>();
      if (v != state.regexPattern) {
        state.regexPattern = v;
        changed = true;
      }
    }

    if (root.containsKey("rs485_enabled")) {
      bool v = root["rs485_enabled"];
      if (v != state.rs485Enabled) {
        state.rs485Enabled = v;
        changed = true;
      }
    }

    if (root.containsKey("rs485_address")) {
      uint8_t v = root["rs485_address"];
      if (v != state.rs485Address) {
        state.rs485Address = v;
        changed = true;
      }
    }

    // WiFi / WebSocket ingress — same shape as RemoteWeightService
    if (root.containsKey("weight")) {
      String v = root["weight"].as<String>();
      if (v != state.weight) {
        state.weight = v;
        changed = true;
      }
    }
    if (root.containsKey("last_line")) {
      String v = root["last_line"].as<String>();
      if (v != state.lastLine) {
        state.lastLine = v;
        changed = true;
      }
    }

    if (changed && (root.containsKey("weight") || root.containsKey("last_line"))) {
      state.timestamp = millis();
      state.activeSource = "wifi";
      state.statusMessage = "Weight received over WiFi / WebSocket";
    }

    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }
};

#endif
