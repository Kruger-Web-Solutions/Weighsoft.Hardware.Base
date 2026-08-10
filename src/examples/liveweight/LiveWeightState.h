#ifndef LiveWeightState_h
#define LiveWeightState_h

#include <StatefulService.h>

#define LIVE_WEIGHT_DEFAULT_BAUD 9600
#define LIVE_WEIGHT_MIN_BAUD 300
#define LIVE_WEIGHT_MAX_BAUD 2000000
#define LIVE_WEIGHT_DEFAULT_REGEX "([+-]?[0-9]+[\\.,]?[0-9]*)"

enum LiveWeightSource : uint8_t {
  LIVE_WEIGHT_SOURCE_SERIAL = 0,
  LIVE_WEIGHT_SOURCE_WIFI = 1,
  LIVE_WEIGHT_SOURCE_RS485 = 2
};

// Runtime band for target range control
enum LiveWeightZone : uint8_t {
  LIVE_WEIGHT_ZONE_NONE = 0,
  LIVE_WEIGHT_ZONE_LOW = 1,
  LIVE_WEIGHT_ZONE_OK = 2,
  LIVE_WEIGHT_ZONE_HIGH = 3
};

class LiveWeightState {
 public:
  // Live reading
  String weight;
  String lastLine;
  unsigned long timestamp;
  String activeSource;
  String statusMessage;
  String unit;
  uint8_t zone;  // LiveWeightZone

  // Input config
  LiveWeightSource source;
  uint32_t baudrate;
  String regexPattern;
  bool rs485Enabled;
  uint8_t rs485Address;

  // Target range + relay map (persisted)
  bool rangeEnabled;
  float rangeLow;
  float rangeHigh;
  uint8_t relayLow;   // 1–4
  uint8_t relayOk;
  uint8_t relayHigh;

  // Optional product panel (persisted)
  String plu;
  String product;
  uint32_t count;
  String total;  // runtime display string (count * weight when possible)

  // DI action map + job / print (persisted except printRequested)
  // Actions: "none" | "print" | "next" | "start" | "stop"
  String di1Action;
  String di2Action;
  bool jobRunning;
  String lastAction;
  uint32_t actionSeq;
  bool printerEnabled;
  String printerIp;
  uint16_t printerPort;
  // Runtime-only: set by update() when trigger_action=print; service sends TCP then clears.
  bool printRequested;
  // Runtime-only: set by update() when trigger_action=next; service logs the weigh then clears.
  // A DI press logs directly from handleDiAction; REST has to defer, because file I/O must not
  // run inside the AsyncWebServer handler.
  bool nextRequested;

  static String normalizeAction(const String& action) {
    String v = action;
    v.trim();
    v.toLowerCase();
    if (v == "print" || v == "next" || v == "start" || v == "stop" || v == "none") {
      return v;
    }
    return "none";
  }

  static bool isValidAction(const String& action) {
    return action == "none" || action == "print" || action == "next" || action == "start" || action == "stop";
  }

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

