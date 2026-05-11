#include <examples/serialwriter/SerialWriterService.h>

#ifdef ESP32
#include <HardwareSerial.h>
#include <esp_log.h>
#endif

bool g_usbOutputActive = false;

SerialWriterService::SerialWriterService(AsyncWebServer* server,
                                         FS* fs,
                                         SecurityManager* securityManager,
                                         AsyncMqttClient* mqttClient) :
    _httpEndpoint(SerialWriterState::read,
                  SerialWriterState::update,
                  this,
                  server,
                  SERIALW_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED),
    _fsPersistence(SerialWriterState::readConfig,
                   SerialWriterState::updateConfig,
                   this,
                   fs,
                   SERIALW_CONFIG_FILE),
    _mqttPubSub(SerialWriterState::read,
                SerialWriterState::update,
                this,
                mqttClient),
    _webSocket(SerialWriterState::read,
               SerialWriterState::update,
               this,
               server,
               SERIALW_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_AUTHENTICATED),
    _mqttClient(mqttClient),
    _inboundWs("/ws/serialWriter/inbound"),
    _server(server),
    _securityManager(securityManager) {
  // Disable FSPersistence auto-handler: runtime stat updates (tx events) would thrash flash.
  // We persist explicitly in onConfigUpdated() instead (only on actual config changes).
  _fsPersistence.disableUpdateHandler();

  addUpdateHandler([this](const String& origin) {
    // Skip runtime stat updates ("tx", "suspend", "resume") — only persist/reconfigure on
    // real config changes that arrive via REST/WS/MQTT.
    if (origin != "tx" && origin != "suspend" && origin != "resume" && origin != "init") {
      onConfigUpdated();
    }
  }, false);

  // Register MQTT message handler for inbound subscription-triggered transmits.
  if (mqttClient != nullptr) {
    mqttClient->onMessage([this](char* topic,
                                 char* payload,
                                 AsyncMqttClientMessageProperties /*props*/,
                                 size_t len,
                                 size_t /*index*/,
                                 size_t /*total*/) {
      this->onMqttMessage(String(topic), String(payload).substring(0, len));
    });
  }
}

void SerialWriterService::begin() {
  _fsPersistence.readFromFS();
  Serial.printf("[SerialWriter] Loaded config: %lu baud, %u%c%u\n",
                (unsigned long)_state.baudrate, _state.databits,
                _state.parity == 0 ? 'N' : (_state.parity == 1 ? 'E' : 'O'),
                _state.stopbits);
  registerSendEndpoint();
  registerWsSink();
  configureMqttSubscription();
  // Do NOT start the serial port here — UartModeService::applyMode() calls resumeWriter().
}

void SerialWriterService::loop() {
  // Reserved for periodic work (status heartbeat, etc.). No-op for v1.
}

HardwareSerial& SerialWriterService::outputSerial() {
  return _outputPort == SERIALW_OUTPUT_USB ? Serial : SERIALW_UART1_PORT;
}

void SerialWriterService::suspendWriter() {
  if (_suspended) return;
  if (_serialStarted && _outputPort != SERIALW_OUTPUT_USB) {
    SERIALW_UART1_PORT.end();
    _serialStarted = false;
    Serial.println(F("[SerialWriter] Serial1 stopped (suspended)"));
  }
  _suspended = true;
  update([&](SerialWriterState& s) {
    s.suspended = 1;
    s.serialStarted = 0;
    return StateUpdateResult::CHANGED;
  }, "suspend");
}

void SerialWriterService::resumeWriter() {
  if (!_suspended) return;
  applySerialConfig();
  _suspended = false;
  update([&](SerialWriterState& s) {
    s.suspended = 0;
    s.serialStarted = _serialStarted ? 1 : 0;
    return StateUpdateResult::CHANGED;
  }, "resume");
}

