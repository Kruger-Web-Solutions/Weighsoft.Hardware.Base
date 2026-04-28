#include <examples/weightforwarder/WeightForwarderService.h>
#include <examples/serial/SerialState.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <Esp.h>

#ifdef ESP32
#include <WiFi.h>
#endif

namespace {
inline bool wfHeapBelowThreshold() {
  return ESP.getFreeHeap() < WEIGHT_FORWARDER_MIN_FREE_HEAP;
}
}  // namespace

WeightForwarderService::WeightForwarderService(AsyncWebServer* server,
                                               FS* fs,
                                               SecurityManager* securityManager,
                                               AsyncMqttClient* mqttClient,
                                               SerialService* serialService) :
    _server(server),
    _securityManager(securityManager),
    _httpEndpoint(WeightForwarderState::read,
                  WeightForwarderState::update,
                  this,
                  server,
                  WEIGHT_FORWARDER_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED),
    _fsPersistence(WeightForwarderState::readConfig,
                   WeightForwarderState::updateConfig,
                   this,
                   fs,
                   WEIGHT_FORWARDER_CONFIG_FILE),
    _webSocket(WeightForwarderState::read,
               WeightForwarderState::update,
               this,
               server,
               WEIGHT_FORWARDER_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_AUTHENTICATED),
    _mqttClient(mqttClient),
    _serialService(serialService)
#ifdef ESP32
    ,_wsClient(nullptr),
    _lastWsReconnectAttempt(0)
#endif
#if FT_ENABLED(FT_BLE)
    ,_bleClient(nullptr),
    _bleRemoteChar(nullptr),
    _lastBleReconnectAttempt(0)
#endif
    ,_lastForwardTime(0) {
  clearAuthTokens();
  // Disable FSPersistence auto-handler: internal status updates (connection state,
  // errors, forward time) fire frequently and would thrash flash writes.
  // We persist explicitly in onConfigUpdated() instead.
  _fsPersistence.disableUpdateHandler();

  // Register update handler to listen for SerialService weight updates
  _serialService->addUpdateHandler([this](const String& originId) { onSerialWeightUpdate(originId); }, false);

  // Register update handler for config changes (persists to flash)
  addUpdateHandler([this](const String& originId) {
    // Only persist on user-initiated config changes, not internal status updates
    if (originId != "internal") {
      onConfigUpdated();
    }
  }, false);
}

void WeightForwarderService::begin() {
  // Load persisted config from flash
  _fsPersistence.readFromFS();
  Serial.printf("[WeightForwarder] Loaded config: protocol=%d, enabled=%d, targets=%d\n",
                (int)_state.protocol, _state.enabled, _state.targetUrlCount);

  _state.connected = false;
  _state.lastError = "";
  _state.lastForwardTime = 0;
  _pendingWeight = "";
  _pendingLine = "";
  _weightLastChangedAt = 0;
  _lastForwardedWeight = "";
  clearAuthTokens();

  // Push USB-echo flag down to SerialService so per-line echo runs
  // continuously regardless of whether the scale value changed.
  if (_serialService) {
    _serialService->setUsbEchoEnabled(_state.usbEchoEnabled);
  }

  // Initialize protocol if enabled
  if (_state.enabled) {
    switchProtocol(_state.protocol);
  }

  _minHeapSeen = ESP.getFreeHeap();
  _lastPeriodicMillis = millis();
  registerAuditEndpoint();
  recordAudit(WeightForwarderAuditReason::BOOT);
  Serial.printf("[WeightForwarder] Service ready — enabled=%d freeHeap=%lu\n",
                _state.enabled, (unsigned long)ESP.getFreeHeap());
}

