#include <examples/diagnostics/DiagnosticsService.h>

#ifdef ESP32
#include <HardwareSerial.h>
#endif

// Loopback test sends packet every X ms
#define LOOPBACK_INTERVAL_MS 100
// Baud scan: wait X ms per rate for data
#define BAUD_SCAN_TIMEOUT_MS 500
// Signal quality: delay between packets (50ms = 20 packets/sec, reduces buffer overflow)
#define SIGNAL_TEST_INTERVAL_MS 50
// WebSocket update throttle (prevent queue overflow)
#define WS_UPDATE_THROTTLE_MS 500

DiagnosticsService::DiagnosticsService(AsyncWebServer* server, SecurityManager* securityManager) :
    _httpEndpoint(DiagnosticsState::read,
                  DiagnosticsState::update,
                  this,
                  server,
                  DIAGNOSTICS_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED),
    _webSocket(DiagnosticsState::read,
               DiagnosticsState::update,
               this,
               server,
               DIAGNOSTICS_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_AUTHENTICATED)
{
  _serialStarted = false;
  _lastTestTime = 0;
  _loopbackLastSend = 0;
  _baudTestStartTime = 0;
  _signalTestStartTime = 0;
  _latencyBuffer = nullptr;
  _latencyBufferSize = 0;
  _lastWsBroadcast = 0;
}

void DiagnosticsService::begin() {
  Serial.println(F("[Diagnostics] Initializing UART diagnostic service..."));
  
  // Initialize state
  _state.loopbackEnabled = false;
  _state.loopbackStatus = "idle";
  _state.loopbackTxCount = 0;
  _state.loopbackRxCount = 0;
  _state.loopbackErrorCount = 0;
  _state.loopbackLastTest = "";
  _state.loopbackLastReceived = "";
  _state.loopbackStartTime = 0;

  _state.baudScanEnabled = false;
  _state.baudScanStatus = "idle";
  _state.baudDetected = 0;
  _state.baudCurrentIndex = 0;
  _state.baudTestPackets = 0;

  _state.signalTestEnabled = false;
  _state.signalStatus = "idle";
  _state.signalQuality = 0;
  _state.signalTotalPackets = 1000;
  _state.signalSentPackets = 0;
  _state.signalReceivedPackets = 0;
  _state.signalAvgLatency = 0.0f;
  _state.signalJitter = 0.0f;
  _state.signalErrorCount = 0;

  _rxBuffer = "";
  Serial.println(F("[Diagnostics] Ready. GPIO18 (RX) / GPIO17 (TX)"));
}

void DiagnosticsService::stopAllTests() {
  Serial.println(F("[Diagnostics] Stopping all tests"));
  
  _state.loopbackEnabled = false;
  _state.baudScanEnabled = false;
  _state.signalTestEnabled = false;
  
  if (_serialStarted) {
    stopSerial();
  }
  
  // Clean up any allocated memory
  if (_latencyBuffer) {
    delete[] _latencyBuffer;
    _latencyBuffer = nullptr;
  }
  
  update([](DiagnosticsState& state) { return StateUpdateResult::CHANGED; }, "mode_switch");
}

void DiagnosticsService::loop() {
  // Check if any test was just disabled - release Serial1
  static bool wasLoopbackActive = false;
  static bool wasBaudScanActive = false;
  static bool wasSignalTestActive = false;
  
  bool anyTestActive = _state.loopbackEnabled || _state.baudScanEnabled || _state.signalTestEnabled;
  bool anyTestWasActive = wasLoopbackActive || wasBaudScanActive || wasSignalTestActive;
  
  // If all tests stopped, release Serial1
  if (!anyTestActive && anyTestWasActive && _serialStarted) {
    Serial.println(F("[Diagnostics] All tests stopped - releasing Serial1"));
    stopSerial();
  }
  
  // Run active tests
  if (_state.loopbackEnabled) {
    runLoopbackTest();
  }
  if (_state.baudScanEnabled) {
    runBaudScan();
  }
  if (_state.signalTestEnabled) {
    runSignalQualityTest();
  }
  
  // Update state tracking
  wasLoopbackActive = _state.loopbackEnabled;
  wasBaudScanActive = _state.baudScanEnabled;
  wasSignalTestActive = _state.signalTestEnabled;
}

