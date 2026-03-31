#ifndef SerialWriterState_h
#define SerialWriterState_h

#include <StatefulService.h>

#define SERIAL_WRITER_DEFAULT_BAUDRATE 9600
#define SERIAL_WRITER_MIN_BAUDRATE 300
#define SERIAL_WRITER_MAX_BAUDRATE 2000000

// Line terminator options
#define SERIAL_WRITER_TERMINATOR_LF "LF"
#define SERIAL_WRITER_TERMINATOR_CRLF "CRLF"
#define SERIAL_WRITER_TERMINATOR_CR "CR"
#define SERIAL_WRITER_TERMINATOR_NONE "NONE"

class SerialWriterState {
 public:
  // Config fields (persisted to /config/serialWriterConfig.json)
  uint32_t baudrate;
  uint8_t databits;
  uint8_t stopbits;
  uint8_t parity;
  String lineTerminator;

  // Runtime send queue (not persisted — cleared after each write)
  String pendingLine;
  String lastSentLine;
  unsigned long lastSentMs;
  uint32_t sentCount;

  SerialWriterState()
      : baudrate(SERIAL_WRITER_DEFAULT_BAUDRATE),
        databits(8),
        stopbits(1),
        parity(0),
        lineTerminator(SERIAL_WRITER_TERMINATOR_LF),
        lastSentMs(0),
        sentCount(0) {}

  // Full read — config + runtime status (used by REST/WebSocket/MQTT/BLE)
  static void read(SerialWriterState& state, JsonObject& root) {
    root["baud_rate"] = state.baudrate;
    root["data_bits"] = state.databits;
    root["stop_bits"] = state.stopbits;
    root["parity"] = state.parity;
    root["line_terminator"] = state.lineTerminator;
    root["pending_line"] = state.pendingLine;
    root["last_sent_line"] = state.lastSentLine;
    root["last_sent_ms"] = state.lastSentMs;
    root["sent_count"] = state.sentCount;
  }

  // Config-only read — used by FSPersistence (does not persist runtime data)
  static void readConfig(SerialWriterState& state, JsonObject& root) {
    root["baud_rate"] = state.baudrate;
    root["data_bits"] = state.databits;
    root["stop_bits"] = state.stopbits;
    root["parity"] = state.parity;
    root["line_terminator"] = state.lineTerminator;
  }

  // Config-only update — used by FSPersistence on load
  static StateUpdateResult updateConfig(JsonObject& root, SerialWriterState& state) {
    uint32_t baud = root["baud_rate"] | SERIAL_WRITER_DEFAULT_BAUDRATE;
    uint8_t data = root["data_bits"] | (uint8_t)8;
    uint8_t stop = root["stop_bits"] | (uint8_t)1;
    uint8_t par = root["parity"] | (uint8_t)0;
    String term = root["line_terminator"] | SERIAL_WRITER_TERMINATOR_LF;

    if (baud < SERIAL_WRITER_MIN_BAUDRATE || baud > SERIAL_WRITER_MAX_BAUDRATE) {
      baud = SERIAL_WRITER_DEFAULT_BAUDRATE;
    }
    if (data < 5 || data > 8) data = 8;
    if (stop < 1 || stop > 2) stop = 1;
    if (par > 2) par = 0;
    if (term != SERIAL_WRITER_TERMINATOR_LF && term != SERIAL_WRITER_TERMINATOR_CRLF &&
        term != SERIAL_WRITER_TERMINATOR_CR && term != SERIAL_WRITER_TERMINATOR_NONE) {
      term = SERIAL_WRITER_TERMINATOR_LF;
    }

    state.baudrate = baud;
    state.databits = data;
    state.stopbits = stop;
    state.parity = par;
    state.lineTerminator = term;
    return StateUpdateResult::CHANGED;
  }

  // Full update — accepts config fields and pending_line trigger
  static StateUpdateResult update(JsonObject& root, SerialWriterState& state) {
    StateUpdateResult result = StateUpdateResult::UNCHANGED;

    if (root.containsKey("baud_rate")) {
      uint32_t v = root["baud_rate"];
      if (v >= SERIAL_WRITER_MIN_BAUDRATE && v <= SERIAL_WRITER_MAX_BAUDRATE && v != state.baudrate) {
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
    if (root.containsKey("line_terminator")) {
      String v = root["line_terminator"].as<String>();
      if ((v == SERIAL_WRITER_TERMINATOR_LF || v == SERIAL_WRITER_TERMINATOR_CRLF ||
           v == SERIAL_WRITER_TERMINATOR_CR || v == SERIAL_WRITER_TERMINATOR_NONE) &&
          v != state.lineTerminator) {
        state.lineTerminator = v;
        result = StateUpdateResult::CHANGED;
      }
    }
    // pending_line — the write trigger; accepted from any protocol channel
    if (root.containsKey("pending_line")) {
      String v = root["pending_line"].as<String>();
      if (v.length() > 0 && v != state.pendingLine) {
        state.pendingLine = v;
        result = StateUpdateResult::CHANGED;
      }
    }
    return result;
  }
};

#endif
