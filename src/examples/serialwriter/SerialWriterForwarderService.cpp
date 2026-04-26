#include <examples/serialwriter/SerialWriterForwarderService.h>
#include <examples/serialwriter/SerialWriterForwarderJsonMapping.h>
#include <ArduinoJson.h>

#ifdef ESP32
#include <WiFi.h>
#endif

SerialWriterForwarderService::SerialWriterForwarderService(AsyncWebServer* server,
                                                           FS* fs,
                                                           SecurityManager* securityManager,
                                                           SerialWriterService* serialWriterService)
    : _httpEndpoint(SerialWriterForwarderState::read,
                    SerialWriterForwarderState::update,
                    this,
                    server,
                    SERIAL_WRITER_FORWARDER_ENDPOINT_PATH,
                    securityManager,
                    AuthenticationPredicates::IS_AUTHENTICATED),
      _fsPersistence(SerialWriterForwarderState::readConfig,
                     SerialWriterForwarderState::updateConfig,
                     this,
                     fs,
                     SERIAL_WRITER_FORWARDER_CONFIG_FILE),
      _webSocket(SerialWriterForwarderState::read,
                 SerialWriterForwarderState::update,
                 this,
                 server,
                 SERIAL_WRITER_FORWARDER_SOCKET_PATH,
                 securityManager,
                 AuthenticationPredicates::IS_AUTHENTICATED),
      _serialWriterService(serialWriterService)
#ifdef ESP32
      ,
      _wsClient(nullptr),
      _lastWsReconnectAttempt(0)
#endif
      ,
      _lastPollTime(0),
      _httpAuthTokenValid(false) {
  _fsPersistence.disableUpdateHandler();

  addUpdateHandler([this](const String& originId) {
    if (originId != "internal") {
      onConfigUpdated();
    }
  }, false);
}

void SerialWriterForwarderService::begin() {
  _fsPersistence.readFromFS();
  const char* outLabel = "usb_only";
  if (_state.outputTargets == SERIAL_WRITER_FORWARDER_OUTPUT_SERIAL1_ONLY) {
    outLabel = "serial1_only";
  } else if (_state.outputTargets == SERIAL_WRITER_FORWARDER_OUTPUT_BOTH) {
    outLabel = "both";
  }
  Serial.printf("[SerialWriterForwarder] Loaded config: protocol=%d, enabled=%d, output=%s, url='%s'\n",
                (int)_state.protocol,
                _state.enabled,
                outLabel,
                _state.sourceUrl.c_str());

  _state.connected = false;
  _state.lastError = "";
  _state.lastReceivedMs = 0;
  _state.receivedCount = 0;
  _httpAuthTokenValid = false;
  _httpAuthToken = "";

  if (_state.enabled && _state.protocol == SERIAL_WRITER_FORWARDER_PROTOCOL_WS) {
    initWsClient();
  }
}

void SerialWriterForwarderService::loop() {
  if (!_state.enabled) {
    return;
  }

#ifdef ESP32
  if (_state.protocol == SERIAL_WRITER_FORWARDER_PROTOCOL_WS) {
    if (_wsClient) {
      _wsClient->loop();
    }
    checkWsConnection();
    return;
  }

  // HTTP polling mode
  if (millis() - _lastPollTime >= _state.pollIntervalMs) {
    _lastPollTime = millis();
    pollHttp();
  }
#endif
}

void SerialWriterForwarderService::onConfigUpdated() {
  Serial.println(F("[SerialWriterForwarder] Config updated"));

  _httpAuthTokenValid = false;
  _httpAuthToken = "";

  bool saved = _fsPersistence.writeToFS();
  Serial.printf("[SerialWriterForwarder] Config persisted: %s\n", saved ? "OK" : "FAILED");

#ifdef ESP32
  // Tear down WS client if protocol changed or disabled
  if (_wsClient) {
    _wsClient->disconnect();
    delete _wsClient;
    _wsClient = nullptr;
  }

  if (_state.enabled && _state.protocol == SERIAL_WRITER_FORWARDER_PROTOCOL_WS) {
    initWsClient();
  }
#endif
}

