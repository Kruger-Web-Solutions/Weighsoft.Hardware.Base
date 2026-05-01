#include <examples/serial/SerialService.h>
#include <examples/serial/KnownWritersService.h>
#include <regex.h>  // POSIX regex

#ifdef ESP32
#include <HardwareSerial.h>
#endif

namespace {
// Avoid flooding WebSocket/MQTT with identical lines when the scale repeats the same string faster
// than the UI can use (reduces heap pressure and async queue load; see stability plan).
#ifndef SERIAL_HW_MIN_BROADCAST_INTERVAL_MS
#define SERIAL_HW_MIN_BROADCAST_INTERVAL_MS 50
#endif

bool shouldPublishSerialHwLine(const SerialState& st, const String& line) {
  if (line.length() == 0) {
    return false;
  }
  if (st.lastLine != line) {
    return true;
  }
  return (unsigned long)(millis() - st.timestamp) >= (unsigned long)SERIAL_HW_MIN_BROADCAST_INTERVAL_MS;
}

// Idle flush without CR/LF otherwise publishes floating-RX noise as "lines" of control bytes
// (UI shows middle dots). Terminated lines still publish verbatim.
bool serialBufferHasPrintableAscii(const String& buf) {
  for (unsigned i = 0; i < buf.length(); i++) {
    char c = buf[i];
    if (c >= 32 && c < 127) {
      return true;
    }
  }
  return false;
}
}  // namespace

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
  _suspended = false;  // Not suspended initially

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

void SerialService::enqueueCompletedLine(const String& line) {
  if (line.length() == 0) {
    return;
  }
  while (_rxCompleteLineQueue.size() >= SERIAL_MAX_QUEUED_COMPLETE_LINES) {
    _rxCompleteLineQueue.pop_front();
  }
  _rxCompleteLineQueue.push_back(line);
}

void SerialService::drainRxLineQueue() {
  if (_rxCompleteLineQueue.empty()) {
    return;
  }
  const unsigned long now = millis();
  const unsigned long minGap = 1000UL / (unsigned long)SERIAL_MAX_COMPLETE_LINES_PER_SEC;
  if (_lastRxLinePublishMs != 0U && (unsigned long)(now - _lastRxLinePublishMs) < minGap) {
    return;
  }
  const String line = _rxCompleteLineQueue.front();
  _rxCompleteLineQueue.pop_front();
  const String extracted = extractWeight(line);
  update([&](SerialState& state) {
    state.lastLine = line;
    state.weight = extracted;
    state.timestamp = now;
    return StateUpdateResult::CHANGED;
  }, "serial_hw");
  _lastRxLinePublishMs = now;
  if (_knownWriters) _knownWriters->recordBroadcastToOnlineWriters(line);
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
  _rxCompleteLineQueue.clear();
  _lastRxLinePublishMs = 0;

  // Start Serial1 with loaded config
  Serial.println(F("[Serial] Initializing Serial1..."));
  applySerialConfig();
}

// If the scale sends frames without CR/LF, commit the partial line after this many ms idle on UART.
#ifndef SERIAL_LINE_IDLE_FLUSH_MS
#define SERIAL_LINE_IDLE_FLUSH_MS 500
#endif

