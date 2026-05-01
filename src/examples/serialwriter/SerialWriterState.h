#ifndef SerialWriterState_h
#define SerialWriterState_h

#include <StatefulService.h>
#include <SettingValue.h>

#define SERIALW_DEFAULT_BAUDRATE 9600
#define SERIALW_MIN_BAUDRATE     300
#define SERIALW_MAX_BAUDRATE     2000000

#define SERIALW_LE_NONE 0
#define SERIALW_LE_CR   1
#define SERIALW_LE_LF   2
#define SERIALW_LE_CRLF 3

class SerialWriterState {
 public:
  // Persisted configuration
  uint32_t baudrate;       // 9600, 19200, etc.
  uint8_t  databits;       // 5..8
  uint8_t  stopbits;       // 1 or 2
  uint8_t  parity;         // 0=None,1=Even,2=Odd
  uint8_t  lineEnding;     // 0=None,1=CR,2=LF,3=CRLF
  String   friendlyName;   // user-set device name shown on Reader's Writers tab
  String   mqttSubscribeTopic;
  String   mqttStatusTopic;
  bool     mqttEnabled;

  // Runtime stats (not persisted)
  String        lastSent;
  unsigned long lastSentAt = 0;
  uint8_t       lastSentSource = 0;  // 0=NONE,1=MANUAL,2=REST,3=WS,4=MQTT,5=READER
  unsigned long bytesSentTotal = 0;
  uint8_t       suspended = 0;
  uint8_t       serialStarted = 0;

  SerialWriterState()
      : baudrate(SERIALW_DEFAULT_BAUDRATE),
        databits(8),
        stopbits(1),
        parity(0),
        lineEnding(SERIALW_LE_CRLF),
        friendlyName(""),
        mqttSubscribeTopic(""),
        mqttStatusTopic(""),
        mqttEnabled(false) {}

  static void read(SerialWriterState& state, JsonObject& root) {
    root["baud_rate"]            = state.baudrate;
    root["data_bits"]            = state.databits;
    root["stop_bits"]            = state.stopbits;
    root["parity"]               = state.parity;
    root["line_ending"]          = state.lineEnding;
    root["friendly_name"]        = state.friendlyName;
    root["mqtt_subscribe_topic"] = state.mqttSubscribeTopic;
    root["mqtt_status_topic"]    = state.mqttStatusTopic;
    root["mqtt_enabled"]         = state.mqttEnabled;
    root["device_id"]            = SettingValue::getUniqueId();

    root["last_sent"]            = state.lastSent;
    root["last_sent_at"]         = state.lastSentAt;
    root["last_sent_source"]     = state.lastSentSource;
    root["bytes_sent_total"]     = state.bytesSentTotal;
    root["suspended"]            = state.suspended;
    root["serial_started"]       = state.serialStarted;
  }

  static void readConfig(SerialWriterState& state, JsonObject& root) {
    root["baud_rate"]            = state.baudrate;
    root["data_bits"]            = state.databits;
    root["stop_bits"]            = state.stopbits;
    root["parity"]               = state.parity;
    root["line_ending"]          = state.lineEnding;
    root["friendly_name"]        = state.friendlyName;
    root["mqtt_subscribe_topic"] = state.mqttSubscribeTopic;
    root["mqtt_status_topic"]    = state.mqttStatusTopic;
    root["mqtt_enabled"]         = state.mqttEnabled;
  }

  static StateUpdateResult updateConfig(JsonObject& root, SerialWriterState& state) {
    state.baudrate           = root["baud_rate"]            | (uint32_t)SERIALW_DEFAULT_BAUDRATE;
    state.databits           = root["data_bits"]            | (uint8_t)8;
    state.stopbits           = root["stop_bits"]            | (uint8_t)1;
    state.parity             = root["parity"]               | (uint8_t)0;
    state.lineEnding         = root["line_ending"]          | (uint8_t)SERIALW_LE_CRLF;
    state.friendlyName       = root["friendly_name"]        | "";
    state.mqttSubscribeTopic = root["mqtt_subscribe_topic"] | "";
    state.mqttStatusTopic    = root["mqtt_status_topic"]    | "";
    state.mqttEnabled        = root["mqtt_enabled"]         | false;

    if (state.baudrate < SERIALW_MIN_BAUDRATE || state.baudrate > SERIALW_MAX_BAUDRATE) state.baudrate = SERIALW_DEFAULT_BAUDRATE;
    if (state.databits < 5 || state.databits > 8) state.databits = 8;
    if (state.stopbits < 1 || state.stopbits > 2) state.stopbits = 1;
    if (state.parity > 2) state.parity = 0;
    if (state.lineEnding > SERIALW_LE_CRLF) state.lineEnding = SERIALW_LE_CRLF;
    return StateUpdateResult::CHANGED;
  }

  static StateUpdateResult update(JsonObject& root, SerialWriterState& state) {
    StateUpdateResult result = StateUpdateResult::UNCHANGED;
    auto changed = [&result]() { result = StateUpdateResult::CHANGED; };

    if (root.containsKey("baud_rate")) {
      uint32_t v = root["baud_rate"];
      if (v >= SERIALW_MIN_BAUDRATE && v <= SERIALW_MAX_BAUDRATE && v != state.baudrate) { state.baudrate = v; changed(); }
    }
    if (root.containsKey("data_bits")) {
      uint8_t v = root["data_bits"];
      if (v >= 5 && v <= 8 && v != state.databits) { state.databits = v; changed(); }
    }
    if (root.containsKey("stop_bits")) {
      uint8_t v = root["stop_bits"];
      if ((v == 1 || v == 2) && v != state.stopbits) { state.stopbits = v; changed(); }
    }
    if (root.containsKey("parity")) {
      uint8_t v = root["parity"];
      if (v <= 2 && v != state.parity) { state.parity = v; changed(); }
    }
    if (root.containsKey("line_ending")) {
      uint8_t v = root["line_ending"];
      if (v <= SERIALW_LE_CRLF && v != state.lineEnding) { state.lineEnding = v; changed(); }
    }
    if (root.containsKey("friendly_name")) {
      String v = root["friendly_name"].as<String>();
      if (v != state.friendlyName) { state.friendlyName = v; changed(); }
    }
    if (root.containsKey("mqtt_subscribe_topic")) {
      String v = root["mqtt_subscribe_topic"].as<String>();
      if (v != state.mqttSubscribeTopic) { state.mqttSubscribeTopic = v; changed(); }
    }
    if (root.containsKey("mqtt_status_topic")) {
      String v = root["mqtt_status_topic"].as<String>();
      if (v != state.mqttStatusTopic) { state.mqttStatusTopic = v; changed(); }
    }
    if (root.containsKey("mqtt_enabled")) {
      bool v = root["mqtt_enabled"];
      if (v != state.mqttEnabled) { state.mqttEnabled = v; changed(); }
    }
    return result;
  }
};

#endif
