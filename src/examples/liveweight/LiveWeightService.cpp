#include <examples/liveweight/LiveWeightService.h>

#ifdef ESP8266
#include <regex.h>
#endif
#ifdef ESP32
#include <regex.h>
#endif

LiveWeightService::LiveWeightService(AsyncWebServer* server,
                                     FS* fs,
                                     SecurityManager* securityManager,
                                     AsyncMqttClient* mqttClient) :
    _httpEndpoint(LiveWeightState::read,
                  LiveWeightState::update,
                  this,
                  server,
                  LIVE_WEIGHT_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED),
    _fsPersistence(LiveWeightState::readConfig,
                   LiveWeightState::updateConfig,
                   this,
                   fs,
                   LIVE_WEIGHT_CONFIG_FILE),
    _mqttPubSub(LiveWeightState::read, LiveWeightState::update, this, mqttClient),
    _webSocket(LiveWeightState::read,
               LiveWeightState::update,
               this,
               server,
               LIVE_WEIGHT_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_AUTHENTICATED),
    _mqttClient(mqttClient),
    _serialConfigured(false),
    _appliedSource(LIVE_WEIGHT_SOURCE_WIFI),
    _appliedBaud(LIVE_WEIGHT_DEFAULT_BAUD),
    _appliedRegex(LIVE_WEIGHT_DEFAULT_REGEX),
    _appliedRs485Enabled(false),
    _appliedRs485Address(1) {
  _mqttBasePath = SettingValue::format("weighsoft/liveWeight/#{unique_id}");
  _mqttClient->onConnect(std::bind(&LiveWeightService::configureMqtt, this));

  // Avoid flash thrash on every weight packet — persist only on config changes
  _fsPersistence.disableUpdateHandler();

  addUpdateHandler(
      [this](const String& originId) {
        if (originId == "serial_hw" || originId == "init") {
          return;
        }
        if (configChanged()) {
          onConfigUpdated();
        }
      },
      false);
}

bool LiveWeightService::configChanged() const {
  return _state.source != _appliedSource || _state.baudrate != _appliedBaud || _state.regexPattern != _appliedRegex ||
         _state.rs485Enabled != _appliedRs485Enabled || _state.rs485Address != _appliedRs485Address;
}

void LiveWeightService::begin() {
  _state.source = LIVE_WEIGHT_SOURCE_WIFI;
  _state.baudrate = LIVE_WEIGHT_DEFAULT_BAUD;
  _state.regexPattern = LIVE_WEIGHT_DEFAULT_REGEX;
  _state.rs485Enabled = false;
  _state.rs485Address = 1;
  _state.weight = "";
  _state.lastLine = "";
  _state.timestamp = 0;
  _state.activeSource = "none";
  _state.statusMessage = "Waiting for weight";
  _lineBuffer = "";

  _fsPersistence.readFromFS();

  if (_state.source == LIVE_WEIGHT_SOURCE_RS485) {
    _state.statusMessage = "RS-485 selected — hardware adapter not fitted yet";
  } else if (_state.source == LIVE_WEIGHT_SOURCE_SERIAL) {
    _state.statusMessage = "Listening on UART0 (MAX3232 / PROG header)";
  } else {
    _state.statusMessage = "Ready for WiFi / WebSocket weight";
  }

  applySource();
  Serial.println(F("[LiveWeight] Service ready — /rest/liveWeight /ws/liveWeight"));
}

void LiveWeightService::loop() {
  if (_state.source == LIVE_WEIGHT_SOURCE_SERIAL) {
    readSerialLine();
  }
}

void LiveWeightService::onConfigUpdated() {
  _fsPersistence.writeToFS();
  applySource();
}

