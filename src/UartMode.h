#ifndef UartMode_h
#define UartMode_h

#include <StatefulService.h>

// UART Mode: controls which service owns Serial1 (GPIO18 RX / GPIO17 TX on ESP32 targets)
// Only one service can use Serial1 at a time
enum class UartModeType {
  LIVE_MONITORING,  // SerialService active (scale monitoring)
  DIAGNOSTICS       // DiagnosticsService active (hardware tests)
};

class UartModeState {
 public:
  uint8_t mode;  // 0=LIVE_MONITORING, 1=DIAGNOSTICS

  // Constructor: initialize to LIVE_MONITORING by default
  UartModeState() : mode((uint8_t)UartModeType::LIVE_MONITORING) {}

  static void read(UartModeState& state, JsonObject& root) {
    root["mode"] = state.mode == (uint8_t)UartModeType::LIVE_MONITORING ? "live" : "diagnostics";
  }

  static StateUpdateResult update(JsonObject& root, UartModeState& state) {
    if (root.containsKey("mode")) {
      String modeStr = root["mode"].as<String>();
      uint8_t newMode;
      
      if (modeStr == "live") {
        newMode = (uint8_t)UartModeType::LIVE_MONITORING;
      } else if (modeStr == "diagnostics") {
        newMode = (uint8_t)UartModeType::DIAGNOSTICS;
      } else {
        return StateUpdateResult::ERROR;
      }
      
      if (newMode != state.mode) {
        state.mode = newMode;
        return StateUpdateResult::CHANGED;
      }
    }
    return StateUpdateResult::UNCHANGED;
  }
};

#endif
