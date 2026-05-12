#ifndef SerialWriterForwarderState_h
#define SerialWriterForwarderState_h

#include <StatefulService.h>

#define SERIALW_FWD_METHOD_WS    0
#define SERIALW_FWD_METHOD_HTTP  1

class SerialWriterForwarderState {
 public:
  // Persisted
  String  sourceUrl;          // e.g. "ws://192.168.2.50/ws/serial" or "http://192.168.2.50/rest/serial"
  uint8_t connectionMethod;   // 0=WS (default), 1=HTTP polling
  bool    autoReconnect;      // default true
  bool    enabled;            // default false until user sets a source

  // Runtime (not persisted)
  bool          connected = false;
  unsigned long lastReceivedAt = 0;
  String        lastReceived;
  String        lastWeight;
  uint16_t      reconnectAttempts = 0;
  String        lastError;

  SerialWriterForwarderState()
      : sourceUrl(""),
        connectionMethod(SERIALW_FWD_METHOD_WS),
        autoReconnect(true),
        enabled(false) {}

  static void read(SerialWriterForwarderState& state, JsonObject& root) {
    root["source_url"]         = state.sourceUrl;
    root["connection_method"]  = state.connectionMethod;
    root["auto_reconnect"]     = state.autoReconnect;
    root["enabled"]            = state.enabled;
    root["connected"]          = state.connected;
    root["last_received_at"]   = state.lastReceivedAt;
    root["last_received"]      = state.lastReceived;
    root["last_weight"]        = state.lastWeight;
    root["reconnect_attempts"] = state.reconnectAttempts;
    root["last_error"]         = state.lastError;
  }

  static void readConfig(SerialWriterForwarderState& state, JsonObject& root) {
    root["source_url"]         = state.sourceUrl;
    root["connection_method"]  = state.connectionMethod;
    root["auto_reconnect"]     = state.autoReconnect;
    root["enabled"]            = state.enabled;
  }

  static StateUpdateResult updateConfig(JsonObject& root, SerialWriterForwarderState& state) {
    state.sourceUrl        = root["source_url"]        | "";
    state.connectionMethod = root["connection_method"] | (uint8_t)SERIALW_FWD_METHOD_WS;
    state.autoReconnect    = root["auto_reconnect"]    | true;
    state.enabled          = root["enabled"]           | false;
    if (state.connectionMethod > SERIALW_FWD_METHOD_HTTP) state.connectionMethod = SERIALW_FWD_METHOD_WS;
    return StateUpdateResult::CHANGED;
  }

  static StateUpdateResult update(JsonObject& root, SerialWriterForwarderState& state) {
    StateUpdateResult result = StateUpdateResult::UNCHANGED;
    if (root.containsKey("source_url")) {
      String v = root["source_url"].as<String>();
      if (v != state.sourceUrl) { state.sourceUrl = v; result = StateUpdateResult::CHANGED; }
    }
    if (root.containsKey("connection_method")) {
      uint8_t v = root["connection_method"];
      if (v <= SERIALW_FWD_METHOD_HTTP && v != state.connectionMethod) { state.connectionMethod = v; result = StateUpdateResult::CHANGED; }
    }
    if (root.containsKey("auto_reconnect")) {
      bool v = root["auto_reconnect"];
      if (v != state.autoReconnect) { state.autoReconnect = v; result = StateUpdateResult::CHANGED; }
    }
    if (root.containsKey("enabled")) {
      bool v = root["enabled"];
      if (v != state.enabled) { state.enabled = v; result = StateUpdateResult::CHANGED; }
    }
    return result;
  }
};

#endif