void LiveWeightService::applySource() {
  _appliedSource = _state.source;
  _appliedBaud = _state.baudrate;
  _appliedRegex = _state.regexPattern;
  _appliedRs485Enabled = _state.rs485Enabled;
  _appliedRs485Address = _state.rs485Address;

  if (_state.source == LIVE_WEIGHT_SOURCE_SERIAL) {
    uint32_t baud = _state.baudrate;
    if (baud < LIVE_WEIGHT_MIN_BAUD || baud > LIVE_WEIGHT_MAX_BAUD) {
      baud = LIVE_WEIGHT_DEFAULT_BAUD;
    }
    // ESP12F_Relay_X4: scale RS-232 lands on UART0 via the MAX3232 / PROG header.
    // Re-baud Serial for the indicator; debug prints stay usable at the new rate.
    Serial.flush();
    Serial.begin(baud);
    delay(50);
    _serialConfigured = true;
    _lineBuffer = "";
    update(
        [&](LiveWeightState& state) {
          state.statusMessage = "Listening on UART0 (MAX3232 / PROG header)";
          state.activeSource = "serial";
          return StateUpdateResult::CHANGED;
        },
        "init");
    Serial.printf("[LiveWeight] Serial source @ %lu baud\n", (unsigned long)baud);
    return;
  }

  if (_state.source == LIVE_WEIGHT_SOURCE_RS485) {
    _serialConfigured = false;
    update(
        [&](LiveWeightState& state) {
          state.statusMessage = "RS-485 stub — fit a transceiver module to enable";
          state.activeSource = "rs485";
          return StateUpdateResult::CHANGED;
        },
        "init");
    Serial.println(F("[LiveWeight] RS-485 source selected (stub)"));
    return;
  }

  // WiFi default — restore debug baud if we previously took UART0
  if (_serialConfigured) {
    Serial.flush();
    Serial.begin(115200);
    delay(50);
    _serialConfigured = false;
  }
  update(
      [&](LiveWeightState& state) {
        state.statusMessage = "Ready for WiFi / WebSocket weight";
        state.activeSource = "wifi";
        return StateUpdateResult::CHANGED;
      },
      "init");
  Serial.println(F("[LiveWeight] WiFi / WebSocket source active"));
}

void LiveWeightService::readSerialLine() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (_lineBuffer.length() > 0) {
        String extracted = extractWeight(_lineBuffer);
        String line = _lineBuffer;
        update(
            [&](LiveWeightState& state) {
              state.lastLine = line;
              state.weight = extracted;
              state.timestamp = millis();
              state.activeSource = "serial";
              state.statusMessage = extracted.length() ? "Weight from serial" : "Serial line (no weight match)";
              return StateUpdateResult::CHANGED;
            },
            "serial_hw");
        _lineBuffer = "";
      }
    } else {
      _lineBuffer += c;
      if (_lineBuffer.length() > 512) {
        _lineBuffer = "";
      }
    }
  }
}

String LiveWeightService::extractWeightSimple(const String& line) {
  int start = -1;
  for (unsigned int i = 0; i < line.length(); i++) {
    char c = line[i];
    if ((c >= '0' && c <= '9') || c == '+' || c == '-') {
      start = (int)i;
      break;
    }
  }
  if (start < 0) {
    return "";
  }
  int end = start + 1;
  bool seenDot = false;
  for (; end < (int)line.length(); end++) {
    char c = line[end];
    if (c >= '0' && c <= '9') {
      continue;
    }
    if ((c == '.' || c == ',') && !seenDot) {
      seenDot = true;
      continue;
    }
    break;
  }
  String extracted = line.substring(start, end);
  extracted.replace(',', '.');
  float value = extracted.toFloat();
  if (value != 0.0f || extracted.indexOf('0') >= 0) {
    return String(value, 2);
  }
  return "";
}

String LiveWeightService::extractWeight(const String& line) {
  const String& pattern = _state.regexPattern;
  if (pattern.length() == 0) {
    return extractWeightSimple(line);
  }

  regex_t regex;
  regmatch_t matches[2];
  int reti = regcomp(&regex, pattern.c_str(), REG_EXTENDED);
  if (reti != 0) {
    return extractWeightSimple(line);
  }

  reti = regexec(&regex, line.c_str(), 2, matches, 0);
  regfree(&regex);

  if (reti == 0) {
    int start = matches[1].rm_so >= 0 ? matches[1].rm_so : matches[0].rm_so;
    int end = matches[1].rm_eo >= 0 ? matches[1].rm_eo : matches[0].rm_eo;
    if (start >= 0 && end > start) {
      String extracted = line.substring(start, end);
      extracted.replace(',', '.');
      float value = extracted.toFloat();
      if (value != 0.0f || extracted.indexOf('0') >= 0) {
        return String(value, 2);
      }
    }
  }

  return extractWeightSimple(line);
}

void LiveWeightService::configureMqtt() {
  if (!_mqttClient->connected()) {
    return;
  }
  String pubTopic = _mqttBasePath + "/data";
  String subTopic = _mqttBasePath + "/set";
  _mqttPubSub.configureTopics(pubTopic, subTopic);
  Serial.printf("[LiveWeight] MQTT pub=%s sub=%s\n", pubTopic.c_str(), subTopic.c_str());
}