void WeightForwarderService::loop() {
  // Periodic 30 s heap snapshot. Mirrors the RemoteWeight pattern so a leak
  // shows up in /rest/weightForwarderAudit instead of only after a crash.
  unsigned long now = millis();
  if (now - _lastPeriodicMillis >= 30000UL) {
    _lastPeriodicMillis = now;
    uint32_t h = ESP.getFreeHeap();
    if (h < _minHeapSeen) _minHeapSeen = h;
    recordAudit(WeightForwarderAuditReason::PERIODIC);
    Serial.printf("[WeightForwarder] heap=%lu min=%lu fwd=%u dropped=%u failed=%u\n",
                  (unsigned long)h, (unsigned long)_minHeapSeen,
                  _fwdTotal, _fwdDropped, _fwdFailed);
  }

#ifdef ESP32
  if (_wsClient) {
    _wsClient->loop();
    checkWsConnection();
  }
#endif

#if FT_ENABLED(FT_BLE)
  if (_state.protocol == PROTOCOL_BLE && _state.enabled) {
    checkBleConnection();
  }
#endif

  // Only consider forwarding when:
  //  - the forwarder is enabled,
  //  - we actually have a weight reading,
  //  - the reading has been stable for WEIGHT_DEBOUNCE_MS,
  //  - and the hard MIN_FORWARD_INTERVAL floor has elapsed since the last forward.
  // The MIN_FORWARD_INTERVAL floor protects the receiver no matter what the user configures.
  const bool canConsiderForward = _state.enabled
      && !_pendingWeight.isEmpty()
      && (millis() - _weightLastChangedAt) >= WEIGHT_DEBOUNCE_MS
      && (millis() - _lastForwardTime) >= MIN_FORWARD_INTERVAL;

  if (canConsiderForward) {
    const bool weightChanged = (_pendingWeight != _lastForwardedWeight);
    const bool heartbeatDue = _state.heartbeatIntervalMs > 0
        && (millis() - _lastForwardTime) >= _state.heartbeatIntervalMs;

    if (weightChanged || heartbeatDue) {
      forwardWeight(_pendingLine, _pendingWeight);
      if (_state.lastError.isEmpty()) {
        _lastForwardedWeight = _pendingWeight;
      }
    }
  }
}

void WeightForwarderService::clearAuthTokens() {
  for (int i = 0; i < MAX_TARGET_URLS; i++) {
    _httpAuthTokens[i] = "";
    _httpAuthTokensValid[i] = false;
    _targetFailCount[i] = 0;
    _targetNextRetry[i] = 0;
  }
}

void WeightForwarderService::onConfigUpdated() {
  Serial.println(F("[WeightForwarder] Config updated"));

  // Propagate USB-echo flag to SerialService so the toggle in the web UI
  // takes effect on the very next scale line, not just after reboot.
  if (_serialService) {
    _serialService->setUsbEchoEnabled(_state.usbEchoEnabled);
  }

  // Invalidate all cached HTTP auth tokens (credentials or URLs may have changed)
  clearAuthTokens();

  // Persist config to flash
  bool saved = _fsPersistence.writeToFS();
  Serial.printf("[WeightForwarder] Config persisted to flash: %s\n", saved ? "SUCCESS" : "FAILED");

  // Handle protocol switch
  static ForwardProtocol lastProtocol = PROTOCOL_HTTP;
  if (_state.protocol != lastProtocol) {
    cleanupProtocol(lastProtocol);
    if (_state.enabled) {
      switchProtocol(_state.protocol);
    }
    lastProtocol = _state.protocol;
  }

  // Handle enable/disable
  static bool lastEnabled = false;
  if (_state.enabled != lastEnabled) {
    if (_state.enabled) {
      switchProtocol(_state.protocol);
    } else {
      cleanupProtocol(_state.protocol);
    }
    lastEnabled = _state.enabled;
  }
}