void SerialWriterForwarderService::deliverLine(const String& line, const char* sourcePath) {
  if (line.isEmpty()) return;

  if (sourcePath != nullptr && sourcePath[0] != '\0') {
    Serial.printf("[SerialWriterForwarder][delivered] field=%s path=%s len=%u\n",
                  _state.jsonField.c_str(),
                  sourcePath,
                  (unsigned)line.length());
  } else {
    Serial.printf("[SerialWriterForwarder][delivered] field=%s len=%u\n",
                  _state.jsonField.c_str(),
                  (unsigned)line.length());
  }

  uint8_t sinkMask = 0;
  if (_state.outputTargets == SERIAL_WRITER_FORWARDER_OUTPUT_USB_ONLY) {
    sinkMask = SERIAL_WRITER_FORWARDER_SINK_USB;
  } else if (_state.outputTargets == SERIAL_WRITER_FORWARDER_OUTPUT_SERIAL1_ONLY) {
    sinkMask = SERIAL_WRITER_FORWARDER_SINK_SERIAL1;
  } else if (_state.outputTargets == SERIAL_WRITER_FORWARDER_OUTPUT_BOTH) {
    sinkMask = SERIAL_WRITER_FORWARDER_SINK_USB | SERIAL_WRITER_FORWARDER_SINK_SERIAL1;
  } else {
    sinkMask = SERIAL_WRITER_FORWARDER_SINK_USB;
  }
  _serialWriterService->enqueueForwardedLine(line, sinkMask);

  update([&](SerialWriterForwarderState& state) {
    state.lastReceivedLine = line;
    state.lastReceivedMs = millis();
    state.receivedCount++;
    state.connected = true;
    state.lastError = "";
    return StateUpdateResult::CHANGED;
  }, "internal");
}

void SerialWriterForwarderService::setRuntimeError(const String& errorText, bool connected) {
  const String message = errorText;
  const bool isConnected = connected;
  update([message, isConnected](SerialWriterForwarderState& state) {
    state.connected = isConnected;
    state.lastError = message;
    return StateUpdateResult::CHANGED;
  }, "internal");
}

void SerialWriterForwarderService::pollHttp() {
#ifdef ESP32
  if (_state.sourceUrl.isEmpty()) {
    return;
  }

  if (!WiFi.isConnected()) {
    setRuntimeError("No WiFi", false);
    return;
  }

  HTTPClient http;
  http.begin(_state.sourceUrl);
  http.setTimeout(2000);

  if (!_state.authUsername.isEmpty()) {
    if (!_httpAuthTokenValid || _httpAuthToken.isEmpty()) {
      String baseUrl = getHttpBaseUrl(_state.sourceUrl);
      fetchHttpAuthToken(baseUrl);
    }
    if (_httpAuthTokenValid && !_httpAuthToken.isEmpty()) {
      http.addHeader("Authorization", "Bearer " + _httpAuthToken);
    }
  }

  int code = http.GET();

  // Retry once after re-auth on 401
  if (code == 401 && !_state.authUsername.isEmpty()) {
    _httpAuthTokenValid = false;
    _httpAuthToken = "";
    String baseUrl = getHttpBaseUrl(_state.sourceUrl);
    if (fetchHttpAuthToken(baseUrl)) {
      http.end();
      http.begin(_state.sourceUrl);
      http.setTimeout(2000);
      http.addHeader("Authorization", "Bearer " + _httpAuthToken);
      code = http.GET();
    }
  }

  if (code == 200) {
    String body = http.getString();
    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, body);
    if (err == DeserializationError::Ok) {
      char lineBuf[256];
      char pathBuf[32];
      SerialWriterForwarderJsonOutcome outcome = serialWriterForwarderExtractLineFromDocument(
          _state.jsonField.c_str(), doc, lineBuf, sizeof(lineBuf), pathBuf, sizeof(pathBuf));
      if (outcome == SerialWriterForwarderJsonOutcome::Ok) {
        deliverLine(String(lineBuf), pathBuf);
      } else if (outcome == SerialWriterForwarderJsonOutcome::FieldMissing) {
        String error = "Field '" + _state.jsonField + "' not found (top-level/payload)";
        Serial.printf("[SerialWriterForwarder][http-field-missing] field=%s\n", _state.jsonField.c_str());
        setRuntimeError(error, true);
      } else if (outcome == SerialWriterForwarderJsonOutcome::FieldEmpty) {
        String error = "Field '" + _state.jsonField + "' is empty";
        Serial.printf("[SerialWriterForwarder][http-field-empty] field=%s\n", _state.jsonField.c_str());
        setRuntimeError(error, true);
      } else {
        String error = "HTTP extract internal error";
        Serial.println(F("[SerialWriterForwarder][http-parse-error] internal"));
        setRuntimeError(error, true);
      }
    } else {
      String error = "JSON parse error";
      Serial.printf("[SerialWriterForwarder][http-parse-error] %s\n", err.c_str());
      setRuntimeError(error, false);
    }
  } else {
    String error = "HTTP " + String(code);
    Serial.printf("[SerialWriterForwarder] %s\n", error.c_str());
    setRuntimeError(error, false);
  }
  http.end();
#endif
}

String SerialWriterForwarderService::getHttpBaseUrl(const String& url) const {
  if (url.isEmpty()) return "";
  int start = url.startsWith("https://") ? 8 : (url.startsWith("http://") ? 7 : 0);
  if (start == 0) return url;
  int slash = url.indexOf('/', start);
  if (slash < 0) return url;
  return url.substring(0, slash);
}

