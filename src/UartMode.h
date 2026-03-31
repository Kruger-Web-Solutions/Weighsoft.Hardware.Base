#ifndef UartMode_h
#define UartMode_h

#include <StatefulService.h>

// UART Mode: controls which service owns Serial1 (GPIO17/18 on ESP32-S3)
// Only one service can use Serial1 at a time
enum class UartModeType {
  WRITER,      // SerialWriterService active (sending data out)
  DIAGNOSTICS  // DiagnosticsService active (hardware tests)
};

class UartModeState {
 public:
  uint8_t mode;  // 0=WRITER, 1=DIAGNOSTICS

  // Constructor: default to WRITER mode
  UartModeState() : mode((uint8_t)UartModeType::WRITER) {}

  static void read(UartModeState& state, JsonObject& root) {
    root["mode"] = state.mode == (uint8_t)UartModeType::WRITER ? "writer" : "diagnostics";
  }

  static StateUpdateResult update(JsonObject& root, UartModeState& state) {
    if (root.containsKey("mode")) {
      String modeStr = root["mode"].as<String>();
      uint8_t newMode;

      if (modeStr == "writer") {
        newMode = (uint8_t)UartModeType::WRITER;
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
