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

  static void read(RemoteWeightState& state, JsonObject& root) {
    root["weight"] = state.weight;
    root["last_line"] = state.lastLine;
    root["timestamp"] = state.timestamp;
    root["enabled"] = state.enabled;
    root["display_enabled"] = state.displayEnabled;
  }

  static void readConfig(RemoteWeightState& state, JsonObject& root) {
    root["enabled"] = state.enabled;
    root["display_enabled"] = state.displayEnabled;
  }

  static StateUpdateResult updateConfig(JsonObject& root, RemoteWeightState& state) {
    state.enabled = root["enabled"] | true;
    state.displayEnabled = root["display_enabled"] | true;
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

    if (root.containsKey("weight")) {
      String v = root["weight"].as<String>();
      if (v != state.weight) { state.weight = v; changed = true; }
    }

    if (root.containsKey("last_line")) {
      String v = root["last_line"].as<String>();
      if (v != state.lastLine) { state.lastLine = v; changed = true; }
    }

    if (root.containsKey("timestamp")) {
      unsigned long v = root["timestamp"] | 0UL;
      if (v != state.timestamp) { state.timestamp = v; changed = true; }
    }

    if (changed && !state.weight.isEmpty()) {
      state.timestamp = millis();
    }

    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }
};

#endif