  static const char* zoneToString(uint8_t z) {
    switch (z) {
      case LIVE_WEIGHT_ZONE_LOW:
        return "low";
      case LIVE_WEIGHT_ZONE_OK:
        return "ok";
      case LIVE_WEIGHT_ZONE_HIGH:
        return "high";
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

  static uint8_t clampRelay(int v, uint8_t fallback) {
    if (v >= 1 && v <= 4) {
      return (uint8_t)v;
    }
    return fallback;
  }

  static void refreshTotal(LiveWeightState& state) {
    if (state.weight.length() == 0 || state.count == 0) {
      state.total = state.weight;
      return;
    }
    float w = state.weight.toFloat();
    state.total = String(w * (float)state.count, 3);
  }

  static void read(LiveWeightState& state, JsonObject& root) {
    root["weight"] = state.weight;
    root["last_line"] = state.lastLine;
    root["timestamp"] = state.timestamp;
    root["active_source"] = state.activeSource;
    root["status_message"] = state.statusMessage;
    root["unit"] = state.unit;
    root["zone"] = state.zone;
    root["zone_name"] = zoneToString(state.zone);
    root["source"] = (int)state.source;
    root["source_name"] = sourceToString(state.source);
    root["baud_rate"] = state.baudrate;
    root["regex_pattern"] = state.regexPattern;
    root["rs485_enabled"] = state.rs485Enabled;
    root["rs485_address"] = state.rs485Address;
    root["rs485_ready"] = false;
    root["range_enabled"] = state.rangeEnabled;
    root["range_low"] = state.rangeLow;
    root["range_high"] = state.rangeHigh;
    root["relay_low"] = state.relayLow;
    root["relay_ok"] = state.relayOk;
    root["relay_high"] = state.relayHigh;
    root["plu"] = state.plu;
    root["product"] = state.product;
    root["count"] = state.count;
    root["total"] = state.total;
    root["di1_action"] = state.di1Action;
    root["di2_action"] = state.di2Action;
    root["job_running"] = state.jobRunning;
    root["last_action"] = state.lastAction;
    root["action_seq"] = state.actionSeq;
    root["printer_enabled"] = state.printerEnabled;
    root["printer_ip"] = state.printerIp;
    root["printer_port"] = state.printerPort;
  }

  static void readConfig(LiveWeightState& state, JsonObject& root) {
    root["source"] = (int)state.source;
    root["baud_rate"] = state.baudrate;
    root["regex_pattern"] = state.regexPattern;
    root["rs485_enabled"] = state.rs485Enabled;
    root["rs485_address"] = state.rs485Address;
    root["range_enabled"] = state.rangeEnabled;
    root["range_low"] = state.rangeLow;
    root["range_high"] = state.rangeHigh;
    root["relay_low"] = state.relayLow;
    root["relay_ok"] = state.relayOk;
    root["relay_high"] = state.relayHigh;
    root["plu"] = state.plu;
    root["product"] = state.product;
    root["count"] = state.count;
    root["unit"] = state.unit;
    root["di1_action"] = state.di1Action;
    root["di2_action"] = state.di2Action;
    root["printer_enabled"] = state.printerEnabled;
    root["printer_ip"] = state.printerIp;
    root["printer_port"] = state.printerPort;
    // job_running / last_action / action_seq are runtime-only (not persisted)
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
    state.rangeEnabled = root["range_enabled"] | false;
    state.rangeLow = root["range_low"] | 1.0f;
    state.rangeHigh = root["range_high"] | 2.0f;
    if (state.rangeHigh < state.rangeLow) {
      float t = state.rangeLow;
      state.rangeLow = state.rangeHigh;
      state.rangeHigh = t;
    }
    state.relayLow = clampRelay(root["relay_low"] | 1, 1);
    state.relayOk = clampRelay(root["relay_ok"] | 2, 2);
    state.relayHigh = clampRelay(root["relay_high"] | 3, 3);
    state.plu = root["plu"] | "";
    state.product = root["product"] | "";
    state.count = root["count"] | 1UL;
    state.unit = root["unit"] | "kg";
    state.di1Action = normalizeAction(root["di1_action"] | "none");
    state.di2Action = normalizeAction(root["di2_action"] | "none");
    state.printerEnabled = root["printer_enabled"] | false;
    state.printerIp = root["printer_ip"] | "";
    uint32_t port = root["printer_port"] | 9100U;
    if (port == 0 || port > 65535) {
      port = 9100;
    }
    state.printerPort = (uint16_t)port;
    refreshTotal(state);
    return StateUpdateResult::CHANGED;
  }

  // Public / operator endpoint: PLU, count, weight, DI actions only.
  // Config (baud, range, printer IP, DI map, etc.) → updateConfig + /rest/liveWeightConfig (auth).
  static StateUpdateResult update(JsonObject& root, LiveWeightState& state) {
    bool changed = false;

    if (root.containsKey("plu")) {
      String v = root["plu"].as<String>();
      if (v != state.plu) {
        state.plu = v;
        changed = true;
      }
    }
    if (root.containsKey("product")) {
      String v = root["product"].as<String>();
      if (v != state.product) {
        state.product = v;
        changed = true;
      }
    }
    if (root.containsKey("count")) {
      uint32_t v = root["count"];
      if (v != state.count) {
        state.count = v;
        changed = true;
      }
    }
    if (root.containsKey("unit")) {
      String v = root["unit"].as<String>();
      if (v != state.unit) {
        state.unit = v;
        changed = true;
      }
    }
    if (root.containsKey("job_running")) {
      bool v = root["job_running"];
      if (v != state.jobRunning) {
        state.jobRunning = v;
        changed = true;
      }
    }

    // REST testing: apply same state mutations as DI actions (no TCP here).
    // For print, sets printRequested; LiveWeightService updateHandler calls sendNetworkPrint().
    if (root.containsKey("trigger_action")) {
      String a = normalizeAction(root["trigger_action"].as<String>());
      if (a != "none") {
        state.lastAction = a;
        state.actionSeq++;
        if (a == "next") {
          state.count++;
          state.statusMessage = "Next piece";
          state.nextRequested = true;  // must log the weigh, same as a DI press
        } else if (a == "start") {
          state.jobRunning = true;
          state.statusMessage = "Job started";
        } else if (a == "stop") {
          state.jobRunning = false;
          state.statusMessage = "Job stopped";
        } else if (a == "print") {
          state.statusMessage = "Print requested";
          state.printRequested = true;
        }
        changed = true;
      }
    }

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

    if (changed) {
      refreshTotal(state);
    }

    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }
};

#endif