void WeightForwarderService::onSerialWeightUpdate(const String& originId) {
  if (originId != "serial_hw") {
    return;
  }

  // Snapshot once: capture current line/weight from the serial service
  String capturedLine;
  String capturedWeight;
  _serialService->read([&capturedLine, &capturedWeight](SerialState& serialState) {
    capturedLine = serialState.lastLine;
    capturedWeight = serialState.weight;
  });

  // NOTE: USB-CDC / UART0 echo is now performed inside SerialService::loop()
  // on every UART1 line read, regardless of whether the StatefulService
  // update fired CHANGED. That decoupling is what lets us throttle
  // /ws/serial broadcasts without starving ScaleCOM on COM3. The flag is
  // pushed down to SerialService in begin() and onConfigUpdated().

  if (!_state.enabled) {
    return;
  }

  if (!capturedWeight.isEmpty()) {
    if (capturedWeight != _pendingWeight) {
      _weightLastChangedAt = millis();
    }
    _pendingWeight = capturedWeight;
    _pendingLine = capturedLine;
  }
}

void WeightForwarderService::forwardWeight(const String& lastLine, const String& weight) {
  // Rate limiting: max 10 forwards/sec
  if (millis() - _lastForwardTime < MIN_FORWARD_INTERVAL) {
    return;
  }

  // Heap-pressure guard. We are about to allocate an HTTPClient + 256 B
  // JSON doc + serialised string + per-target retry buffers. If the heap
  // is already below the danger zone, dropping the forward is far better
  // than triggering an OOM panic mid-request. Counted in audit as
  // FORWARD_DROPPED so the leak is visible from /rest/weightForwarderAudit.
  if (wfHeapBelowThreshold()) {
    registerForwardDropped();
    return;
  }

  _lastForwardTime = millis();

#ifdef ESP32
  // Check WiFi connection
  if (!WiFi.isConnected()) {
    update([](WeightForwarderState& state) {
      state.connected = false;
      state.lastError = "No WiFi";
      return StateUpdateResult::CHANGED;
    }, "internal");
    return;
  }
#endif

  // Forward using selected protocol
  switch (_state.protocol) {
    case PROTOCOL_HTTP:
      forwardViaHttp(lastLine, weight);
      break;
    case PROTOCOL_WS:
      forwardViaWs(lastLine, weight);
      break;
    case PROTOCOL_MQTT:
      forwardViaMqtt(lastLine, weight);
      break;
    case PROTOCOL_BLE:
      forwardViaBle(lastLine, weight);
      break;
  }
}

void WeightForwarderService::formatJson(DynamicJsonDocument& doc, const String& lastLine, const String& weight, OutputFormat fmt) {
  switch (fmt) {
    case FMT_LCD: {
      String line1 = weight;
      String line2 = lastLine;
      while (line1.length() < 16) line1 += " ";
      while (line2.length() < 16) line2 += " ";
      if (line1.length() > 16) line1 = line1.substring(0, 16);
      if (line2.length() > 16) line2 = line2.substring(0, 16);
      doc["line1"] = line1;
      doc["line2"] = line2;
      break;
    }
    case FMT_TFT:
      doc["weight"] = weight;
      doc["last_line"] = lastLine;
      doc["timestamp"] = millis();
      doc["unit"] = "kg";
      doc["status"] = "live";
      break;
    case FMT_STANDARD:
    default:
      doc["last_line"] = lastLine;
      doc["weight"] = weight;
      doc["timestamp"] = millis();
      break;
  }
}

String WeightForwarderService::getHttpBaseUrl(const String& targetUrl) const {
  if (targetUrl.isEmpty()) return "";
  int start = (targetUrl.startsWith("https://")) ? 8 : (targetUrl.startsWith("http://") ? 7 : 0);
  if (start == 0) return targetUrl;
  int slash = targetUrl.indexOf('/', start);
  if (slash < 0) return targetUrl;
  return targetUrl.substring(0, slash);
}

