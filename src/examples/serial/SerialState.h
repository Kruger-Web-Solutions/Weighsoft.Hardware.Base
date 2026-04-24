#ifndef SerialState_h
#define SerialState_h

#include <StatefulService.h>

#define SERIAL_DEFAULT_BAUDRATE 9600
#define SERIAL_MIN_BAUDRATE 300
#define SERIAL_MAX_BAUDRATE 2000000

class SerialState {
 public:
  // Data fields (read from serial, broadcast to channels)
  String lastLine;           // Full original line from serial
  String weight;             // Extracted weight value (empty if regex failed)
  unsigned long timestamp;   // millis() when line was received

  // Configuration fields (user-configurable via REST/UI)
  uint32_t baudrate;         // 9600, 19200, 38400, 57600, 115200, etc.
  uint8_t databits;          // 5, 6, 7, or 8
  uint8_t stopbits;          // 1 or 2
  uint8_t parity;            // 0=None, 1=Even, 2=Odd
  String regexPattern;       // Pattern to extract weight (e.g. first capture group)

  // Runtime-only diagnostics (not persisted); updated from SerialService::loop for REST/UI
  int dbgUartRxAvail = -1;   // SERIAL_PORT.available() when UART owned by SerialService, else -1
  uint8_t dbgSuspended = 0;
  uint8_t dbgSerialStarted = 0;
  uint16_t dbgLineBufLen = 0;

  static void read(SerialState& state, JsonObject& root) {
    root["last_line"] = state.lastLine;
    root["weight"] = state.weight;
    root["timestamp"] = state.timestamp;
    root["baud_rate"] = state.baudrate;
    root["data_bits"] = state.databits;
    root["stop_bits"] = state.stopbits;
    root["parity"] = state.parity;
    root["regex_pattern"] = state.regexPattern;
    root["dbg_uart_rx_avail"] = state.dbgUartRxAvail;
    root["dbg_suspended"] = state.dbgSuspended;
    root["dbg_serial_started"] = state.dbgSerialStarted;
    root["dbg_line_buf_len"] = state.dbgLineBufLen;
  }

  // Config-only read/update for FSPersistence (does not persist runtime data)
  static void readConfig(SerialState& state, JsonObject& root) {
    root["baud_rate"] = state.baudrate;
    root["data_bits"] = state.databits;
    root["stop_bits"] = state.stopbits;
    root["parity"] = state.parity;
    root["regex_pattern"] = state.regexPattern;
  }

  static StateUpdateResult updateConfig(JsonObject& root, SerialState& state) {
    uint32_t baud = root["baud_rate"] | SERIAL_DEFAULT_BAUDRATE;
    uint8_t data = root["data_bits"] | (uint8_t)8;
    uint8_t stop = root["stop_bits"] | (uint8_t)1;
    uint8_t par = root["parity"] | (uint8_t)0;
    String regex = root["regex_pattern"] | "";

    if (baud < SERIAL_MIN_BAUDRATE || baud > SERIAL_MAX_BAUDRATE) baud = SERIAL_DEFAULT_BAUDRATE;
    if (data < 5 || data > 8) data = 8;
    if (stop < 1 || stop > 2) stop = 1;
    if (par > 2) par = 0;

    state.baudrate = baud;
    state.databits = data;
    state.stopbits = stop;
    state.parity = par;
    state.regexPattern = regex;
    return StateUpdateResult::CHANGED;
  }

  static StateUpdateResult update(JsonObject& root, SerialState& state) {
    StateUpdateResult result = StateUpdateResult::UNCHANGED;

    if (root.containsKey("baud_rate")) {
      uint32_t v = root["baud_rate"];
      if (v >= SERIAL_MIN_BAUDRATE && v <= SERIAL_MAX_BAUDRATE && v != state.baudrate) {
        state.baudrate = v;
        result = StateUpdateResult::CHANGED;
      }
    }
    if (root.containsKey("data_bits")) {
      uint8_t v = root["data_bits"];
      if (v >= 5 && v <= 8 && v != state.databits) {
        state.databits = v;
        result = StateUpdateResult::CHANGED;
      }
    }
    if (root.containsKey("stop_bits")) {
      uint8_t v = root["stop_bits"];
      if ((v == 1 || v == 2) && v != state.stopbits) {
        state.stopbits = v;
        result = StateUpdateResult::CHANGED;
      }
    }
    if (root.containsKey("parity")) {
      uint8_t v = root["parity"];
      if (v <= 2 && v != state.parity) {
        state.parity = v;
        result = StateUpdateResult::CHANGED;
      }
    }
    if (root.containsKey("regex_pattern")) {
      String v = root["regex_pattern"].as<String>();
      if (v != state.regexPattern) {
        state.regexPattern = v;
        result = StateUpdateResult::CHANGED;
      }
    }
    return result;
  }
};

#endif
