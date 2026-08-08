#include <examples/liveweight/LiveWeightService.h>
#include <examples/relay/RelayBoardService.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <regex.h>
#endif
#ifdef ESP32
#include <WiFi.h>
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
    _relayBoard(nullptr),
    _serialConfigured(false),
    _appliedSource(LIVE_WEIGHT_SOURCE_WIFI),
    _appliedBaud(LIVE_WEIGHT_DEFAULT_BAUD),
    _appliedRegex(LIVE_WEIGHT_DEFAULT_REGEX),
    _appliedRs485Enabled(false),
    _appliedRs485Address(1),
    _appliedRangeEnabled(false),
    _appliedRangeLow(1.0f),
    _appliedRangeHigh(2.0f),
    _appliedRelayLow(1),
    _appliedRelayOk(2),
    _appliedRelayHigh(3),
    _appliedCount(1),
    _appliedUnit("kg"),
    _appliedDi1Action("none"),
    _appliedDi2Action("none"),
    _appliedPrinterEnabled(false),
    _appliedPrinterIp(""),
    _appliedPrinterPort(9100),
    _lastDrivenZone(255),
    _pendingAction(""),
    _printPending(false) {
  _mqttBasePath = SettingValue::format("weighsoft/liveWeight/#{unique_id}");
  _mqttClient->onConnect(std::bind(&LiveWeightService::configureMqtt, this));
  _fsPersistence.disableUpdateHandler();

  addUpdateHandler(
      [this](const String& originId) {
        if (originId == "init" || originId == "band" || originId == "print" || originId == "print_prep" ||
            originId == "di_action") {
          return;
        }
        // Queue print from REST trigger_action — never block AsyncWebServer disconnect path
        if (_state.printRequested) {
          _printPending = true;
        }
        if (originId == "serial_hw" || originId == "http" || originId == "mqtt" || originId.startsWith("websocket")) {
          if (configChanged()) {
            onConfigUpdated();
          }
          evaluateBandAndDrive(originId);
          return;
        }
        if (configChanged()) {
          onConfigUpdated();
          evaluateBandAndDrive("config");
        } else {
          evaluateBandAndDrive(originId);
        }
      },
      false);
}

void LiveWeightService::setRelayBoardService(RelayBoardService* relayBoard) {
  _relayBoard = relayBoard;
}

bool LiveWeightService::sourceSettingsChanged() const {
  return _state.source != _appliedSource || _state.baudrate != _appliedBaud || _state.regexPattern != _appliedRegex ||
         _state.rs485Enabled != _appliedRs485Enabled || _state.rs485Address != _appliedRs485Address;
}

bool LiveWeightService::configChanged() const {
  // Persisted settings only — not runtime last_action / action_seq / job_running
  return sourceSettingsChanged() || _state.rangeEnabled != _appliedRangeEnabled || _state.rangeLow != _appliedRangeLow ||
         _state.rangeHigh != _appliedRangeHigh || _state.relayLow != _appliedRelayLow ||
         _state.relayOk != _appliedRelayOk || _state.relayHigh != _appliedRelayHigh || _state.plu != _appliedPlu ||
         _state.product != _appliedProduct || _state.count != _appliedCount || _state.unit != _appliedUnit ||
         _state.di1Action != _appliedDi1Action || _state.di2Action != _appliedDi2Action ||
         _state.printerEnabled != _appliedPrinterEnabled || _state.printerIp != _appliedPrinterIp ||
         _state.printerPort != _appliedPrinterPort;
}

void LiveWeightService::syncAppliedConfig() {
  _appliedSource = _state.source;
  _appliedBaud = _state.baudrate;
  _appliedRegex = _state.regexPattern;
  _appliedRs485Enabled = _state.rs485Enabled;
  _appliedRs485Address = _state.rs485Address;
  _appliedRangeEnabled = _state.rangeEnabled;
  _appliedRangeLow = _state.rangeLow;
  _appliedRangeHigh = _state.rangeHigh;
  _appliedRelayLow = _state.relayLow;
  _appliedRelayOk = _state.relayOk;
  _appliedRelayHigh = _state.relayHigh;
  _appliedPlu = _state.plu;
  _appliedProduct = _state.product;
  _appliedCount = _state.count;
  _appliedUnit = _state.unit;
  _appliedDi1Action = _state.di1Action;
  _appliedDi2Action = _state.di2Action;
  _appliedPrinterEnabled = _state.printerEnabled;
  _appliedPrinterIp = _state.printerIp;
  _appliedPrinterPort = _state.printerPort;
}