size_t SerialWriterService::transmit(const String& data, TxSource source) {
  if (_suspended || !_serialStarted) return 0;
  if (data.length() == 0) return 0;

  HardwareSerial& port = outputSerial();
  size_t written = port.print(data);
  String le = lineEndingChars();
  if (le.length() > 0) written += port.print(le);

  String composed = data + le;
  update([&](SerialWriterState& s) {
    s.lastSent = composed;
    s.lastSentAt = millis();
    s.lastSentSource = (uint8_t)source;
    s.bytesSentTotal += written;
    return StateUpdateResult::CHANGED;
  }, "tx");

  // broadcastTxEvent is called after the update above. The WebSocketTxRx _webSocket
  // already broadcasts the full state to all clients whenever update() fires with CHANGED,
  // which carries lastSent, lastSentSource, and bytesSentTotal. For v1 this is sufficient;
  // broadcastTxEvent adds nothing extra.
  broadcastTxEvent(composed, source);
  return written;
}

void SerialWriterService::applySerialConfig() {
#ifdef ESP32
  read([&](const SerialWriterState& s) { _outputPort = s.outputPort; });

  uint32_t baud = 0;
  read([&](const SerialWriterState& s) { baud = s.baudrate; });
  if (baud < SERIALW_MIN_BAUDRATE || baud > SERIALW_MAX_BAUDRATE) {
    baud = SERIALW_DEFAULT_BAUDRATE;
  }
  uint32_t mode = serialMode();

  if (_outputPort == SERIALW_OUTPUT_USB) {
    if (_serialStarted) {
      SERIALW_UART1_PORT.end();
    }
    Serial.printf("[SerialWriter] Switching to USB Serial: %lu baud — debug logs OFF\n", (unsigned long)baud);
    Serial.flush();
    delay(50);
    Serial.end();
    Serial.begin(baud, mode);
    delay(50);
    Serial.setDebugOutput(false);
    esp_log_level_set("*", ESP_LOG_NONE);
    g_usbOutputActive = true;
    _serialStarted = true;
    return;
  }

  // GPIO pin mode — restore debug output if we were in USB mode
  if (g_usbOutputActive) {
    g_usbOutputActive = false;
    Serial.end();
    Serial.begin(115200);
    delay(50);
    Serial.setDebugOutput(true);
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    Serial.println(F("[SerialWriter] Debug logs restored (back to GPIO output)"));
  }

  if (_serialStarted) {
    SERIALW_UART1_PORT.end();
    Serial.println(F("[SerialWriter] Stopping Serial1 for reconfiguration..."));
  }

  SERIALW_UART1_PORT.begin(baud, mode, SERIALW_RX_PIN, SERIALW_TX_PIN);
  delay(100);
  _serialStarted = true;

  Serial.printf("[SerialWriter] Serial1 started: %lu baud, RX=GPIO%d, TX=GPIO%d\n",
                (unsigned long)baud, SERIALW_RX_PIN, SERIALW_TX_PIN);
#endif
}

