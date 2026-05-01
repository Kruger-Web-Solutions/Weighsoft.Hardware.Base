#include <examples/serialwriter/SerialWriterForwarderService.h>
#include <examples/serialwriter/SerialWriterService.h>

SerialWriterForwarderService::SerialWriterForwarderService(AsyncWebServer* server,
                                                           FS* fs,
                                                           SecurityManager* securityManager,
                                                           SerialWriterService* writerService) :
    _httpEndpoint(SerialWriterForwarderState::read,
                  SerialWriterForwarderState::update,
                  this,
                  server,
                  SERIALW_FWD_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED),
    _fsPersistence(SerialWriterForwarderState::readConfig,
                   SerialWriterForwarderState::updateConfig,
                   this,
                   fs,
                   SERIALW_FWD_CONFIG_FILE),
    _writerService(writerService) {
  addUpdateHandler([this](const String& origin) { onConfigChanged(); }, false);
}

void SerialWriterForwarderService::begin() {
  _fsPersistence.readFromFS();
  // Do NOT auto-start — UartModeService::applyMode() calls start() when entering Writer mode.
}

void SerialWriterForwarderService::loop() {
  if (!_running) return;
  _wsClient.loop();

  if (!_wsClient.isConnected() && millis() >= _nextRetryMs) {
    connectWs();
  }

  if (_wsClient.isConnected() && (millis() - _lastAnnounceMs) > 10000UL) {
    announce();
    _lastAnnounceMs = millis();
  }
}

void SerialWriterForwarderService::start() {
  bool shouldRun = false;
  read([&](const SerialWriterForwarderState& s) {
    shouldRun = s.enabled && s.sourceUrl.length() > 0;
  });
  if (!shouldRun) {
    _running = false;
    return;
  }
  _running = true;
  _backoffMs = 1000;
  connectWs();
}

void SerialWriterForwarderService::stop() {
  _running = false;
  disconnectWs();
}

void SerialWriterForwarderService::onConfigChanged() {
  _fsPersistence.writeToFS();
  if (_running) {
    disconnectWs();
    start();
  }
}

void SerialWriterForwarderService::connectWs() {
  String url;
  uint8_t method = SERIALW_FWD_METHOD_WS;
  read([&](const SerialWriterForwarderState& s) { url = s.sourceUrl; method = s.connectionMethod; });

  if (method != SERIALW_FWD_METHOD_WS) {
    update([&](SerialWriterForwarderState& s) {
      s.lastError = "HTTP polling not implemented in v1";
      return StateUpdateResult::CHANGED;
    }, "fwd-error");
    return;
  }

  // Strip "ws://" prefix and split host:port + path.
  String host;
  uint16_t port = 80;
  String path = "/ws/serial";
  if (url.startsWith("ws://")) url = url.substring(5);
  int slash = url.indexOf('/');
  String hostPort = slash >= 0 ? url.substring(0, slash) : url;
  if (slash >= 0) path = url.substring(slash);
  int colon = hostPort.indexOf(':');
  if (colon >= 0) {
    host = hostPort.substring(0, colon);
    port = hostPort.substring(colon + 1).toInt();
  } else {
    host = hostPort;
  }

  String fullPath = path + "?role=writer&id=" + SettingValue::getUniqueId();

  _announceUrl = String("http://") + host + ":" + String(port) + "/rest/writers/announce";

  _wsClient.begin(host, port, fullPath);
  _wsClient.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
    onWsEvent(type, payload, length);
  });
  _wsClient.setReconnectInterval(0);  // we manage backoff ourselves
}

void SerialWriterForwarderService::disconnectWs() {
  _wsClient.disconnect();
  update([](SerialWriterForwarderState& s) {
    s.connected = false;
    return StateUpdateResult::CHANGED;
  }, "fwd-disconnect");
}

void SerialWriterForwarderService::onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      update([](SerialWriterForwarderState& s) {
        s.connected = true;
        s.lastError = "";
        s.reconnectAttempts = 0;
        return StateUpdateResult::CHANGED;
      }, "fwd-connect");
      _backoffMs = 1000;
      announce();
      _lastAnnounceMs = millis();
      break;

    case WStype_DISCONNECTED:
      update([](SerialWriterForwarderState& s) {
        s.connected = false;
        return StateUpdateResult::CHANGED;
      }, "fwd-disconnect");
      scheduleRetry();
      break;

    case WStype_TEXT: {
      String line((const char*)payload, length);
      StaticJsonDocument<512> doc;
      if (deserializeJson(doc, line) == DeserializationError::Ok) {
        String lastLine = doc["last_line"] | "";
        if (lastLine.length() > 0 && _writerService) {
          _writerService->transmit(lastLine, TxSource::READER);
          update([&](SerialWriterForwarderState& s) {
            s.lastReceived = lastLine;
            s.lastReceivedAt = millis();
            return StateUpdateResult::CHANGED;
          }, "fwd-rx");
        }
      }
      break;
    }

    default:
      break;
  }
}

void SerialWriterForwarderService::scheduleRetry() {
  _nextRetryMs = millis() + _backoffMs;
  _backoffMs = _backoffMs >= 30000 ? 30000 : _backoffMs * 2;
  update([](SerialWriterForwarderState& s) {
    s.reconnectAttempts++;
    return StateUpdateResult::CHANGED;
  }, "fwd-retry");
}

void SerialWriterForwarderService::announce() {
  if (_announceUrl.length() == 0) return;

  String id   = SettingValue::getUniqueId();
  String name;
  if (_writerService) {
    _writerService->read([&](const SerialWriterState& s) {
      name = s.friendlyName;
    });
  }
  if (id.length() == 0) return;

  HTTPClient http;
  http.begin(_announceUrl);
  http.addHeader("Content-Type", "application/json");
  String body = String("{\"id\":\"") + id + "\",\"name\":\"" + name + "\"}";
  int status = http.POST(body);
  http.end();
  (void)status;  // errors are silently ignored; next heartbeat will retry
}