bool WeightForwarderService::fetchHttpAuthToken(const String& baseUrl, int idx) {
#ifdef ESP32
  if (baseUrl.isEmpty() || _state.authUsername.isEmpty() || idx < 0 || idx >= MAX_TARGET_URLS) return false;

  String signInUrl = baseUrl + "/rest/signIn";
  HTTPClient http;
  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(HTTP_RESPONSE_TIMEOUT_MS);
  http.begin(signInUrl);
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument reqDoc(256);
  reqDoc["username"] = _state.authUsername;
  reqDoc["password"] = _state.authPassword;
  String reqBody;
  serializeJson(reqDoc, reqBody);

  int code = http.POST(reqBody);
  bool ok = false;
  if (code == 200) {
    String resp = http.getString();
    DynamicJsonDocument respDoc(512);
    if (deserializeJson(respDoc, resp) == DeserializationError::Ok) {
      const char* token = respDoc["access_token"];
      if (token && strlen(token) > 0) {
        _httpAuthTokens[idx] = token;
        _httpAuthTokensValid[idx] = true;
        ok = true;
        Serial.printf("[WeightForwarder] HTTP auth OK for target %d\n", idx);
      }
    }
  }
  if (!ok) {
    _httpAuthTokens[idx] = "";
    _httpAuthTokensValid[idx] = false;
    Serial.printf("[WeightForwarder] HTTP auth failed for target %d: %d\n", idx, code);
  }
  http.end();
  return ok;
#else
  return false;
#endif
}

bool WeightForwarderService::forwardViaHttpSingle(const String& url, int idx, const String& lastLine, const String& weight) {
#ifdef ESP32
  if (url.isEmpty()) return false;

  // Skip targets that have been failing — check backoff window
  if (_targetFailCount[idx] >= TARGET_FAIL_THRESHOLD) {
    if (millis() < _targetNextRetry[idx]) {
      return false;  // still in backoff, don't block
    }
    // backoff expired — try again and reset
    _targetFailCount[idx] = 0;
    Serial.printf("[WeightForwarder] Retrying target %d after backoff\n", idx + 1);
  }

  OutputFormat fmt = (idx < _state.targetUrlCount) ? _state.targetFormats[idx] : FMT_STANDARD;
  DynamicJsonDocument doc(256);
  formatJson(doc, lastLine, weight, fmt);
  String json;
  serializeJson(doc, json);

  HTTPClient http;
  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(HTTP_RESPONSE_TIMEOUT_MS);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  if (!_state.authUsername.isEmpty()) {
    if (!_httpAuthTokensValid[idx] || _httpAuthTokens[idx].isEmpty()) {
      if (!fetchHttpAuthToken(getHttpBaseUrl(url), idx)) {
        http.end();
        return false;
      }
    }
    http.addHeader("Authorization", "Bearer " + _httpAuthTokens[idx]);
  }

  int code = http.POST(json);

  // On 401, retry once after re-login
  if (code == 401 && !_state.authUsername.isEmpty()) {
    _httpAuthTokensValid[idx] = false;
    _httpAuthTokens[idx] = "";
    if (fetchHttpAuthToken(getHttpBaseUrl(url), idx)) {
      http.end();
      http.begin(url);
      http.addHeader("Content-Type", "application/json");
      http.addHeader("Authorization", "Bearer " + _httpAuthTokens[idx]);
      code = http.POST(json);
    }
  }

  bool ok = (code == 200);
  if (ok) {
    _targetFailCount[idx] = 0;
    _targetNextRetry[idx] = 0;
  } else {
    _targetFailCount[idx]++;
    if (_targetFailCount[idx] >= TARGET_FAIL_THRESHOLD) {
      _targetNextRetry[idx] = millis() + TARGET_FAIL_BACKOFF_MS;
      Serial.printf("[WeightForwarder] Target %d failed %d times, backing off %ds\n",
                    idx + 1, _targetFailCount[idx], TARGET_FAIL_BACKOFF_MS / 1000);
    }
  }
  http.end();
  return ok;
#else
  return false;
#endif
}