bool SerialWriterForwarderService::fetchHttpAuthToken(const String& baseUrl) {
#ifdef ESP32
  if (baseUrl.isEmpty() || _state.authUsername.isEmpty()) return false;

  String signInUrl = baseUrl + "/rest/signIn";
  HTTPClient http;
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
        _httpAuthToken = token;
        _httpAuthTokenValid = true;
        ok = true;
        Serial.println(F("[SerialWriterForwarder] HTTP auth OK"));
      }
    }
  }
  if (!ok) {
    _httpAuthToken = "";
    _httpAuthTokenValid = false;
    Serial.printf("[SerialWriterForwarder] HTTP auth failed: %d\n", code);
  }
  http.end();
  return ok;
#else
  return false;
#endif
}

void SerialWriterForwarderService::initWsClient() {
#ifdef ESP32
  if (_state.sourceUrl.isEmpty()) {
    Serial.println(F("[SerialWriterForwarder] WS source URL not configured"));
    return;
  }

  if (_wsClient) {
    delete _wsClient;
  }

  _wsClient = new WebSocketsClient();

  // Parse ws://host:port/path
  String url = _state.sourceUrl;
  url.replace("ws://", "");
  int portIdx = url.indexOf(':');
  int pathIdx = url.indexOf('/');

  String host = portIdx > 0 ? url.substring(0, portIdx) : url.substring(0, pathIdx > 0 ? pathIdx : url.length());
  uint16_t port = portIdx > 0 ? url.substring(portIdx + 1, pathIdx > 0 ? pathIdx : url.length()).toInt() : 80;
  String path = pathIdx > 0 ? url.substring(pathIdx) : "/";

  if (!_state.authUsername.isEmpty()) {
    String baseUrl = "http://" + host + (port != 80 ? ":" + String(port) : "");
    if (!_httpAuthTokenValid || _httpAuthToken.isEmpty()) {
      fetchHttpAuthToken(baseUrl);
    }
    if (_httpAuthTokenValid && !_httpAuthToken.isEmpty()) {
      path += (path.indexOf('?') >= 0 ? "&" : "?");
      path += "access_token=" + _httpAuthToken;
    }
  }

  Serial.printf("[SerialWriterForwarder] WS connecting to %s:%d%s\n", host.c_str(), port, path.c_str());

  _wsClient->onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
    if (type == WStype_CONNECTED) {
      Serial.println(F("[SerialWriterForwarder] WS connected"));
      update([](SerialWriterForwarderState& state) {
        state.connected = true;
        state.lastError = "";
        return StateUpdateResult::CHANGED;
      }, "internal");
    } else if (type == WStype_DISCONNECTED) {
      Serial.println(F("[SerialWriterForwarder] WS disconnected"));
      update([](SerialWriterForwarderState& state) {
        state.connected = false;
        state.lastError = "Disconnected";
        return StateUpdateResult::CHANGED;
      }, "internal");
    } else if (type == WStype_TEXT) {
      char lineBuf[256];
      char pathBuf[32];
      SerialWriterForwarderJsonOutcome outcome = serialWriterForwarderExtractLineFromJsonBytes(
          _state.jsonField.c_str(), payload, length, lineBuf, sizeof(lineBuf), pathBuf, sizeof(pathBuf));
      if (outcome == SerialWriterForwarderJsonOutcome::Ok) {
        deliverLine(String(lineBuf), pathBuf);
      } else if (outcome == SerialWriterForwarderJsonOutcome::FieldMissing) {
        String error = "WS field '" + _state.jsonField + "' not found (top-level/payload)";
        Serial.printf("[SerialWriterForwarder][ws-field-missing] field=%s\n", _state.jsonField.c_str());
        setRuntimeError(error, true);
      } else if (outcome == SerialWriterForwarderJsonOutcome::FieldEmpty) {
        String error = "WS field '" + _state.jsonField + "' is empty";
        Serial.printf("[SerialWriterForwarder][ws-field-empty] field=%s\n", _state.jsonField.c_str());
        setRuntimeError(error, true);
      } else {
        DynamicJsonDocument scratch(64);
        DeserializationError err = deserializeJson(scratch, payload, length);
        String error = String("WS JSON parse error: ") + err.c_str();
        Serial.printf("[SerialWriterForwarder][ws-parse-error] field=%s err=%s\n",
                      _state.jsonField.c_str(),
                      err.c_str());
        setRuntimeError(error, true);
      }
    } else if (type == WStype_ERROR) {
      Serial.println(F("[SerialWriterForwarder] WS error"));
      setRuntimeError("Connection error", false);
    }
  });

  _wsClient->begin(host, port, path);
  _lastWsReconnectAttempt = millis();
#endif
}

void SerialWriterForwarderService::checkWsConnection() {
#ifdef ESP32
  if (_state.protocol != SERIAL_WRITER_FORWARDER_PROTOCOL_WS || !_state.enabled) {
    return;
  }
  if (!_wsClient || !_wsClient->isConnected()) {
    if (millis() - _lastWsReconnectAttempt >= 5000) {
      Serial.println(F("[SerialWriterForwarder] Reconnecting WS..."));
      initWsClient();
    }
  }
#endif
}