void DiagnosticsService::startSerial(uint32_t baud) {
  if (_serialStarted) {
    DIAG_SERIAL_PORT.end();
  }
  // Increase RX buffer from default 256 to 1024 bytes to handle bursts
  DIAG_SERIAL_PORT.setRxBufferSize(1024);
  DIAG_SERIAL_PORT.begin(baud, SERIAL_8N1, DIAG_RX_PIN, DIAG_TX_PIN);
  delay(100);  // ESP32-S3: Allow hardware to stabilize after Serial1 init
  _serialStarted = true;
  Serial.printf("[Diagnostics] Serial1 started: %lu baud, GPIO18 (RX), GPIO17 (TX), RX buffer=1024\n", (unsigned long)baud);
}

void DiagnosticsService::stopSerial() {
  if (_serialStarted) {
    DIAG_SERIAL_PORT.end();
    _serialStarted = false;
    Serial.println(F("[Diagnostics] Serial1 stopped"));
  }
}

String DiagnosticsService::readSerialLine() {
  while (DIAG_SERIAL_PORT.available()) {
    char c = DIAG_SERIAL_PORT.read();
    if (c == '\n' || c == '\r') {
      if (_rxBuffer.length() > 0) {
        String line = _rxBuffer;
        _rxBuffer = "";
        return line;
      }
    } else {
      _rxBuffer += c;
      if (_rxBuffer.length() > 256) {
        _rxBuffer = "";  // Overflow protection
      }
    }
  }
  return "";
}

void DiagnosticsService::runLoopbackTest() {
  // Start serial if not started
  if (!_serialStarted) {
    startSerial(115200);
    _loopbackLastSend = millis();
    _lastWsBroadcast = millis();
  }

  // Send test packet every LOOPBACK_INTERVAL_MS
  if (millis() - _loopbackLastSend >= LOOPBACK_INTERVAL_MS) {
    _state.loopbackTxCount++;
    // Use static buffer to avoid String memory fragmentation
    char testMsg[32];
    snprintf(testMsg, sizeof(testMsg), "TEST:%lu", (unsigned long)_state.loopbackTxCount);
    DIAG_SERIAL_PORT.println(testMsg);
    _state.loopbackLastTest = testMsg;
    _loopbackLastSend = millis();
  }

  // Read any incoming data
  String line = readSerialLine();
  if (line.length() > 0) {
    _state.loopbackRxCount++;
    _state.loopbackLastReceived = line;
    
    // Check if it matches expected
    if (line != _state.loopbackLastTest) {
      _state.loopbackErrorCount++;
    }

    // Update status
    float successRate = (float)(_state.loopbackRxCount - _state.loopbackErrorCount) / _state.loopbackTxCount * 100.0f;
    if (successRate >= 95.0f) {
      _state.loopbackStatus = "pass";
    } else {
      _state.loopbackStatus = "fail";
    }
  }
  
  // Throttled WebSocket broadcast (only every WS_UPDATE_THROTTLE_MS)
  if (millis() - _lastWsBroadcast >= WS_UPDATE_THROTTLE_MS) {
    update([](DiagnosticsState& state) { return StateUpdateResult::CHANGED; }, "diag_hw");
    _lastWsBroadcast = millis();
  }
}