void WeightForwarderService::forwardViaHttp(const String& lastLine, const String& weight) {
#ifdef ESP32
  if (_state.targetUrlCount == 0) return;

  int successCount = 0;
  int httpCount = 0;
  String lastErr = "";

  for (int i = 0; i < _state.targetUrlCount; i++) {
    if (_state.targetFormats[i] == FMT_SERIAL) {
      forwardViaSerial(lastLine, weight);
      successCount++;
      continue;
    }
    httpCount++;
    if (forwardViaHttpSingle(_state.targetUrls[i], i, lastLine, weight)) {
      successCount++;
    } else {
      registerTargetFailed();
      lastErr = "Target " + String(i + 1) + " failed";
    }
  }

  int total = _state.targetUrlCount;
  if (successCount > 0) {
    registerForwardOk();
  }
  update([successCount, total, lastErr](WeightForwarderState& state) {
    state.connected = (successCount > 0);
    state.lastError = (successCount == total) ? "" : lastErr;
    state.lastForwardTime = millis();
    return StateUpdateResult::CHANGED;
  }, "internal");
#endif
}

void WeightForwarderService::forwardViaSerial(const String& lastLine, const String& weight) {
  Serial1.println(weight);
}

void WeightForwarderService::forwardViaWs(const String& lastLine, const String& weight) {
#ifdef ESP32
  if (!_wsClient || !_wsClient->isConnected()) {
    update([](WeightForwarderState& state) {
      state.connected = false;
      state.lastError = "WS not connected";
      return StateUpdateResult::CHANGED;
    }, "internal");
    return;
  }

  DynamicJsonDocument doc(256);
  formatJson(doc, lastLine, weight, FMT_STANDARD);

  String json;
  serializeJson(doc, json);

  _wsClient->sendTXT(json);

  update([](WeightForwarderState& state) {
    state.lastForwardTime = millis();
    return StateUpdateResult::CHANGED;
  }, "internal");
#endif
}

void WeightForwarderService::forwardViaMqtt(const String& lastLine, const String& weight) {
#if FT_ENABLED(FT_MQTT)
  if (!_mqttClient->connected() || _state.mqttTopic.isEmpty()) {
    update([](WeightForwarderState& state) {
      state.connected = false;
      state.lastError = "MQTT not connected";
      return StateUpdateResult::CHANGED;
    }, "internal");
    return;
  }

  DynamicJsonDocument doc(256);
  formatJson(doc, lastLine, weight, FMT_STANDARD);

  String json;
  serializeJson(doc, json);

  _mqttClient->publish(_state.mqttTopic.c_str(), 0, false, json.c_str());

  update([](WeightForwarderState& state) {
    state.connected = true;
    state.lastError = "";
    state.lastForwardTime = millis();
    return StateUpdateResult::CHANGED;
  }, "internal");
#endif
}

void WeightForwarderService::forwardViaBle(const String& lastLine, const String& weight) {
#if FT_ENABLED(FT_BLE)
  if (!_bleClient || !_bleClient->isConnected()) {
    update([](WeightForwarderState& state) {
      state.connected = false;
      state.lastError = "BLE not connected";
      return StateUpdateResult::CHANGED;
    }, "internal");
    return;
  }

  if (_bleRemoteChar && _bleRemoteChar->canWrite()) {
    DynamicJsonDocument doc(256);
    formatJson(doc, lastLine, weight, FMT_STANDARD);

    String json;
    serializeJson(doc, json);

    _bleRemoteChar->writeValue(json.c_str(), json.length());

    update([](WeightForwarderState& state) {
      state.lastForwardTime = millis();
      return StateUpdateResult::CHANGED;
    }, "internal");
  }
#endif
}