uint32_t SerialWriterService::serialMode() {
#ifdef ESP32
  uint8_t d = 8, p = 0, s = 1;
  read([&](const SerialWriterState& st) {
    d = st.databits;
    p = st.parity;
    s = st.stopbits;
  });
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

String SerialWriterService::lineEndingChars() {
  String result = "";
  read([&](const SerialWriterState& s) {
    switch (s.lineEnding) {
      case SERIALW_LE_CR:   result = "\r";   break;
      case SERIALW_LE_LF:   result = "\n";   break;
      case SERIALW_LE_CRLF: result = "\r\n"; break;
      default:              result = "";     break;
    }
  });
  return result;
}

void SerialWriterService::onConfigUpdated() {
  if (!g_usbOutputActive) {
    Serial.printf("[SerialWriter] Config updated - baud=%lu\n",
                  (unsigned long)_state.baudrate);
  }
  bool saved = _fsPersistence.writeToFS();
  if (!g_usbOutputActive) {
    Serial.printf("[SerialWriter] Config persisted: %s\n", saved ? "SUCCESS" : "FAILED");
  }
  if (!_suspended) applySerialConfig();
  configureMqttSubscription();
}

void SerialWriterService::registerSendEndpoint() {
  _server->on((String(SERIALW_ENDPOINT_PATH) + "/send").c_str(),
              HTTP_POST,
              [](AsyncWebServerRequest* req) {},
              nullptr,
              [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
                StaticJsonDocument<512> doc;
                if (deserializeJson(doc, data, len)) {
                  req->send(400, "application/json", "{\"error\":\"invalid json\"}");
                  return;
                }
                String text = doc["data"] | "";
                if (text.length() == 0) {
                  req->send(400, "application/json", "{\"error\":\"data required\"}");
                  return;
                }
                size_t n = transmit(text, TxSource::REST);
                if (n == 0) {
                  req->send(503, "application/json", "{\"error\":\"writer suspended or serial not started\"}");
                } else {
                  req->send(200, "application/json", "{\"sent\":true}");
                }
              });
}

void SerialWriterService::registerWsSink() {
  // Mount a dedicated inbound WebSocket at /ws/serialWriter/inbound.
  //
  // Pattern rationale: the main _webSocket (WebSocketTxRx at /ws/serialWriter) is a
  // stateful config channel — its WebSocketRx handler passes frames through
  // SerialWriterState::update(), which updates config fields (baud_rate, etc.), not
  // trigger serial transmits.  We need a separate channel whose frames are forwarded
  // directly to transmit().  A plain AsyncWebSocket gives us full control over the
  // onEvent handler without interfering with the state machinery.
  //
  // Security: the filter mirrors what SerialService does for its state WebSocket —
  // IS_AUTHENTICATED.  Unauthenticated clients receive 403 via the GET handler below.

  _inboundWs.setFilter(_securityManager->filterRequest(AuthenticationPredicates::IS_AUTHENTICATED));
  _inboundWs.onEvent([this](AsyncWebSocket* /*server*/,
                             AsyncWebSocketClient* client,
                             AwsEventType type,
                             void* arg,
                             uint8_t* data,
                             size_t len) {
    if (type == WS_EVT_CONNECT) {
      Serial.printf("[SerialWriter/inbound] Client connected: %u\n", client->id());
    } else if (type == WS_EVT_DISCONNECT) {
      Serial.printf("[SerialWriter/inbound] Client disconnected: %u\n", client->id());
    } else if (type == WS_EVT_DATA) {
      AwsFrameInfo* info = (AwsFrameInfo*)arg;
      // Only handle complete, single-frame text messages (mirrors WebSocketRx pattern)
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        // Accept either plain text or JSON {"data":"..."}
        String payload = String((char*)data).substring(0, len);
        if (payload.length() == 0) return;

        String textToSend = payload;  // default: treat frame as raw text

        // Try to parse as JSON {"data":"..."} — if it fails, fall through to raw text
        StaticJsonDocument<512> doc;
        if (!deserializeJson(doc, payload) && doc.is<JsonObject>() && doc.containsKey("data")) {
          textToSend = doc["data"].as<String>();
        }

        if (textToSend.length() > 0) {
          transmit(textToSend, TxSource::WS);
        }
      }
    }
  });
  _server->addHandler(&_inboundWs);
  // Forbidden response for GET requests that bypass the WS upgrade
  _server->on("/ws/serialWriter/inbound", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(403);
  });
}

void SerialWriterService::configureMqttSubscription() {
  if (_mqttClient == nullptr) return;
  if (!_mqttClient->connected()) return;
  read([&](const SerialWriterState& s) {
    if (s.mqttEnabled && s.mqttSubscribeTopic.length() > 0) {
      _mqttClient->subscribe(s.mqttSubscribeTopic.c_str(), 0);
      Serial.printf("[SerialWriter] MQTT subscribed to: %s\n", s.mqttSubscribeTopic.c_str());
    }
  });
}

void SerialWriterService::onMqttMessage(const String& topic, const String& payload) {
  bool shouldSend = false;
  read([&](const SerialWriterState& s) {
    shouldSend = s.mqttEnabled &&
                 s.mqttSubscribeTopic.length() > 0 &&
                 topic == s.mqttSubscribeTopic;
  });
  if (shouldSend) {
    transmit(payload, TxSource::MQTT);
  }
}

void SerialWriterService::broadcastTxEvent(const String& /*data*/, TxSource /*source*/) {
  // No-op: the update() call in transmit() already triggers the WebSocketTxRx _webSocket
  // to broadcast the full serialised state (including lastSent, lastSentSource,
  // bytesSentTotal) to every connected client via WebSocketTx::transmitData().
  // A separate push-only "tx" event envelope is not needed for v1; revisit if the UI
  // requires a lightweight event stream distinct from the full state snapshot.
}
