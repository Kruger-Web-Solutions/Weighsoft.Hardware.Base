#include <examples/serialwriter/SerialWriterForwarderService.h>
#include <examples/serialwriter/SerialWriterForwarderJsonMapping.h>
#include <ArduinoJson.h>

#ifdef ESP32
#include <WiFi.h>

namespace {

bool outboundWsDisconnectPayloadIsAsciiStackMessage(const uint8_t* payload, size_t length) {
  if (length == 0 || payload == nullptr) {
    return false;
  }
  // RFC 6455 close codes 1000–4999 are big-endian with first byte in 0x03–0x13 (never ASCII printable).
  // links2004/WebSockets sometimes passes a plain ASCII status (e.g. "TCP connection cleanup") instead.
  if (payload[0] < 0x20) {
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    uint8_t c = payload[i];
    if (c == '\0') {
      break;
    }
    if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') {
      return false;
    }
  }
  return true;
}

// WebSocketsClient DISCONNECTED: either RFC 6455 close (2-byte BE code + optional UTF-8 reason) or ASCII stack text.
void logSerialWriterOutboundWsClosePayload(const uint8_t* payload, size_t length) {
  if (length == 0) {
    Serial.println(F("[SerialWriterForwarder][ws-close] no_close_payload (abnormal/TCP reset common)"));
    return;
  }
  if (payload == nullptr) {
    Serial.printf("[SerialWriterForwarder][ws-close] null_payload len=%u\n", static_cast<unsigned>(length));
    return;
  }
  if (length < 2) {
    Serial.printf("[SerialWriterForwarder][ws-close] short_payload len=%u\n", static_cast<unsigned>(length));
    return;
  }

  if (outboundWsDisconnectPayloadIsAsciiStackMessage(payload, length)) {
    constexpr size_t kMaxAscii = 120;
    size_t n = length < kMaxAscii ? length : kMaxAscii;
    Serial.print(F("[SerialWriterForwarder][ws-close] stack_msg len="));
    Serial.print(static_cast<unsigned>(length));
    Serial.print(F(" text='"));
    for (size_t i = 0; i < n; ++i) {
      char c = static_cast<char>(payload[i]);
      if (c == '\0') {
        break;
      }
      if (c >= 0x20 && c <= 0x7e) {
        Serial.print(c);
      } else if (c == '\n' || c == '\r' || c == '\t') {
        Serial.print(c);
      } else {
        Serial.print('.');
      }
    }
    if (length > kMaxAscii) {
      Serial.print(F("..."));
    }
    Serial.println('\'');
    return;
  }

  uint16_t code =
      static_cast<uint16_t>((static_cast<uint16_t>(payload[0]) << 8) | static_cast<uint16_t>(payload[1]));
  Serial.printf("[SerialWriterForwarder][ws-close] rfc6455_code=%u len=%u\n",
                static_cast<unsigned>(code),
                static_cast<unsigned>(length));
  if (length > 2) {
    constexpr size_t kMaxReasonHexBytes = 32;
    size_t reasonLen = length - 2;
    if (reasonLen > kMaxReasonHexBytes) {
      reasonLen = kMaxReasonHexBytes;
    }
    Serial.print(F("[SerialWriterForwarder][ws-close] reason_hex="));
    for (size_t i = 0; i < reasonLen; ++i) {
      Serial.printf("%02x", static_cast<unsigned>(payload[2 + i]));
    }
    if (length - 2 > kMaxReasonHexBytes) {
      Serial.print(F("..."));
    }
    Serial.println();
  }
}

}  // namespace
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
      _lastWsReconnectAttempt(0),
      _wsAttemptCount(0),
      _wsHadConnected(false),
      _wsReconnectDelayMs(5000),
      _lastSignInHttpCode(0),
      _consecutiveSignInTransportFailures(0)
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
#ifdef ESP32
  _wsAttemptCount = 0;
  _wsHadConnected = false;
  _wsReconnectDelayMs = 5000;
  _lastSignInHttpCode = 0;
  _consecutiveSignInTransportFailures = 0;