void WeightForwarderService::switchProtocol(ForwardProtocol protocol) {
  Serial.printf("[WeightForwarder] Switching to protocol: %d\n", (int)protocol);

  switch (protocol) {
    case PROTOCOL_HTTP:
      // HTTP client created per-request, no init needed
      break;

    case PROTOCOL_WS:
      initWebSocketClient();
      break;

    case PROTOCOL_MQTT:
#if FT_ENABLED(FT_MQTT)
      // MQTT client already initialized by framework
      if (_mqttClient->connected()) {
        update([](WeightForwarderState& state) {
          state.connected = true;
          state.lastError = "";
          return StateUpdateResult::CHANGED;
        }, "internal");
      }
#endif
      break;

    case PROTOCOL_BLE:
      initBleClient();
      break;
  }
}

void WeightForwarderService::cleanupProtocol(ForwardProtocol protocol) {
  Serial.printf("[WeightForwarder] Cleaning up protocol: %d\n", (int)protocol);

  switch (protocol) {
    case PROTOCOL_WS:
#ifdef ESP32
      if (_wsClient) {
        _wsClient->disconnect();
        delete _wsClient;
        _wsClient = nullptr;
      }
#endif
      break;

    case PROTOCOL_BLE:
#if FT_ENABLED(FT_BLE)
      if (_bleClient) {
        if (_bleClient->isConnected()) {
          _bleClient->disconnect();
        }
        delete _bleClient;
        _bleClient = nullptr;
        _bleRemoteChar = nullptr;
      }
#endif
      break;

    default:
      // HTTP and MQTT don't need cleanup
      break;
  }
}

void WeightForwarderService::initWebSocketClient() {
#ifdef ESP32
  if (_state.wsUrl.isEmpty()) {
    Serial.println(F("[WeightForwarder] WebSocket URL not configured"));
    return;
  }

  if (_wsClient) {
    delete _wsClient;
  }

  _wsClient = new WebSocketsClient();

  // Parse URL: ws://192.168.1.50:8080/ws
  String url = _state.wsUrl;
  url.replace("ws://", "");
  int portIdx = url.indexOf(':');
  int pathIdx = url.indexOf('/');

  String host = portIdx > 0 ? url.substring(0, portIdx) : url.substring(0, pathIdx);
  uint16_t port = portIdx > 0 ? url.substring(portIdx + 1, pathIdx).toInt() : 80;
  String path = pathIdx > 0 ? url.substring(pathIdx) : "/";

  // If auth credentials are set, sign in to get JWT and append as query parameter
  // (WebSocket endpoints use ?access_token=<JWT> for auth) — use slot 0 for WS auth
  if (!_state.authUsername.isEmpty()) {
    String baseUrl = "http://" + host + (port != 80 ? ":" + String(port) : "");
    if (!_httpAuthTokensValid[0] || _httpAuthTokens[0].isEmpty()) {
      fetchHttpAuthToken(baseUrl, 0);
    }
    if (_httpAuthTokensValid[0] && !_httpAuthTokens[0].isEmpty()) {
      path += (path.indexOf('?') >= 0 ? "&" : "?");
      path += "access_token=" + _httpAuthTokens[0];
      Serial.println(F("[WeightForwarder] WS auth token appended"));
    } else {
      Serial.println(F("[WeightForwarder] WS auth sign-in failed, connecting without token"));
    }
  }

  Serial.printf("[WeightForwarder] WS connecting to %s:%d%s\n", host.c_str(), port, path.c_str());

  _wsClient->onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
    if (type == WStype_CONNECTED) {
      Serial.println(F("[WeightForwarder] WebSocket connected"));
      update([](WeightForwarderState& state) {
        state.connected = true;
        state.lastError = "";
        return StateUpdateResult::CHANGED;
      }, "internal");
    } else if (type == WStype_DISCONNECTED) {
      Serial.println(F("[WeightForwarder] WebSocket disconnected"));
      update([](WeightForwarderState& state) {
        state.connected = false;
        state.lastError = "Disconnected";
        return StateUpdateResult::CHANGED;
      }, "internal");
    } else if (type == WStype_ERROR) {
      Serial.println(F("[WeightForwarder] WebSocket error"));
      update([](WeightForwarderState& state) {
        state.connected = false;
        state.lastError = "Connection error";
        return StateUpdateResult::CHANGED;
      }, "internal");
    }
  });

  _wsClient->begin(host, port, path);
  _lastWsReconnectAttempt = millis();
