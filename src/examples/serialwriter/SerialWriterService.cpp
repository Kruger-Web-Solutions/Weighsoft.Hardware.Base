#include <examples/serialwriter/SerialWriterService.h>

#ifdef ESP32
#include <HardwareSerial.h>
#endif

SerialWriterService::SerialWriterService(AsyncWebServer* server,
                                         FS* fs,
                                         SecurityManager* securityManager,
                                         AsyncMqttClient* mqttClient
#if FT_ENABLED(FT_BLE)
                                         ,
                                         BLEServer* bleServer
#endif
                                         )
    : _httpEndpoint(SerialWriterState::read,
                    SerialWriterState::update,
                    this,
                    server,
                    SERIAL_WRITER_ENDPOINT_PATH,
                    securityManager,
                    AuthenticationPredicates::IS_AUTHENTICATED),
      _fsPersistence(SerialWriterState::readConfig,
                     SerialWriterState::updateConfig,
                     this,
                     fs,
                     SERIAL_WRITER_CONFIG_FILE),
      _mqttPubSub(SerialWriterState::read, SerialWriterState::update, this, mqttClient),
      _webSocket(SerialWriterState::read,
                 SerialWriterState::update,
                 this,
                 server,
                 SERIAL_WRITER_SOCKET_PATH,
                 securityManager,
                 AuthenticationPredicates::IS_AUTHENTICATED),
      _mqttClient(mqttClient)
#if FT_ENABLED(FT_BLE)
      ,
      _blePubSub(SerialWriterState::read, SerialWriterState::update, this, bleServer),
      _bleServer(bleServer),
      _bleService(nullptr),
      _bleCharacteristic(nullptr)
#endif
{
  _mqttBasePath = SettingValue::format("weighsoft/serialwriter/#{unique_id}");
  _mqttClient->onConnect(std::bind(&SerialWriterService::configureMqtt, this));
  _serialStarted = false;
  _suspended = false;

  // Disable FSPersistence auto-handler to prevent flash thrash on every pending_line write
  _fsPersistence.disableUpdateHandler();

  addUpdateHandler([this](const String& originId) {
    // "serial_writer_sent" is the echo-back from writePendingLine — skip to avoid feedback loop
    // "init" is from begin() — serial config applied directly there
    if (originId != "serial_writer_sent" && originId != "init") {
      onConfigUpdated();
    }
  }, false);
}

void SerialWriterService::begin() {
  _fsPersistence.readFromFS();
  Serial.printf("[SerialWriter] Loaded config: %lu baud, %u%c%u, terminator='%s'\n",
                (unsigned long)_state.baudrate,
                _state.databits,
                _state.parity == 0 ? 'N' : (_state.parity == 1 ? 'E' : 'O'),
                _state.stopbits,
                _state.lineTerminator.c_str());

  // Clear runtime state
  _state.pendingLine = "";
  _state.lastSentLine = "";
  _state.lastSentMs = 0;
  _state.sentCount = 0;
  _serialStarted = false;

  Serial.println(F("[SerialWriter] Initializing Serial1..."));
  applySerialConfig();
}

void SerialWriterService::loop() {
  if (_suspended) {
    return;
  }

  // Write any pending line to Serial1
  if (_state.pendingLine.length() > 0) {
    writePendingLine();
  }
}

void SerialWriterService::writePendingLine() {
#ifdef ESP32
  if (!_serialStarted || _suspended) {
    return;
  }

  String line = _state.pendingLine;

  // Write the line content
  SERIAL_WRITER_PORT.print(line);

  // Append the configured line terminator
  if (_state.lineTerminator == SERIAL_WRITER_TERMINATOR_LF) {
    SERIAL_WRITER_PORT.print('\n');
  } else if (_state.lineTerminator == SERIAL_WRITER_TERMINATOR_CRLF) {
    SERIAL_WRITER_PORT.print('\r');
    SERIAL_WRITER_PORT.print('\n');
  } else if (_state.lineTerminator == SERIAL_WRITER_TERMINATOR_CR) {
    SERIAL_WRITER_PORT.print('\r');
  }
  // NONE: no terminator appended

  SERIAL_WRITER_PORT.flush();

  unsigned long now = millis();
  Serial.printf("[SerialWriter] Sent: '%s'\n", line.c_str());

  // Clear pending_line and record the sent line; use special origin to skip feedback loop
  update([&](SerialWriterState& state) {
    state.pendingLine = "";
    state.lastSentLine = line;
    state.lastSentMs = now;
    state.sentCount++;
    return StateUpdateResult::CHANGED;
  }, "serial_writer_sent");
#endif
}

void SerialWriterService::writeLineWithTerminatorToUsbSerial(const String& line) const {
  Serial.print(line);
  if (_state.lineTerminator == SERIAL_WRITER_TERMINATOR_LF) {
    Serial.print('\n');
  } else if (_state.lineTerminator == SERIAL_WRITER_TERMINATOR_CRLF) {
    Serial.print('\r');
    Serial.print('\n');
  } else if (_state.lineTerminator == SERIAL_WRITER_TERMINATOR_CR) {
    Serial.print('\r');
  }
  // NONE: no terminator appended
  Serial.flush();
}

