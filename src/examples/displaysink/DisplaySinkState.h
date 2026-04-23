#ifndef DisplaySinkState_h
#define DisplaySinkState_h

#include <StatefulService.h>

// Last payload received from WeightForwarder (WS or HTTP body shape).
class DisplaySinkState {
 public:
  String line1;
  String line2;
  String last_line;
  String weight;
  unsigned long source_timestamp;
  unsigned long last_rx_ms;

  static void read(DisplaySinkState& state, JsonObject& root) {
    root["line1"] = state.line1;
    root["line2"] = state.line2;
    root["last_line"] = state.last_line;
    root["weight"] = state.weight;
    root["source_timestamp"] = state.source_timestamp;
    root["last_rx_ms"] = state.last_rx_ms;
  }

  static StateUpdateResult update(JsonObject& root, DisplaySinkState& state) {
    bool changed = false;
    if (root.containsKey("line1")) {
      state.line1 = root["line1"].as<String>();
      changed = true;
    }
    if (root.containsKey("line2")) {
      state.line2 = root["line2"].as<String>();
      changed = true;
    }
    if (root.containsKey("last_line")) {
      state.last_line = root["last_line"].as<String>();
      changed = true;
    }
    if (root.containsKey("weight")) {
      state.weight = root["weight"].as<String>();
      changed = true;
    }
    if (root.containsKey("timestamp")) {
      state.source_timestamp = root["timestamp"].as<unsigned long>();
      changed = true;
    }
    if (changed) {
      state.last_rx_ms = millis();
    }
    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }
};

#endif
