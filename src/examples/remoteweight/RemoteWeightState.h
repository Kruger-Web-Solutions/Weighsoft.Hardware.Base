#ifndef RemoteWeightState_h
#define RemoteWeightState_h

#include <StatefulService.h>

class RemoteWeightState {
 public:
  String weight;
  String lastLine;
  unsigned long timestamp;

  static void read(RemoteWeightState& state, JsonObject& root) {
    root["weight"] = state.weight;
    root["last_line"] = state.lastLine;
    root["timestamp"] = state.timestamp;
  }

  static StateUpdateResult update(JsonObject& root, RemoteWeightState& state) {
    bool changed = false;

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

    if (root.containsKey("timestamp")) {
      unsigned long v = root["timestamp"] | 0UL;
      if (v != state.timestamp) {
        state.timestamp = v;
        changed = true;
      }
    }

    if (changed) {
      state.timestamp = millis();
    }

    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }
};

#endif