#endif
}

void WeightForwarderService::initBleClient() {
#if FT_ENABLED(FT_BLE)
  if (_state.bleServiceUuid.isEmpty() || _state.bleCharUuid.isEmpty()) {
    Serial.println(F("[WeightForwarder] BLE UUIDs not configured"));
    return;
  }

  Serial.println(F("[WeightForwarder] Initializing BLE client..."));
  BLEDevice::init("WeightForwarder");

  if (_bleClient) {
    delete _bleClient;
  }

  _bleClient = BLEDevice::createClient();
  Serial.println(F("[WeightForwarder] BLE client created, scanning for devices..."));

  // Start scan (connection happens in checkBleConnection)
  _lastBleReconnectAttempt = millis();
#endif
}

#ifdef ESP32
void WeightForwarderService::checkWsConnection() {
  if (_state.protocol != PROTOCOL_WS || !_state.enabled) {
    return;
  }

  // Reconnect every 5 seconds if disconnected
  if (!_wsClient || !_wsClient->isConnected()) {
    if (millis() - _lastWsReconnectAttempt >= 5000) {
      Serial.println(F("[WeightForwarder] Reconnecting WebSocket..."));
      initWebSocketClient();
    }
  }
}
#endif

#if FT_ENABLED(FT_BLE)
void WeightForwarderService::checkBleConnection() {
  if (!_bleClient || !_bleClient->isConnected()) {
    // Reconnect every 10 seconds if disconnected
    if (millis() - _lastBleReconnectAttempt >= 10000) {
      Serial.println(F("[WeightForwarder] Reconnecting BLE..."));

      BLEScan* pScan = BLEDevice::getScan();
      pScan->setActiveScan(true);
      BLEScanResults results = pScan->start(5, false);

      // Find target device by service UUID
      for (int i = 0; i < results.getCount(); i++) {
        BLEAdvertisedDevice device = results.getDevice(i);
        if (device.haveServiceUUID() &&
            device.getServiceUUID().equals(BLEUUID(_state.bleServiceUuid.c_str()))) {
          Serial.printf("[WeightForwarder] Found BLE device: %s\n", device.getAddress().toString().c_str());

          if (_bleClient->connect(&device)) {
            Serial.println(F("[WeightForwarder] BLE connected, getting service..."));
            BLERemoteService* pService = _bleClient->getService(BLEUUID(_state.bleServiceUuid.c_str()));
            if (pService) {
              _bleRemoteChar = pService->getCharacteristic(BLEUUID(_state.bleCharUuid.c_str()));
              if (_bleRemoteChar) {
                Serial.println(F("[WeightForwarder] BLE characteristic ready"));
                update([](WeightForwarderState& state) {
                  state.connected = true;
                  state.lastError = "";
                  return StateUpdateResult::CHANGED;
                }, "internal");
                return;
              }
            }
          }
        }
      }

      // Not found or connection failed
      update([](WeightForwarderState& state) {
        state.connected = false;
        state.lastError = "BLE device not found";
        return StateUpdateResult::CHANGED;
      }, "internal");

      _lastBleReconnectAttempt = millis();
    }
  }
}
#endif

// ---------------------------------------------------------------------------
// Audit ring-buffer + REST endpoint
// ---------------------------------------------------------------------------

void WeightForwarderService::recordAudit(WeightForwarderAuditReason reason) {
  WeightForwarderAuditEntry& e = _audit[_auditHead];
  e.millis_at = millis();
  e.free_heap = ESP.getFreeHeap();
  e.min_heap = _minHeapSeen;
  e.max_alloc = ESP.getMaxAllocHeap();
  e.fwd_total = _fwdTotal;
  e.fwd_dropped = _fwdDropped;
  e.fwd_failed = _fwdFailed;
  e.reason = static_cast<uint8_t>(reason);
  e.ws_clients = static_cast<uint8_t>(_webSocket.getWebSocket().count());

  _auditHead = (_auditHead + 1) % WEIGHT_FORWARDER_AUDIT_CAPACITY;
  if (_auditCount < WEIGHT_FORWARDER_AUDIT_CAPACITY) _auditCount++;
}