#endif

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
  _wsAttemptCount = 0;
  _wsHadConnected = false;
  _wsReconnectDelayMs = 5000;
  _lastSignInHttpCode = 0;
  _consecutiveSignInTransportFailures = 0;

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
  http.setTimeout(2000);
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument reqDoc(256);
  reqDoc["username"] = _state.authUsername;
  reqDoc["password"] = _state.authPassword;
  String reqBody;
  serializeJson(reqDoc, reqBody);

  int code = http.POST(reqBody);
  _lastSignInHttpCode = code;
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
      }
    }
  }
  if (!ok) {
    _httpAuthToken = "";
    _httpAuthTokenValid = false;
    if (code == -1) {
      Serial.printf(
          "[SerialWriterForwarder][http-auth] failed url=%s http_code=%d (TCP/connect failed; source unreachable?)\n",
          signInUrl.c_str(),
          code);
    } else {
      Serial.printf("[SerialWriterForwarder][http-auth] failed url=%s http_code=%d\n", signInUrl.c_str(), code);
    }
  } else {
    _consecutiveSignInTransportFailures = 0;
    Serial.printf("[SerialWriterForwarder][http-auth] ok url=%s token_len=%u\n",
                  signInUrl.c_str(),
                  (unsigned)_httpAuthToken.length());
  }
  http.end();
  return ok;
#else
  return false;
#endif
}

#ifdef ESP32
const char* SerialWriterForwarderService::wsTypeName(WStype_t type) {
  switch (type) {
    case WStype_ERROR:
      return "ERROR";
    case WStype_DISCONNECTED:
      return "DISCONNECTED";
    case WStype_CONNECTED:
      return "CONNECTED";
    case WStype_TEXT:
      return "TEXT";
    case WStype_BIN:
      return "BIN";
    case WStype_FRAGMENT_TEXT_START:
      return "FRAGMENT_TEXT_START";
    case WStype_FRAGMENT_BIN_START:
      return "FRAGMENT_BIN_START";
    case WStype_FRAGMENT:
      return "FRAGMENT";
    case WStype_FRAGMENT_FIN:
      return "FRAGMENT_FIN";
    case WStype_PING:
      return "PING";
    case WStype_PONG:
      return "PONG";
    default:
      return "UNKNOWN";
  }
}
#endif

