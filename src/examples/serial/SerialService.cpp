#include <examples/serial/SerialService.h>
#include <regex.h>  // POSIX regex

#ifdef ESP32
#include <HardwareSerial.h>
#endif

SerialService::SerialService(AsyncWebServer* server,
                             FS* fs,
                             SecurityManager* securityManager,
                             AsyncMqttClient* mqttClient
#if FT_ENABLED(FT_BLE)
                             ,BLEServer* bleServer
#endif
                             ) :
    _httpEndpoint(SerialState::read,
                  SerialState::update,
                  this,
                  server,
                  SERIAL_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED),
    _fsPersistence(SerialState::readConfig,
                   SerialState::updateConfig,
                   this,
                   fs,
                   SERIAL_CONFIG_FILE),
    _mqttPubSub(SerialState::read, SerialState::update, this, mqttClient),
    _webSocket(SerialState::read,
               SerialState::update,
               this,
               server,
               SERIAL_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_AUTHENTICATED),
    _mqttClient(mqttClient)
#if FT_ENABLED(FT_BLE)
    ,_blePubSub(SerialState::read, SerialState::update, this, bleServer),
    _bleServer(bleServer),
    _bleService(nullptr),
    _bleCharacteristic(nullptr)
#endif
{
  _mqttBasePath = SettingValue::format("weighsoft/serial/#{unique_id}");
  _mqttName = SettingValue::format("serial-monitor-#{unique_id}");
  _mqttUniqueId = SettingValue::format("serial-#{unique_id}");
  _mqttClient->onConnect(std::bind(&SerialService::configureMqtt, this));
  _serialStarted = false;
  _suspended = false;
  _regexCompiled = false;
  _compiledPattern = "";

  // Disable FSPersistence auto-handler: serial_hw updates fire ~2x/sec and would
  // thrash flash writes, risking filesystem corruption.  We persist explicitly
  // in onConfigUpdated() instead (only on actual config changes).
  _fsPersistence.disableUpdateHandler();

  addUpdateHandler([this](const String& originId) {
    // Skip "serial_hw" (data from scale) and "init" (begin() will call applySerialConfig() itself)
    if (originId != "serial_hw" && originId != "init") {
      onConfigUpdated();
    }
  }, false);
}

void SerialService::begin() {
  // Load persisted config from flash (baud_rate, data_bits, stop_bits, parity, regex_pattern)
  _fsPersistence.readFromFS();
  Serial.printf("[Serial] Loaded config: %lu baud, %u%c%u, regex='%s'\n",
                (unsigned long)_state.baudrate, _state.databits,
                _state.parity == 0 ? 'N' : (_state.parity == 1 ? 'E' : 'O'),
                _state.stopbits, _state.regexPattern.c_str());

  // Clear runtime data (not persisted)
  _state.lastLine = "";
  _state.weight = "";
  _state.timestamp = 0;
  _lineBuffer = "";
  _serialStarted = false;

  // Start Serial1 with loaded config
  Serial.println(F("[Serial] Initializing Serial1..."));
  applySerialConfig();
}

void SerialService::loop() {
  if (_suspended) {
    return;
  }

  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat >= 30000) {
    Serial.printf("[Serial] Heartbeat: heap=%lu, serialStarted=%d\n",
                  (unsigned long)ESP.getFreeHeap(), _serialStarted);
    lastHeartbeat = millis();
  }

  int bytesRead = 0;
  while (SERIAL_PORT.available() && bytesRead < 512) {
    char c = SERIAL_PORT.read();
    bytesRead++;

    if (c == '\n' || c == '\r') {
      if (_lineBuffer.length() > 0) {
        String extracted = extractWeight(_lineBuffer);
        update([&](SerialState& state) {
          state.lastLine = _lineBuffer;
          state.weight = extracted;
          state.timestamp = millis();
          return StateUpdateResult::CHANGED;
        }, "serial_hw");
      }
      _lineBuffer = "";
    } else {
      _lineBuffer += c;
      if (_lineBuffer.length() > 512) {
        _lineBuffer = "";
      }
    }
  }
}

void SerialService::onConfigUpdated() {
  Serial.printf("[Serial] Config update triggered - baud=%lu, regex='%s'\n",
                (unsigned long)_state.baudrate, _state.regexPattern.c_str());

  bool saved = _fsPersistence.writeToFS();
  Serial.printf("[Serial] Config persisted to flash: %s\n", saved ? "SUCCESS" : "FAILED");

  // Force regex recompile on next extractWeight() call
  if (_state.regexPattern != _compiledPattern) {
    compileRegex(_state.regexPattern);
  }

  applySerialConfig();
}