void LiveWeightService::begin() {
  _state.source = LIVE_WEIGHT_SOURCE_WIFI;
  _state.baudrate = LIVE_WEIGHT_DEFAULT_BAUD;
  _state.regexPattern = LIVE_WEIGHT_DEFAULT_REGEX;
  _state.rs485Enabled = false;
  _state.rs485Address = 1;
  _state.rangeEnabled = false;
  _state.rangeLow = 1.0f;
  _state.rangeHigh = 2.0f;
  _state.relayLow = 1;
  _state.relayOk = 2;
  _state.relayHigh = 3;
  _state.plu = "";
  _state.product = "";
  _state.count = 1;
  _state.unit = "kg";
  _state.total = "";
  _state.zone = LIVE_WEIGHT_ZONE_NONE;
  _state.di1Action = "none";
  _state.di2Action = "none";
  _state.jobRunning = false;
  _state.lastAction = "";
  _state.actionSeq = 0;
  _state.printerEnabled = false;
  _state.printerIp = "";
  _state.printerPort = 9100;
  _state.printRequested = false;
  _state.weight = "";
  _state.lastLine = "";
  _state.timestamp = 0;
  _state.activeSource = "none";
  _state.statusMessage = "Waiting for weight";
  _lineBuffer = "";

  _fsPersistence.readFromFS();
  LiveWeightState::refreshTotal(_state);

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
  if (_pendingAction.length() > 0) {
    String action = _pendingAction;
    _pendingAction = "";
    runAction(action);
  }
  if (_printPending) {
    _printPending = false;
    sendNetworkPrint();
  }
  if (_state.source == LIVE_WEIGHT_SOURCE_SERIAL) {
    readSerialLine();
  }
}

void LiveWeightService::onConfigUpdated() {
  const bool reapplySource = sourceSettingsChanged();
  _fsPersistence.writeToFS();
  if (reapplySource) {
    applySource();
  } else {
    syncAppliedConfig();
  }
}

void LiveWeightService::applySource() {
  syncAppliedConfig();

  if (_state.source == LIVE_WEIGHT_SOURCE_SERIAL) {
    uint32_t baud = _state.baudrate;
    if (baud < LIVE_WEIGHT_MIN_BAUD || baud > LIVE_WEIGHT_MAX_BAUD) {
      baud = LIVE_WEIGHT_DEFAULT_BAUD;
    }
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
    return;
  }

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
}

uint8_t LiveWeightService::computeZone(float weight) const {
  if (!_state.rangeEnabled) {
    return LIVE_WEIGHT_ZONE_NONE;
  }
  if (weight < _state.rangeLow) {
    return LIVE_WEIGHT_ZONE_LOW;
  }
  if (weight > _state.rangeHigh) {
    return LIVE_WEIGHT_ZONE_HIGH;
  }
  return LIVE_WEIGHT_ZONE_OK;
}

void LiveWeightService::evaluateBandAndDrive(const String& originId) {
  uint8_t zone = LIVE_WEIGHT_ZONE_NONE;
  if (_state.weight.length() > 0) {
    zone = computeZone(_state.weight.toFloat());
  }

  String nextTotal = _state.weight;
  if (_state.weight.length() > 0 && _state.count > 0) {
    nextTotal = String(_state.weight.toFloat() * (float)_state.count, 3);
  }

  if (zone != _state.zone || nextTotal != _state.total) {
    update(
        [&](LiveWeightState& state) {
          state.zone = zone;
          LiveWeightState::refreshTotal(state);
          if (state.rangeEnabled) {
            if (zone == LIVE_WEIGHT_ZONE_LOW) {
              state.statusMessage = "UNDER target range";
            } else if (zone == LIVE_WEIGHT_ZONE_OK) {
              state.statusMessage = "IN range";
            } else if (zone == LIVE_WEIGHT_ZONE_HIGH) {
              state.statusMessage = "OVER target range";
            }
          }
          return StateUpdateResult::CHANGED;
        },
        "band");
  }

  if (!_relayBoard) {
    return;
  }

  uint8_t driveZone = _state.rangeEnabled ? zone : LIVE_WEIGHT_ZONE_NONE;
  if (driveZone == _lastDrivenZone && originId != "config") {
    return;
  }
  _lastDrivenZone = driveZone;
  _relayBoard->setWeightBandRelays(_state.relayLow, _state.relayOk, _state.relayHigh, driveZone);
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
              // Prefer UNCHANGED when line/weight identical to cut WebSocket spam
              if (state.lastLine == line && state.weight == extracted) {
                return StateUpdateResult::UNCHANGED;
              }
              state.lastLine = line;
              state.weight = extracted;
              state.timestamp = millis();
              state.activeSource = "serial";
              state.statusMessage = extracted.length() ? "Weight from serial" : "Serial line (no weight match)";
              LiveWeightState::refreshTotal(state);
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
    return String(value, 3);
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
        return String(value, 3);
      }
    }
  }

  return extractWeightSimple(line);
}

