#ifndef SerialWriterForwarderState_h
#define SerialWriterForwarderState_h

#include <StatefulService.h>

// Source protocol for the forwarder
#define SERIAL_WRITER_FORWARDER_PROTOCOL_HTTP 0
#define SERIAL_WRITER_FORWARDER_PROTOCOL_WS 1

// Where forwarder-delivered lines are emitted (persisted as JSON string: usb_only / serial1_only / both)
#define SERIAL_WRITER_FORWARDER_OUTPUT_USB_ONLY 0
#define SERIAL_WRITER_FORWARDER_OUTPUT_SERIAL1_ONLY 1
#define SERIAL_WRITER_FORWARDER_OUTPUT_BOTH 2

class SerialWriterForwarderState {
 public:
  // Config fields (persisted)
  bool enabled;
  uint8_t protocol;          // 0=HTTP poll, 1=WS subscribe
  String sourceUrl;          // "http://..." or "ws://..."
  String jsonField;          // JSON field to extract as the line (default: "last_line")
  uint32_t pollIntervalMs;   // HTTP polling interval in ms (default 1000)
  uint8_t outputTargets;     // SERIAL_WRITER_FORWARDER_OUTPUT_*
  String authUsername;
  String authPassword;

  // Runtime status (not persisted)
  bool connected;
  String lastReceivedLine;
  unsigned long lastReceivedMs;
  uint32_t receivedCount;
  String lastError;

  SerialWriterForwarderState()
      : enabled(false),
        protocol(SERIAL_WRITER_FORWARDER_PROTOCOL_HTTP),
        jsonField("last_line"),
        pollIntervalMs(1000),
        outputTargets(SERIAL_WRITER_FORWARDER_OUTPUT_USB_ONLY),
        connected(false),
        lastReceivedMs(0),
        receivedCount(0) {}

  // Full read — config + runtime status (REST/WebSocket)
  static void read(SerialWriterForwarderState& state, JsonObject& root) {
    root["enabled"] = state.enabled;
    root["protocol"] = state.protocol;
    root["source_url"] = state.sourceUrl;
    root["json_field"] = state.jsonField;
    root["poll_interval_ms"] = state.pollIntervalMs;
    if (state.outputTargets == SERIAL_WRITER_FORWARDER_OUTPUT_SERIAL1_ONLY) {
      root["output_targets"] = "serial1_only";
    } else if (state.outputTargets == SERIAL_WRITER_FORWARDER_OUTPUT_BOTH) {
      root["output_targets"] = "both";
    } else {
      root["output_targets"] = "usb_only";
    }
    root["auth_username"] = state.authUsername;
    // auth_password intentionally omitted from read for security
    root["connected"] = state.connected;
    root["last_received_line"] = state.lastReceivedLine;
    root["last_received_ms"] = state.lastReceivedMs;
    root["received_count"] = state.receivedCount;
    root["last_error"] = state.lastError;
  }

  // Config-only read — used by FSPersistence
  static void readConfig(SerialWriterForwarderState& state, JsonObject& root) {
    root["enabled"] = state.enabled;
    root["protocol"] = state.protocol;
    root["source_url"] = state.sourceUrl;
    root["json_field"] = state.jsonField;
    root["poll_interval_ms"] = state.pollIntervalMs;
    if (state.outputTargets == SERIAL_WRITER_FORWARDER_OUTPUT_SERIAL1_ONLY) {
      root["output_targets"] = "serial1_only";
    } else if (state.outputTargets == SERIAL_WRITER_FORWARDER_OUTPUT_BOTH) {
      root["output_targets"] = "both";
    } else {
      root["output_targets"] = "usb_only";
    }
    root["auth_username"] = state.authUsername;
    root["auth_password"] = state.authPassword;
  }

