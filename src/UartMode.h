#ifndef UartMode_h
#define UartMode_h

#include <StatefulService.h>

// UART Mode: controls which service owns Serial1 (GPIO18/17 on ESP32-S3)
// Only one service can use Serial1 at a time
enum class UartModeType {
  LIVE_MONITORING,  // SerialService active (scale monitoring / reading)
  WRITER,           // SerialWriterService active (sending data out)
  DIAGNOSTICS       // DiagnosticsService active (hardware tests)
};

class UartModeState {
 public:
  uint8_t mode;  // 0=LIVE_MONITORING, 1=WRITER, 2=DIAGNOSTICS

  // Constructor: initialize to LIVE_MONITORING by default
  UartModeState() : mode((uint8_t)UartModeType::LIVE_MONITORING) {}

  static void read(UartModeState& state, JsonObject& root) {
    if (state.mode == (uint8_t)UartModeType::LIVE_MONITORING) {
      root["mode"] = "live";
    } else if (state.mode == (uint8_t)UartModeType::WRITER) {
      root["mode"] = "writer";
    } else {
      root["mode"] = "diagnostics";
    }
  }

  static StateUpdateResult update(JsonObject& root, UartModeState& state) {
    if (root.containsKey("mode")) {
      String modeStr = root["mode"].as<String>();
      uint8_t newMode;

      if (modeStr == "live") {
        newMode = (uint8_t)UartModeType::LIVE_MONITORING;
      } else if (modeStr == "writer") {
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