void SerialWriterForwarderService::initWsClient() {
#ifdef ESP32
  if (_state.sourceUrl.isEmpty()) {
    Serial.println(F("[SerialWriterForwarder][ws-attempt] skipped reason=no-source-url"));
    setRuntimeError("Source URL not configured", false);
    return;
  }

  // Gate on WiFi: skip allocation when WiFi is down so we don't churn TCP attempts on a dead radio
  if (!WiFi.isConnected()) {
    Serial.println(F("[SerialWriterForwarder][ws-attempt] skipped reason=wifi-down"));
    setRuntimeError("No WiFi", false);
    _lastWsReconnectAttempt = millis();
    return;
  }

  if (_wsClient) {
    _wsClient->disconnect();
    delete _wsClient;
    _wsClient = nullptr;
  }

  // Parse ws://host:port/path before any WebSocketsClient allocation
  String url = _state.sourceUrl;
  url.replace("ws://", "");
  int portIdx = url.indexOf(':');
  int pathIdx = url.indexOf('/');

  String host = portIdx > 0 ? url.substring(0, portIdx) : url.substring(0, pathIdx > 0 ? pathIdx : url.length());
  uint16_t port = portIdx > 0 ? url.substring(portIdx + 1, pathIdx > 0 ? pathIdx : url.length()).toInt() : 80;
  String path = pathIdx > 0 ? url.substring(pathIdx) : "/";

  String baseUrl = "http://" + host + (port != 80 ? ":" + String(port) : "");

  // Auth before WebSocketsClient: failed sign-in must not call begin() (avoids socket churn / lwIP exhaustion)
  if (_state.authUsername.isEmpty()) {
    Serial.println(F("[SerialWriterForwarder][ws-attempt] auth=none"));
  } else {
    if (!_httpAuthTokenValid || _httpAuthToken.isEmpty()) {
      fetchHttpAuthToken(baseUrl);
    }
    if (!_httpAuthTokenValid || _httpAuthToken.isEmpty()) {
      Serial.printf("[SerialWriterForwarder][ws-attempt] auth=token-failed user=%s (sign-in did not return access_token)\n",
                    _state.authUsername.c_str());
      if (_lastSignInHttpCode < 0) {
        setRuntimeError("Source unreachable: TCP to " + baseUrl + " failed (reader HTTP not accepting connections)",
                        false);
      } else {
        setRuntimeError("Auth failed: sign-in to " + baseUrl + " did not return token", false);
      }
      // Transport (-1): lower cap for quick recovery after brief reader outages. HTTP errors (401): higher cap.
      constexpr uint32_t kBackoffCapTransportMs = 15000;
      constexpr uint32_t kBackoffCapHttpMs = 60000;
      if (_lastSignInHttpCode < 0) {
        if (_wsReconnectDelayMs < kBackoffCapTransportMs) {
          uint32_t next = _wsReconnectDelayMs * 2u;
          _wsReconnectDelayMs =
              (next > kBackoffCapTransportMs || next < _wsReconnectDelayMs) ? kBackoffCapTransportMs : next;
        }
      } else {
        if (_wsReconnectDelayMs < kBackoffCapHttpMs) {
          uint32_t next = _wsReconnectDelayMs * 2u;
          _wsReconnectDelayMs = (next > kBackoffCapHttpMs || next < _wsReconnectDelayMs) ? kBackoffCapHttpMs : next;
        }
      }
      Serial.printf("[SerialWriterForwarder][ws-reconnect-backoff] delay_ms=%u last_signin_http=%d\n",
                    (unsigned)_wsReconnectDelayMs,
                    _lastSignInHttpCode);
      if (_lastSignInHttpCode < 0) {
        if (_consecutiveSignInTransportFailures < 255) {
          _consecutiveSignInTransportFailures++;
        }
        constexpr uint8_t kWifiRecoverAfterTcpFailures = 6;
        if (_consecutiveSignInTransportFailures >= kWifiRecoverAfterTcpFailures && WiFi.isConnected()) {
          Serial.printf(
              "[SerialWriterForwarder][wifi-recover] WiFi.reconnect() after %u consecutive TCP sign-in failures\n",
              (unsigned)_consecutiveSignInTransportFailures);
          WiFi.reconnect();
          _consecutiveSignInTransportFailures = 0;
        }
      } else {
        _consecutiveSignInTransportFailures = 0;
      }
      _lastWsReconnectAttempt = millis();
      return;
    }
    path += (path.indexOf('?') >= 0 ? "&" : "?");
    path += "access_token=" + _httpAuthToken;
    Serial.printf("[SerialWriterForwarder][ws-attempt] auth=token-ok user=%s token_len=%u\n",
                  _state.authUsername.c_str(),
                  (unsigned)_httpAuthToken.length());
  }

  _wsReconnectDelayMs = 5000;

  _wsClient = new WebSocketsClient();
  _wsAttemptCount++;
  _wsHadConnected = false;

  Serial.printf("[SerialWriterForwarder][ws-attempt] n=%u url='%s' host=%s port=%u path=%s wifi_ip=%s\n",
                (unsigned)_wsAttemptCount,
                _state.sourceUrl.c_str(),
                host.c_str(),
                (unsigned)port,
                path.c_str(),
                WiFi.localIP().toString().c_str());

  // Show progress in the UI immediately so a user clicking Save sees the next state instead of stale "Disconnected"
  {
    String progress = "Connecting to " + host + ":" + String((unsigned)port) + path;
    update([progress](SerialWriterForwarderState& state) {
      state.connected = false;
      // Don't clobber a more specific auth error set just above
      if (state.lastError.isEmpty() || state.lastError == "Disconnected" ||
          state.lastError == "Source closed (auth or path?)" ||
          state.lastError == "Source disconnected" ||
          state.lastError == "No WiFi") {
        state.lastError = progress;
      }
      return StateUpdateResult::CHANGED;
    }, "internal");
  }

  _wsClient->onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
    Serial.printf("[SerialWriterForwarder][ws-event] type=%s t=%lu len=%u\n",
                  wsTypeName(type),
                  (unsigned long)millis(),
                  (unsigned)length);

    if (type == WStype_CONNECTED) {
      _wsHadConnected = true;
      _wsReconnectDelayMs = 5000;
      _consecutiveSignInTransportFailures = 0;
      update([](SerialWriterForwarderState& state) {
        state.connected = true;
        state.lastError = "";
        return StateUpdateResult::CHANGED;
      }, "internal");
    } else if (type == WStype_DISCONNECTED) {
      logSerialWriterOutboundWsClosePayload(payload, length);
      // Distinguish "rejected on handshake" from "lost connection after a frame"
      const bool hadConnected = _wsHadConnected;
      update([hadConnected](SerialWriterForwarderState& state) {
        state.connected = false;
        state.lastError = hadConnected ? "Source disconnected" : "Source closed (auth or path?)";
        return StateUpdateResult::CHANGED;
      }, "internal");
      // Mid-session drops: keep JWT so the next attempt opens WebSocket first (same recovery pattern as pre-auth-first
      // builds). Handshake-only failures still clear the token so signIn runs again.
      if (hadConnected) {
        Serial.println(
            F("[SerialWriterForwarder][ws-token] kept after mid-session disconnect (retry WS before re-auth)"));
      } else {
        _httpAuthTokenValid = false;
        _httpAuthToken = "";
      }
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
      String detail;
      if (payload != nullptr && length > 0) {
        detail.reserve(length);
        for (size_t i = 0; i < length; ++i) {
          char c = (char)payload[i];
          if (c == '\0') break;
          detail += c;
        }
      }
      Serial.printf("[SerialWriterForwarder][ws-error] detail='%s'\n", detail.c_str());
      String error = detail.isEmpty() ? String("WS error") : (String("WS error: ") + detail);
      setRuntimeError(error, false);
    }
    // Other event types (PING/PONG/BIN/FRAGMENT*) are logged at the top of the lambda
  });

  _wsClient->begin(host, port, path);
  _wsClient->enableHeartbeat(15000, 3000, 2);
  _lastWsReconnectAttempt = millis();
#endif
}

void SerialWriterForwarderService::checkWsConnection() {
#ifdef ESP32
  if (_state.protocol != SERIAL_WRITER_FORWARDER_PROTOCOL_WS || !_state.enabled) {
    return;
  }

  // Always gate reconnects on WiFi so we don't spam TCP attempts on a dead radio
  if (!WiFi.isConnected()) {
    if (millis() - _lastWsReconnectAttempt >= 5000) {
      Serial.println(F("[SerialWriterForwarder][ws-reconnect] reason=wifi-down"));
      _lastWsReconnectAttempt = millis();
      setRuntimeError("No WiFi", false);
    }
    return;
  }

  if (!_wsClient) {
    if (millis() - _lastWsReconnectAttempt >= _wsReconnectDelayMs) {
      Serial.println(F("[SerialWriterForwarder][ws-reconnect] reason=no-client"));
      initWsClient();
    }
    return;
  }

  if (!_wsClient->isConnected()) {
    if (millis() - _lastWsReconnectAttempt >= _wsReconnectDelayMs) {
      Serial.println(F("[SerialWriterForwarder][ws-reconnect] reason=not-connected"));
      initWsClient();
    }
  }
#endif
}