void WeightForwarderService::registerForwardOk() {
  _fwdTotal++;
  uint32_t h = ESP.getFreeHeap();
  if (h < _minHeapSeen) _minHeapSeen = h;
  recordAudit(WeightForwarderAuditReason::FORWARD_OK);
}

void WeightForwarderService::registerForwardDropped() {
  _fwdDropped++;
  uint32_t h = ESP.getFreeHeap();
  if (h < _minHeapSeen) _minHeapSeen = h;
  recordAudit(WeightForwarderAuditReason::FORWARD_DROPPED);

  // Rate-limit log so a sustained heap-pressure event doesn't flood COM3.
  static unsigned long lastDropLog = 0;
  unsigned long now = millis();
  if (now - lastDropLog >= 5000UL) {
    lastDropLog = now;
    Serial.printf("[WeightForwarder] DROPPING forward — free_heap=%lu min=%lu threshold=%u dropped_total=%u\n",
                  (unsigned long)h, (unsigned long)_minHeapSeen,
                  WEIGHT_FORWARDER_MIN_FREE_HEAP, _fwdDropped);
  }
}

void WeightForwarderService::registerTargetFailed() {
  _fwdFailed++;
  recordAudit(WeightForwarderAuditReason::TARGET_FAILED);
}

void WeightForwarderService::registerAuditEndpoint() {
  _server->on(WEIGHT_FORWARDER_AUDIT_PATH, HTTP_GET,
              _securityManager->wrapRequest(
                  [this](AsyncWebServerRequest* request) {
                    AsyncJsonResponse* response = new AsyncJsonResponse(false, 4096);
                    JsonObject root = response->getRoot().to<JsonObject>();
                    readAuditAndStats(this, root);
                    response->setLength();
                    request->send(response);
                  },
                  AuthenticationPredicates::IS_AUTHENTICATED));
}

void WeightForwarderService::readAuditAndStats(WeightForwarderService* self, JsonObject& root) {
  root["now_ms"] = millis();
  root["free_heap"] = ESP.getFreeHeap();
  root["min_heap_seen"] = self->_minHeapSeen;
  root["max_alloc_heap"] = ESP.getMaxAllocHeap();
  root["fwd_total"] = self->_fwdTotal;
  root["fwd_dropped"] = self->_fwdDropped;
  root["fwd_failed"] = self->_fwdFailed;
  root["heap_threshold"] = WEIGHT_FORWARDER_MIN_FREE_HEAP;
  root["ws_clients"] = self->_webSocket.getWebSocket().count();
  root["enabled"] = self->_state.enabled;
  root["protocol"] = (int)self->_state.protocol;
  root["target_url_count"] = self->_state.targetUrlCount;

  JsonArray entries = root.createNestedArray("entries");
  uint8_t cap = WEIGHT_FORWARDER_AUDIT_CAPACITY;
  uint8_t start = (cap + self->_auditHead - self->_auditCount) % cap;
  for (uint8_t i = 0; i < self->_auditCount; i++) {
    const WeightForwarderAuditEntry& e = self->_audit[(start + i) % cap];
    JsonObject obj = entries.createNestedObject();
    obj["t"] = e.millis_at;
    obj["heap"] = e.free_heap;
    obj["min"] = e.min_heap;
    obj["alloc"] = e.max_alloc;
    obj["fwd"] = e.fwd_total;
    obj["dropped"] = e.fwd_dropped;
    obj["failed"] = e.fwd_failed;
    obj["ws"] = e.ws_clients;
    obj["reason"] = e.reason;
  }
}
