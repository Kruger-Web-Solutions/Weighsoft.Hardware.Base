#ifndef UartMode_h
#define UartMode_h

#include <StatefulService.h>

// UART Mode: which service owns Serial1 (GPIO18 RX / GPIO17 TX) on ESP32 targets.
// Only one service can use Serial1 at a time.
enum class UartModeType {
  READER,        // SerialService — reads from a physical scale (was LIVE_MONITORING)
  WRITER,        // SerialWriterService — writes to a physical serial port
};

class UartModeState {
 public:
  // Empty modeStr ("") represents the NEW state — device hasn't been told what it is yet.
  // When configured, modeStr is one of: "reader", "writer".
  String modeStr;

  UartModeState() : modeStr("") {}

  bool isConfigured() const { return modeStr.length() > 0; }
  bool isReader() const { return modeStr == "reader"; }
  bool isWriter() const { return modeStr == "writer"; }
  UartModeType type() const {
    if (modeStr == "writer") return UartModeType::WRITER;
    return UartModeType::READER;  // default for empty/NEW or "reader"
  }

  static void read(UartModeState& state, JsonObject& root) {
    root["mode"] = state.modeStr;  // "" means NEW
  }

  static StateUpdateResult update(JsonObject& root, UartModeState& state) {
    if (root.containsKey("mode")) {
      String incoming = root["mode"].as<String>();

      // Migration: legacy "live" → "reader"
      if (incoming == "live") incoming = "reader";
      // Migration: legacy "diagnostics" → "reader"
      if (incoming == "diagnostics") incoming = "reader";

      // Validate
      if (incoming != "" && incoming != "reader" && incoming != "writer") {
        return StateUpdateResult::ERROR;
      }

      if (incoming != state.modeStr) {
        state.modeStr = incoming;
        return StateUpdateResult::CHANGED;
      }
    }
    return StateUpdateResult::UNCHANGED;
  }
};

#endif