void DiagnosticsService::runBaudScan() {
  // Initialize scan if just started
  if (_baudTestStartTime == 0) {
    Serial.println(F("[Diagnostics] === STARTING BAUD SCAN ==="));
    Serial.println(F("[Diagnostics] Ensure your device is connected:"));
    Serial.println(F("[Diagnostics]   Device TX -> ESP32 GPIO18 (RX)"));
    Serial.println(F("[Diagnostics]   Device GND -> ESP32 GND"));
    Serial.println(F("[Diagnostics]   Device must be actively transmitting data!"));

    _baudTestStartTime = millis();
    _lastWsBroadcast = millis();
    _state.baudCurrentIndex = 0;
    _state.baudTestPackets = 0;
    _rxBuffer = "";
    startSerial(DIAG_BAUD_RATES[_state.baudCurrentIndex]);
    Serial.printf("[Diagnostics] Baud scan: testing %lu baud...\n", 
                  (unsigned long)DIAG_BAUD_RATES[_state.baudCurrentIndex]);
    update([](DiagnosticsState& state) { return StateUpdateResult::CHANGED; }, "diag_hw");
  }

  // Read any data at current baud rate (check hardware buffer status)
  int available = DIAG_SERIAL_PORT.available();
  
  // Periodic status update every 500ms
  static unsigned long lastStatusLog = 0;
  if (millis() - lastStatusLog >= 500) {
    Serial.printf("[Diagnostics] Baud scan status: testing %lu baud, %d bytes in buffer, %u packets received so far\n",
                  (unsigned long)DIAG_BAUD_RATES[_state.baudCurrentIndex], available, _state.baudTestPackets);
    lastStatusLog = millis();
  }
  
  if (available > 0) {
    Serial.printf("[Diagnostics] Baud scan: %d bytes available in buffer at %lu baud\n", 
                  available, (unsigned long)DIAG_BAUD_RATES[_state.baudCurrentIndex]);
  }
  
  String line = readSerialLine();
  if (line.length() > 0) {
    _state.baudTestPackets++;
    Serial.printf("[Diagnostics] Baud scan: received LINE at %lu baud (packet %u): '%s' (%d chars)\n",
                  (unsigned long)DIAG_BAUD_RATES[_state.baudCurrentIndex], _state.baudTestPackets, 
                  line.c_str(), line.length());
    
    // If we get 3+ valid packets, assume this is the correct baud
    if (_state.baudTestPackets >= 3) {
      _state.baudDetected = DIAG_BAUD_RATES[_state.baudCurrentIndex];
      _state.baudScanStatus = "found";
      _state.baudScanEnabled = false;
      stopSerial();
      _baudTestStartTime = 0;
      Serial.printf("[Diagnostics] Baud scan: DETECTED %lu baud!\n", (unsigned long)_state.baudDetected);
      update([](DiagnosticsState& state) { return StateUpdateResult::CHANGED; }, "diag_hw");
      return;
    }
  }

  // Timeout for current baud rate
  if (millis() - _baudTestStartTime >= BAUD_SCAN_TIMEOUT_MS) {
    Serial.printf("[Diagnostics] Baud scan: no data at %lu baud\n", 
                  (unsigned long)DIAG_BAUD_RATES[_state.baudCurrentIndex]);
    
    // Move to next baud rate
    _state.baudCurrentIndex++;
    _state.baudTestPackets = 0;
    
    if (_state.baudCurrentIndex >= DIAG_BAUD_RATES_COUNT) {
      // Scan complete - not found
      _state.baudScanStatus = "not_found";
      _state.baudScanEnabled = false;
      stopSerial();
      _baudTestStartTime = 0;
      Serial.println(F("[Diagnostics] Baud scan: NOT FOUND (no data at any rate)"));
      update([](DiagnosticsState& state) { return StateUpdateResult::CHANGED; }, "diag_hw");
    } else {
      // Try next rate
      _baudTestStartTime = millis();
      _rxBuffer = "";
      startSerial(DIAG_BAUD_RATES[_state.baudCurrentIndex]);
      Serial.printf("[Diagnostics] Baud scan: testing %lu baud...\n", 
                    (unsigned long)DIAG_BAUD_RATES[_state.baudCurrentIndex]);
      update([](DiagnosticsState& state) { return StateUpdateResult::CHANGED; }, "diag_hw");
    }
  }
}

