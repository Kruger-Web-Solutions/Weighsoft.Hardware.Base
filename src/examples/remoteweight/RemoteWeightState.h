#ifndef RemoteWeightState_h
#define RemoteWeightState_h

#include <StatefulService.h>

class RemoteWeightState {
 public:
  String weight;
  String lastLine;
  unsigned long timestamp;
  bool enabled;
  bool displayEnabled;
  bool usbEchoEnabled = false;  // when true, every received scale line is echoed to Serial (USB CDC COM port)

  static void read(RemoteWeightState& state, JsonObject& root) {
    root["weight"] = state.weight;
    root["last_line"] = state.lastLine;
    root["timestamp"] = state.timestamp;
    root["enabled"] = state.enabled;
    root["display_enabled"] = state.displayEnabled;
    root["usb_echo_enabled"] = state.usbEchoEnabled;
  }

  static void readConfig(RemoteWeightState& state, JsonObject& root) {
    root["enabled"] = state.enabled;
    root["display_enabled"] = state.displayEnabled;
    root["usb_echo_enabled"] = state.usbEchoEnabled;
  }

  static StateUpdateResult updateConfig(JsonObject& root, RemoteWeightState& state) {
    state.enabled = root["enabled"] | true;
    state.displayEnabled = root["display_enabled"] | true;
    state.usbEchoEnabled = root["usb_echo_enabled"] | false;
    return StateUpdateResult::CHANGED;
  }

  static StateUpdateResult update(JsonObject& root, RemoteWeightState& state) {
    bool changed = false;

    if (root.containsKey("enabled")) {
      bool v = root["enabled"];
      if (v != state.enabled) { state.enabled = v; changed = true; }
    }

    if (root.containsKey("display_enabled")) {
      bool v = root["display_enabled"];
      if (v != state.displayEnabled) { state.displayEnabled = v; changed = true; }
    }

    if (root.containsKey("usb_echo_enabled")) {
      bool v = root["usb_echo_enabled"];
      if (v != state.usbEchoEnabled) { state.usbEchoEnabled = v; changed = true; }
    }

    if (root.containsKey("weight")) {
      state.weight = root["weight"].as<String>();
    }

    if (root.containsKey("last_line")) {
      state.lastLine = root["last_line"].as<String>();
    }

    // A weight payload from the Forwarder always carries `weight` and/or
    // `last_line`. Treat it as a real reading even when the value is identical
    // to the previous reading (e.g. a stable scale heartbeat) so update
    // handlers actually fire — otherwise StatefulService skips them and the
    // USB echo never triggers for steady weight.
    bool isWeightPayload = root.containsKey("weight") || root.containsKey("last_line");
    if (isWeightPayload) {
      state.timestamp = millis();
      changed = true;
    }

    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }
};

#endif