void SerialWriterService::writeLineWithTerminatorToSerial1(const String& line) const {
#ifdef ESP32
  SERIAL_WRITER_PORT.print(line);
  if (_state.lineTerminator == SERIAL_WRITER_TERMINATOR_LF) {
    SERIAL_WRITER_PORT.print('\n');
  } else if (_state.lineTerminator == SERIAL_WRITER_TERMINATOR_CRLF) {
    SERIAL_WRITER_PORT.print('\r');
    SERIAL_WRITER_PORT.print('\n');
  } else if (_state.lineTerminator == SERIAL_WRITER_TERMINATOR_CR) {
    SERIAL_WRITER_PORT.print('\r');
  }
  SERIAL_WRITER_PORT.flush();
#endif
}

void SerialWriterService::enqueueForwardedLine(const String& line, uint8_t sinkMask) {
  if (line.isEmpty() || sinkMask == 0) {
    return;
  }

  if (sinkMask & SERIAL_WRITER_FORWARDER_SINK_USB) {
    writeLineWithTerminatorToUsbSerial(line);
    Serial.printf("[SerialWriter] Forwarder sink=USB len=%u\n", (unsigned)line.length());
  }

  if (sinkMask & SERIAL_WRITER_FORWARDER_SINK_SERIAL1) {
#ifdef ESP32
    if (_suspended || !_serialStarted) {
      Serial.println(F("[SerialWriter] Forwarder sink=Serial1 skipped (UART suspended or not started; use UART "
                       "Writer mode)"));
    } else {
      writeLineWithTerminatorToSerial1(line);
      Serial.printf("[SerialWriter] Forwarder sink=Serial1 len=%u\n", (unsigned)line.length());
    }
#endif
  }

  unsigned long now = millis();
  update([&](SerialWriterState& state) {
    state.lastSentLine = line;
    state.lastSentMs = now;
    state.sentCount++;
    return StateUpdateResult::CHANGED;
  }, "serial_writer_sent");
}

void SerialWriterService::onConfigUpdated() {
  Serial.printf("[SerialWriter] Config updated - baud=%lu, terminator='%s'\n",
                (unsigned long)_state.baudrate,
                _state.lineTerminator.c_str());

  bool saved = _fsPersistence.writeToFS();
  Serial.printf("[SerialWriter] Config persisted: %s\n", saved ? "OK" : "FAILED");

  applySerialConfig();
}

uint32_t SerialWriterService::getSerialConfig() {
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

void SerialWriterService::applySerialConfig() {
#ifdef ESP32
  if (_serialStarted) {
    SERIAL_WRITER_PORT.end();
    Serial.println(F("[SerialWriter] Stopping Serial1 for reconfiguration..."));
  }

  uint32_t baud = _state.baudrate;
  if (baud < SERIAL_WRITER_MIN_BAUDRATE || baud > SERIAL_WRITER_MAX_BAUDRATE) {
    baud = SERIAL_WRITER_DEFAULT_BAUDRATE;
  }

  uint32_t config = getSerialConfig();
  SERIAL_WRITER_PORT.begin(baud, config, SERIAL_WRITER_RX_PIN, SERIAL_WRITER_TX_PIN);
  delay(100);
  _serialStarted = true;

  Serial.printf("[SerialWriter] Serial1 started: %lu baud, %u%c%u, RX=GPIO%d, TX=GPIO%d\n",
                (unsigned long)baud,
                _state.databits,
                _state.parity == 0 ? 'N' : (_state.parity == 1 ? 'E' : 'O'),
                _state.stopbits,
                SERIAL_WRITER_RX_PIN,
                SERIAL_WRITER_TX_PIN);
#endif
}

void SerialWriterService::configureMqtt() {
  if (!_mqttClient->connected()) {
    return;
  }
  String statusTopic = _mqttBasePath + "/status";
  String sendTopic = _mqttBasePath + "/send";
  _mqttPubSub.configureTopics(statusTopic, sendTopic);
  Serial.printf("[SerialWriter] MQTT configured - pub: %s, sub: %s\n",
                statusTopic.c_str(), sendTopic.c_str());
}

#if FT_ENABLED(FT_BLE)
void SerialWriterService::configureBle() {
  if (_bleServer == nullptr) {
    Serial.println(F("[SerialWriter] BLE server not available, skipping"));
    return;
  }
  Serial.println(F("[SerialWriter] Configuring BLE service..."));

  _bleService = _bleServer->createService(BLE_SERVICE_UUID);
  _bleCharacteristic = _bleService->createCharacteristic(
      BLE_CHAR_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);

  _blePubSub.configureCharacteristic(_bleCharacteristic);
  _bleService->start();

  Serial.printf("[SerialWriter] BLE configured - Service: %s, Char: %s\n", BLE_SERVICE_UUID, BLE_CHAR_UUID);
}
#endif

void SerialWriterService::suspendSerial() {
  if (_serialStarted && !_suspended) {
    Serial.println(F("[SerialWriter] Suspending - releasing Serial1"));
    SERIAL_WRITER_PORT.end();
    _serialStarted = false;
    _suspended = true;
  }
}

void SerialWriterService::resumeSerial() {
  if (_suspended) {
    Serial.println(F("[SerialWriter] Resuming - restarting Serial1"));
    _suspended = false;
    applySerialConfig();
  }
}