uint32_t SerialService::getSerialConfig() {
#ifdef ESP32
  uint8_t d = _state.databits;
  uint8_t p = _state.parity;
  uint8_t s = _state.stopbits;
  if (d < 7) d = 7;
  if (d > 8) d = 8;
  if (s < 1) s = 1;
  if (s > 2) s = 2;
  if (p > 2) p = 0;
  if (d == 7) {
    if (p == 0) return s == 1 ? SERIAL_7N1 : SERIAL_7N2;
    if (p == 1) return s == 1 ? SERIAL_7E1 : SERIAL_7E2;
    return s == 1 ? SERIAL_7O1 : SERIAL_7O2;
  }
  if (p == 0) return s == 1 ? SERIAL_8N1 : SERIAL_8N2;
  if (p == 1) return s == 1 ? SERIAL_8E1 : SERIAL_8E2;
  return s == 1 ? SERIAL_8O1 : SERIAL_8O2;
#else
  return SERIAL_8N1;
#endif
}

void SerialService::applySerialConfig() {
#ifdef ESP32
  if (_serialStarted) {
    SERIAL_PORT.end();
    Serial.println(F("[Serial] Stopping Serial1 for reconfiguration..."));
  }
  
  uint32_t baud = _state.baudrate;
  if (baud < SERIAL_MIN_BAUDRATE || baud > SERIAL_MAX_BAUDRATE) {
    baud = SERIAL_DEFAULT_BAUDRATE;
  }
  
  uint32_t config = getSerialConfig();
  SERIAL_PORT.begin(baud, config, SERIAL2_RX_PIN, SERIAL2_TX_PIN);
  delay(100);  // Allow hardware to stabilize
  _serialStarted = true;
  
  Serial.printf("[Serial] Serial1 started: %lu baud, %u%c%u, RX=GPIO%d, TX=GPIO%d\n",
                (unsigned long)baud, _state.databits,
                _state.parity == 0 ? 'N' : (_state.parity == 1 ? 'E' : 'O'),
                _state.stopbits, SERIAL2_RX_PIN, SERIAL2_TX_PIN);
#endif
}

void SerialService::compileRegex(const String& pattern) {
  freeRegex();
  if (pattern.length() == 0) return;

  int reti = regcomp(&_compiledRegex, pattern.c_str(), REG_EXTENDED);
  if (reti != 0) {
    char buf[100];
    regerror(reti, &_compiledRegex, buf, sizeof(buf));
    Serial.printf("[Serial] Regex compile error: %s\n", buf);
    regfree(&_compiledRegex);
    return;
  }
  _regexCompiled = true;
  _compiledPattern = pattern;
  Serial.printf("[Serial] Regex compiled OK: '%s'\n", pattern.c_str());
}

void SerialService::freeRegex() {
  if (_regexCompiled) {
    regfree(&_compiledRegex);
    _regexCompiled = false;
    _compiledPattern = "";
  }
}

String SerialService::extractWeight(const String& line) {
  const String& pattern = _state.regexPattern;
  if (pattern.length() == 0) return "";

  // Recompile only when pattern changes
  if (!_regexCompiled || pattern != _compiledPattern) {
    compileRegex(pattern);
  }
  if (!_regexCompiled) return "";

  regmatch_t matches[2];
  int reti = regexec(&_compiledRegex, line.c_str(), 2, matches, 0);

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
  return "";
}

void SerialService::readSerial() {
  // Kept for API compatibility but logic is now in loop() for diagnostics
}

void SerialService::configureMqtt() {
  if (!_mqttClient->connected()) {
    return;
  }
  String pubTopic = _mqttBasePath + "/data";
  String subTopic = "";
  _mqttPubSub.configureTopics(pubTopic, subTopic);
  Serial.printf("[Serial] MQTT configured - topic: %s\n", pubTopic.c_str());
}

#if FT_ENABLED(FT_BLE)
void SerialService::configureBle() {
  if (_bleServer == nullptr) {
    Serial.println("[Serial] BLE server not available, skipping BLE configuration");
    return;
  }
  Serial.println("[Serial] Configuring BLE service...");
  _bleService = _bleServer->createService(BLE_SERVICE_UUID);
  _bleCharacteristic = _bleService->createCharacteristic(
      BLE_CHAR_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_NOTIFY
  );
  _blePubSub.configureCharacteristic(_bleCharacteristic);
  _bleService->start();
  Serial.printf("[Serial] BLE service configured - Service UUID: %s, Char UUID: %s\n",
                BLE_SERVICE_UUID, BLE_CHAR_UUID);
}
#endif

// === Coordination with DiagnosticsService ===

void SerialService::suspendSerial() {
  if (_serialStarted && !_suspended) {
    Serial.println(F("[Serial] Suspending - DiagnosticsService taking control of Serial1"));
    SERIAL_PORT.end();
    _serialStarted = false;
    _suspended = true;
  }
}

void SerialService::resumeSerial() {
  if (_suspended) {
    Serial.println(F("[Serial] Resuming - restarting Serial1"));
    _suspended = false;
    applySerialConfig();
  }
}