  // Config-only update — used by FSPersistence on load
  static StateUpdateResult updateConfig(JsonObject& root, SerialWriterForwarderState& state) {
    state.enabled = root["enabled"] | false;
    state.protocol = root["protocol"] | (uint8_t)SERIAL_WRITER_FORWARDER_PROTOCOL_HTTP;
    state.sourceUrl = root["source_url"] | "";
    state.jsonField = root["json_field"] | "last_line";
    state.pollIntervalMs = root["poll_interval_ms"] | (uint32_t)1000;
    state.authUsername = root["auth_username"] | "";
    state.authPassword = root["auth_password"] | "";

    if (state.protocol > SERIAL_WRITER_FORWARDER_PROTOCOL_WS) {
      state.protocol = SERIAL_WRITER_FORWARDER_PROTOCOL_HTTP;
    }
    if (state.pollIntervalMs < 100) state.pollIntervalMs = 100;
    if (state.jsonField.length() == 0) state.jsonField = "last_line";

    if (root.containsKey("output_targets")) {
      state.outputTargets = parseOutputTargets(root["output_targets"]);
    } else {
      state.outputTargets = SERIAL_WRITER_FORWARDER_OUTPUT_USB_ONLY;
    }

    return StateUpdateResult::CHANGED;
  }

  // Full update — accepts config fields from REST POST
  static StateUpdateResult update(JsonObject& root, SerialWriterForwarderState& state) {
    StateUpdateResult result = StateUpdateResult::UNCHANGED;

    if (root.containsKey("enabled")) {
      bool v = root["enabled"];
      if (v != state.enabled) {
        state.enabled = v;
        result = StateUpdateResult::CHANGED;
      }
    }
    if (root.containsKey("protocol")) {
      uint8_t v = root["protocol"];
      if (v <= SERIAL_WRITER_FORWARDER_PROTOCOL_WS && v != state.protocol) {
        state.protocol = v;
        result = StateUpdateResult::CHANGED;
      }
    }
    if (root.containsKey("source_url")) {
      String v = root["source_url"].as<String>();
      if (v != state.sourceUrl) {
        state.sourceUrl = v;
        result = StateUpdateResult::CHANGED;
      }
    }
    if (root.containsKey("json_field")) {
      String v = root["json_field"].as<String>();
      if (v.length() > 0 && v != state.jsonField) {
        state.jsonField = v;
        result = StateUpdateResult::CHANGED;
      }
    }
    if (root.containsKey("poll_interval_ms")) {
      uint32_t v = root["poll_interval_ms"];
      if (v >= 100 && v != state.pollIntervalMs) {
        state.pollIntervalMs = v;
        result = StateUpdateResult::CHANGED;
      }
    }
    if (root.containsKey("auth_username")) {
      String v = root["auth_username"].as<String>();
      if (v != state.authUsername) {
        state.authUsername = v;
        result = StateUpdateResult::CHANGED;
      }
    }
    if (root.containsKey("auth_password")) {
      String v = root["auth_password"].as<String>();
      if (v != state.authPassword) {
        state.authPassword = v;
        result = StateUpdateResult::CHANGED;
      }
    }
    if (root.containsKey("output_targets")) {
      uint8_t next = parseOutputTargets(root["output_targets"]);
      if (next != state.outputTargets) {
        state.outputTargets = next;
        result = StateUpdateResult::CHANGED;
      }
    }
    return result;
  }

 private:
  static uint8_t parseOutputTargets(JsonVariant v) {
    if (v.is<int>() || v.is<unsigned int>()) {
      int i = v.as<int>();
      if (i == SERIAL_WRITER_FORWARDER_OUTPUT_SERIAL1_ONLY) return SERIAL_WRITER_FORWARDER_OUTPUT_SERIAL1_ONLY;
      if (i == SERIAL_WRITER_FORWARDER_OUTPUT_BOTH) return SERIAL_WRITER_FORWARDER_OUTPUT_BOTH;
      return SERIAL_WRITER_FORWARDER_OUTPUT_USB_ONLY;
    }
    String s = v.as<String>();
    s.trim();
    s.toLowerCase();
    if (s == "serial1_only" || s == "uart" || s == "serial1") {
      return SERIAL_WRITER_FORWARDER_OUTPUT_SERIAL1_ONLY;
    }
    if (s == "both" || s == "usb_and_serial1" || s == "mirror") {
      return SERIAL_WRITER_FORWARDER_OUTPUT_BOTH;
    }
    return SERIAL_WRITER_FORWARDER_OUTPUT_USB_ONLY;
  }
};

#endif