void DiagnosticsService::runSignalQualityTest() {
  // Initialize test if just started
  if (_signalTestStartTime == 0) {
    _signalTestStartTime = millis();
    _lastWsBroadcast = millis();
    _state.signalSentPackets = 0;
    _state.signalReceivedPackets = 0;
    _state.signalErrorCount = 0;
    _rxBuffer = "";
    
    // Allocate latency buffer for jitter calculation (limit to 500 to save memory)
    _latencyBufferSize = min(_state.signalTotalPackets, (uint32_t)500);
    _latencyBuffer = new float[_latencyBufferSize];
    
    startSerial(115200);
    Serial.printf("[Diagnostics] Signal quality test: %u packets\n", _state.signalTotalPackets);
    update([](DiagnosticsState& state) { return StateUpdateResult::CHANGED; }, "diag_hw");
  }

  // Send next packet
  if (_state.signalSentPackets < _state.signalTotalPackets && 
      millis() - _lastTestTime >= SIGNAL_TEST_INTERVAL_MS) {
    unsigned long sendTime = micros();
    // Use printf to avoid String concatenation memory fragmentation
    DIAG_SERIAL_PORT.printf("SIG:%lu:%lu\n", (unsigned long)_state.signalSentPackets, sendTime);
    _state.signalSentPackets++;
    _lastTestTime = millis();
  }
  
  // Throttled WebSocket broadcast for progress updates
  if (millis() - _lastWsBroadcast >= WS_UPDATE_THROTTLE_MS) {
    update([](DiagnosticsState& state) { return StateUpdateResult::CHANGED; }, "diag_hw");
    _lastWsBroadcast = millis();
  }

  // Read ALL available incoming data (not just one line) to prevent buffer overflow
  while (true) {
    String line = readSerialLine();
    if (line.length() == 0) break;  // No more complete lines
    
    unsigned long receiveTime = micros();
    _state.signalReceivedPackets++;
    
    // Parse: "SIG:123:1234567890"
    int colon1 = line.indexOf(':');
    int colon2 = line.indexOf(':', colon1 + 1);
    if (colon1 > 0 && colon2 > colon1) {
      String seqStr = line.substring(colon1 + 1, colon2);
      String timeStr = line.substring(colon2 + 1);
      unsigned long sendTime = timeStr.toInt();
      unsigned long latency = receiveTime - sendTime;
      float latencyMs = latency / 1000.0f;
      
      // Store latency for jitter calculation
      uint32_t idx = _state.signalReceivedPackets - 1;
      if (idx < _latencyBufferSize) {
        _latencyBuffer[idx] = latencyMs;
      }
    } else {
      _state.signalErrorCount++;
    }
  }

  // Check if test complete
  if (_state.signalSentPackets >= _state.signalTotalPackets && 
      (millis() - _lastTestTime >= 1000 || _state.signalReceivedPackets >= _state.signalTotalPackets)) {
    // Calculate final metrics
    calculateSignalQuality();
    
    _state.signalStatus = "complete";
    _state.signalTestEnabled = false;
    stopSerial();
    _signalTestStartTime = 0;
    
    // Clean up
    if (_latencyBuffer) {
      delete[] _latencyBuffer;
      _latencyBuffer = nullptr;
    }
    
    Serial.printf("[Diagnostics] Signal quality test complete: %u%% quality\n", _state.signalQuality);
    update([](DiagnosticsState& state) { return StateUpdateResult::CHANGED; }, "diag_hw");
  }
}

void DiagnosticsService::calculateSignalQuality() {
  uint32_t received = _state.signalReceivedPackets;
  uint32_t sent = _state.signalSentPackets;
  
  if (sent == 0) {
    _state.signalQuality = 0;
    return;
  }

  // Calculate average latency
  float totalLatency = 0.0f;
  uint32_t validLatencies = received < (uint32_t)_latencyBufferSize ? received : (uint32_t)_latencyBufferSize;
  for (uint32_t i = 0; i < validLatencies; i++) {
    totalLatency += _latencyBuffer[i];
  }
  _state.signalAvgLatency = validLatencies > 0 ? totalLatency / validLatencies : 0.0f;

  // Calculate jitter (standard deviation of latency)
  float variance = 0.0f;
  for (uint32_t i = 0; i < validLatencies; i++) {
    float diff = _latencyBuffer[i] - _state.signalAvgLatency;
    variance += diff * diff;
  }
  _state.signalJitter = validLatencies > 0 ? sqrt(variance / validLatencies) : 0.0f;

  // Quality = (packets received - errors) / packets sent * 100
  uint32_t successful = received - _state.signalErrorCount;
  _state.signalQuality = (uint8_t)((float)successful / sent * 100.0f);
}