void SerialService::loop() {
  // Snapshot for REST (debug): classify empty last_line without USB serial monitor
  _state.dbgSuspended = _suspended ? 1 : 0;
  _state.dbgSerialStarted = _serialStarted ? 1 : 0;
  _state.dbgLineBufLen = (uint16_t)(_lineBuffer.length() > 65535 ? 65535 : _lineBuffer.length());
  if (!_suspended && _serialStarted) {
    int a = SERIAL_PORT.available();
    _state.dbgUartRxAvail = a > 32000 ? 32000 : (a < 0 ? 0 : a);
  } else {
    _state.dbgUartRxAvail = -1;
  }

  // Skip reading if suspended (DiagnosticsService is using Serial1)
  if (_suspended) {
    return;
  }

  // Debug: Show incoming data periodically
  static unsigned long lastDebug = 0;
  static String debugBuffer = "";
  static unsigned long lastSerialRxMs = 0;

  // Heartbeat: confirm loop is running and show Serial1 status
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat >= 5000) {
    int avail = SERIAL_PORT.available();
    Serial.printf("[Serial] Heartbeat: loop running, suspended=%d, serialStarted=%d, available=%d\n",
                  _suspended, _serialStarted, avail);
    lastHeartbeat = millis();
  }
  
  // Read serial data and assemble lines
  while (SERIAL_PORT.available()) {
    char c = SERIAL_PORT.read();
    lastSerialRxMs = millis();
    
    // Collect data for debug output
    if (debugBuffer.length() < 200) {
      if (c >= 32 && c < 127) {
        debugBuffer += c;  // Printable ASCII
      } else {
        char hex[5];
        sprintf(hex, "\\x%02X", (uint8_t)c);
        debugBuffer += hex;
      }
    }

    // Line assembly (accepts both \r and \n as terminators)
    if (c == '\n' || c == '\r') {
      if (_lineBuffer.length() > 0) {
        if (shouldPublishSerialHwLine(_state, _lineBuffer)) {
          enqueueCompletedLine(_lineBuffer);
        }
      }
      _lineBuffer = "";
    } else {
      _lineBuffer += c;
      // Prevent memory overflow - larger buffer for industrial scales
      if (_lineBuffer.length() > 2048) {
        Serial.printf("[Serial] WARNING: Line exceeded 2048 chars (first 100): %.100s...\n", 
                      _lineBuffer.c_str());
        _lineBuffer = "";
      }
    }
  }

  // Idle flush: frames without CR/LF still publish after UART idle (baud/noise may look like control bytes;
  // REST/WebSocket sanitize for display). Printable-only gating was reverted after logs showed all-zero
  // last_line when peripherals send delayed ASCII or binary prelude without newline.
  if (_lineBuffer.length() > 0 && !SERIAL_PORT.available() && lastSerialRxMs > 0 &&
      (unsigned long)(millis() - lastSerialRxMs) >= SERIAL_LINE_IDLE_FLUSH_MS) {
    if (serialBufferHasPrintableAscii(_lineBuffer)) {
      if (shouldPublishSerialHwLine(_state, _lineBuffer)) {
        enqueueCompletedLine(_lineBuffer);
      }
    }
    _lineBuffer = "";
    lastSerialRxMs = 0;
  }

  drainRxLineQueue();

  // Print debug sample every 2 seconds
  if (millis() - lastDebug >= 2000 && debugBuffer.length() > 0) {
    Serial.printf("[Serial] Sample (first 200 chars): %s\n", debugBuffer.c_str());
    Serial.printf("[Serial] Buffer length: %d, Current baud: %lu\n", 
                  _lineBuffer.length(), (unsigned long)_state.baudrate);
    debugBuffer = "";
    lastDebug = millis();
  }
}

void SerialService::onConfigUpdated() {
  Serial.printf("[Serial] Config update triggered - baud=%lu, regex='%s'\n",
                (unsigned long)_state.baudrate, _state.regexPattern.c_str());
  
  // Persist config to flash
  bool saved = _fsPersistence.writeToFS();
  Serial.printf("[Serial] Config persisted to flash: %s\n", saved ? "SUCCESS" : "FAILED");
  
  // Apply new serial configuration
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

String SerialService::extractWeight(const String& line) {
  const String& pattern = _state.regexPattern;
  if (pattern.length() == 0) {
    return "";
  }
  
  // Use POSIX regex for pattern matching
  regex_t regex;
  regmatch_t matches[2]; // 0 = full match, 1 = first capture group
  
  // Compile the regex pattern
  int reti = regcomp(&regex, pattern.c_str(), REG_EXTENDED);
  if (reti != 0) {
    char error_buffer[100];
    regerror(reti, &regex, error_buffer, sizeof(error_buffer));
    Serial.printf("[Serial] Regex compile error: %s\n", error_buffer);
    return "";
  }
  
  // Execute the regex match
  reti = regexec(&regex, line.c_str(), 2, matches, 0);
  regfree(&regex);
  
  if (reti == 0) {
    // Match found - extract first capture group if present, otherwise full match
    int start = matches[1].rm_so >= 0 ? matches[1].rm_so : matches[0].rm_so;
    int end = matches[1].rm_eo >= 0 ? matches[1].rm_eo : matches[0].rm_eo;
    
    if (start >= 0 && end > start) {
      String extracted = line.substring(start, end);
      
      // Replace comma with period for locales that use comma as decimal separator
      extracted.replace(',', '.');
      
      // Convert to float and back to clean up (remove leading zeros, standardize format)
      float value = extracted.toFloat();
      
      // Only return if we actually got a valid number
      if (value != 0.0f || extracted.indexOf('0') >= 0) {
        return String(value, 2); // Return with 2 decimal places
      }
    }
  } else if (reti == REG_NOMATCH) {
    // No match - this is normal, not an error
  } else {
    char error_buffer[100];
    regerror(reti, &regex, error_buffer, sizeof(error_buffer));
    Serial.printf("[Serial] Regex match error: %s\n", error_buffer);
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
    _rxCompleteLineQueue.clear();
    _lastRxLinePublishMs = 0;
  }
}

void SerialService::resumeSerial() {
  if (_suspended) {
    Serial.println(F("[Serial] Resuming - restarting Serial1"));
    _suspended = false;
    applySerialConfig();
  }
}