void LiveWeightService::onDiEdge(uint8_t diIndex, bool active) {
  if (!active) {
    return;
  }
  // Defer work to loop() so RelayBoard can finish DI state update without blocking on TCP/FS
  if (diIndex == 1) {
    _pendingAction = _state.di1Action;
  } else if (diIndex == 2) {
    _pendingAction = _state.di2Action;
  }
}

void LiveWeightService::runAction(const String& action) {
  String a = LiveWeightState::normalizeAction(action);
  if (a == "none") {
    return;
  }

  update(
      [&](LiveWeightState& state) {
        if (a == "next") {
          state.count++;
          LiveWeightState::refreshTotal(state);
          state.lastAction = "next";
          state.actionSeq++;
          state.statusMessage = "Next piece";
          return StateUpdateResult::CHANGED;
        }
        if (a == "start") {
          state.jobRunning = true;
          state.lastAction = "start";
          state.actionSeq++;
          state.statusMessage = "Job started";
          return StateUpdateResult::CHANGED;
        }
        if (a == "stop") {
          state.jobRunning = false;
          state.lastAction = "stop";
          state.actionSeq++;
          state.statusMessage = "Job stopped";
          return StateUpdateResult::CHANGED;
        }
        if (a == "print") {
          state.lastAction = "print";
          state.actionSeq++;
          state.statusMessage = "Print requested";
          state.printRequested = false;
          return StateUpdateResult::CHANGED;
        }
        return StateUpdateResult::UNCHANGED;
      },
      "di_action");

  if (a == "next") {
    // Persist piece count without re-opening UART
    if (configChanged()) {
      onConfigUpdated();
    }
    evaluateBandAndDrive("di_action");
  } else if (a == "print") {
    _printPending = true;
  }
}

void LiveWeightService::sendNetworkPrint() {
  String ip;
  uint16_t port = 9100;
  char ticket[384];
  bool abortPrint = false;

  update(
      [&](LiveWeightState& state) {
        state.printRequested = false;
        if (!state.printerEnabled || state.printerIp.length() == 0) {
          state.statusMessage = "Print failed — no printer IP";
          abortPrint = true;
          return StateUpdateResult::CHANGED;
        }
        ip = state.printerIp;
        port = state.printerPort ? state.printerPort : 9100;
        snprintf(ticket,
                 sizeof(ticket),
                 "================================\n"
                 "Weighsoft Live Weight\n"
                 "================================\n"
                 "PLU: %s\n"
                 "Product: %s\n"
                 "Weight: %s %s\n"
                 "Count: %lu\n"
                 "Total: %s %s\n"
                 "================================\n\n",
                 state.plu.c_str(),
                 state.product.c_str(),
                 state.weight.c_str(),
                 state.unit.c_str(),
                 (unsigned long)state.count,
                 state.total.c_str(),
                 state.unit.c_str());
        return StateUpdateResult::CHANGED;
      },
      "print_prep");

  if (abortPrint) {
    return;
  }

  WiFiClient client;
  client.setTimeout(2000);
  if (!client.connect(ip.c_str(), port)) {
    update(
        [&](LiveWeightState& state) {
          state.statusMessage = "Print failed — connect";
          return StateUpdateResult::CHANGED;
        },
        "print");
    return;
  }

  // ESC/POS: initialize, plain text, partial cut
  client.write((const uint8_t*)"\x1b\x40", 2);
  client.print(ticket);
  client.write((const uint8_t*)"\x1d\x56\x00", 3);
  client.flush();
  client.stop();

  update(
      [&](LiveWeightState& state) {
        state.statusMessage = "Print sent";
        return StateUpdateResult::CHANGED;
      },
      "print");
}

void LiveWeightService::configureMqtt() {
  if (!_mqttClient->connected()) {
    return;
  }
  String pubTopic = _mqttBasePath + "/data";
  String subTopic = _mqttBasePath + "/set";
  _mqttPubSub.configureTopics(pubTopic, subTopic);
}
