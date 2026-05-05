#include <examples/serialwriter/SerialWriterForwarderService.h>
#include <examples/serialwriter/SerialWriterService.h>
#include <WiFi.h>

#ifndef SERIALW_FWD_REST_POLL_INTERVAL_MS
#define SERIALW_FWD_REST_POLL_INTERVAL_MS 1000
#endif

#ifndef SERIALW_FWD_WS_BACKUP_POLL_INTERVAL_MS
#define SERIALW_FWD_WS_BACKUP_POLL_INTERVAL_MS 3000
#endif

#ifndef SERIALW_FWD_WS_STALE_MS
#define SERIALW_FWD_WS_STALE_MS 5000
#endif

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
  // Runtime connection fields change on every reconnect/receive cycle. Keep
  // those in RAM and persist only user-supplied forwarding configuration.
  _fsPersistence.disableUpdateHandler();
  addUpdateHandler([this](const String& origin) {
    if (!origin.startsWith("fwd-")) onConfigChanged();
  }, false);
}

void SerialWriterForwarderService::begin() {
  _fsPersistence.readFromFS();
  // Do NOT auto-start — UartModeService::applyMode() calls start() when entering Writer mode.
}

void SerialWriterForwarderService::loop() {
  if (!_running) return;

  const unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (now >= _nextRetryMs) {
      _nextRetryMs = now + 1000UL;
    }
    return;
  }

  if (_activeConnectionMethod == SERIALW_FWD_METHOD_WS) {
    _wsClient.loop();
    if (!_wsClient.isConnected() && now >= _nextRetryMs) {
      yield();
      connectReader();
      yield();
    }
  } else if (_serialRestUrl.length() == 0 && now >= _nextRetryMs) {
    yield();
    connectReader();
    yield();
  }

  // In WS mode this is only a backup health check; continuous REST polling from every
  // writer adds needless load to the reader when several writers are connected.
  bool shouldPollRest = _serialRestUrl.length() > 0;
  unsigned long pollInterval = SERIALW_FWD_REST_POLL_INTERVAL_MS;
  if (_activeConnectionMethod == SERIALW_FWD_METHOD_WS) {
    const bool wsRecentlyOk =
        _lastReaderOkMs > 0 && (unsigned long)(now - _lastReaderOkMs) < (unsigned long)SERIALW_FWD_WS_STALE_MS;
    shouldPollRest = shouldPollRest && (!_wsClient.isConnected() || !wsRecentlyOk);
    pollInterval = SERIALW_FWD_WS_BACKUP_POLL_INTERVAL_MS;
  }
  if (shouldPollRest && (unsigned long)(now - _lastPollMs) > pollInterval) {
    pollReaderRest();
    _lastPollMs = now;
  }

  if (_announceUrl.length() > 0 && (unsigned long)(now - _lastAnnounceMs) > 10000UL) {
    const bool ready = _activeConnectionMethod == SERIALW_FWD_METHOD_HTTP || _wsClient.isConnected();
    if (ready) {
      announce();
      _lastAnnounceMs = now;
    }
  }
}

void SerialWriterForwarderService::start() {
  bool shouldRun = false;
  read([&](const SerialWriterForwarderState& s) {
    shouldRun = s.enabled && s.sourceUrl.length() > 0;
    _activeConnectionMethod = s.connectionMethod;
  });
  if (!shouldRun) {
    _running = false;
    _wsClient.disconnect();
    _serialRestUrl = "";
    _announceUrl = "";
    return;
  }
  _running = true;
  _backoffMs = 1000;
  _readerPollFailures = 0;
  _lastReaderOkMs = 0;
  _announceUrl = "";
  _serialRestUrl = "";
  _nextRetryMs = millis() + 1000UL;
}

void SerialWriterForwarderService::stop() {
  _running = false;
  disconnectWs();
}

void SerialWriterForwarderService::onConfigChanged() {
  _fsPersistence.writeToFS();
  if (_running) {
    disconnectWs();
  }
  start();
}

void SerialWriterForwarderService::connectReader() {
  String url;
  read([&](const SerialWriterForwarderState& s) {
    url = s.sourceUrl;
    _activeConnectionMethod = s.connectionMethod;
  });

  if (WiFi.status() != WL_CONNECTED) {
    update([](SerialWriterForwarderState& s) {
      s.connected = false;
      s.lastError = "WiFi is not connected";
      return StateUpdateResult::CHANGED;
    }, "fwd-wifi");
    scheduleRetry();
    return;
  }

  // Older UI saved ws:// URLs even when "HTTP polling" was selected; use WebSocket in that case.
  if (_activeConnectionMethod == SERIALW_FWD_METHOD_HTTP && (url.startsWith("ws://") || url.startsWith("wss://"))) {
    _activeConnectionMethod = SERIALW_FWD_METHOD_WS;
  }

  if (_activeConnectionMethod == SERIALW_FWD_METHOD_HTTP) {
    _wsClient.disconnect();
    String u = url;
    u.trim();
    if (u.length() == 0) {
      return;
    }
    if (!u.startsWith("http://") && !u.startsWith("https://")) {
      u = "http://" + u;
    }
    bool isTls = u.startsWith("https://");
    if (isTls) {
      update([&](SerialWriterForwarderState& s) {
        s.connected = false;
        s.lastError = "https Reader URL is not supported on device; use http:// or WebSocket";
        return StateUpdateResult::CHANGED;
      }, "fwd-error");
      _announceUrl = "";
      _serialRestUrl = "";
      return;
    }

    String rest = u.substring(7);
    int slash = rest.indexOf('/');
    String hostPort = slash >= 0 ? rest.substring(0, slash) : rest;
    String path = "/rest/serial";
    if (slash >= 0) {
      path = rest.substring(slash);
    }
    if (path.length() == 0 || path == "/") {
      path = "/rest/serial";
    }

    String host;
    uint16_t port = 80;
    int colon = hostPort.indexOf(':');
    if (colon >= 0) {
      host = hostPort.substring(0, colon);
      port = (uint16_t)hostPort.substring(colon + 1).toInt();
    } else {
      host = hostPort;
    }

    String accessToken = fetchReaderAccessToken(host, port);
    if (accessToken.length() > 0) {
      _readerAccessToken = accessToken;
    }

    _announceUrl = String("http://") + host + ":" + String(port) + "/rest/writers";
    _serialRestUrl = String("http://") + host + ":" + String(port) + path;

    update([](SerialWriterForwarderState& s) {
      s.lastError = "";
      return StateUpdateResult::CHANGED;
    }, "fwd-http-ready");
    return;
  }

  // WebSocket (default)
  if (url.startsWith("http://") || url.startsWith("https://")) {
    update([&](SerialWriterForwarderState& s) {
      s.lastError = "Use WebSocket URL (ws://...) or switch to HTTP polling for http:// Reader address";
      return StateUpdateResult::CHANGED;
    }, "fwd-error");
    return;
  }

  _wsClient.disconnect();

  String host;
  uint16_t port = 80;
  String path = "/ws/serial";
  if (url.startsWith("ws://")) {
    url = url.substring(5);
  } else if (url.startsWith("wss://")) {
    update([&](SerialWriterForwarderState& s) {
      s.lastError = "wss:// is not supported on the Writer device";
      return StateUpdateResult::CHANGED;
    }, "fwd-error");
    return;
  }

  int slash = url.indexOf('/');
  String hostPort = slash >= 0 ? url.substring(0, slash) : url;
  if (slash >= 0) path = url.substring(slash);
  int colon = hostPort.indexOf(':');
  if (colon >= 0) {
    host = hostPort.substring(0, colon);
    port = (uint16_t)hostPort.substring(colon + 1).toInt();
  } else {
    host = hostPort;
  }

  String fullPath = path + "?role=writer&id=" + SettingValue::getUniqueId();
  String accessToken = fetchReaderAccessToken(host, port);
  if (accessToken.length() > 0) {
    _readerAccessToken = accessToken;
  }

  if (_readerAccessToken.length() > 0) {
    fullPath += "&access_token=" + _readerAccessToken;
  }

  _announceUrl = String("http://") + host + ":" + String(port) + "/rest/writers";
  _serialRestUrl = String("http://") + host + ":" + String(port) + "/rest/serial";

  _wsClient.begin(host, port, fullPath);
  _wsClient.enableHeartbeat(15000, 3000, 2);
  _wsClient.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
    onWsEvent(type, payload, length);
  });

  scheduleRetry();
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
      markReaderOk();
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

    case WStype_ERROR:
      update([](SerialWriterForwarderState& s) {
        s.connected = false;
        s.lastError = "Reader WebSocket connection error";
        return StateUpdateResult::CHANGED;
      }, "fwd-error");
      scheduleRetry();
      break;

    case WStype_TEXT: {
      String line((const char*)payload, length);
      StaticJsonDocument<1024> doc;
      if (deserializeJson(doc, line) == DeserializationError::Ok) {
        JsonObject payload = doc["payload"].is<JsonObject>() ? doc["payload"].as<JsonObject>() : doc.as<JsonObject>();
        String lastLine = payload["last_line"] | "";
        if (lastLine.length() > 0 && _writerService) {
          markReaderOk();
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
  unsigned long retryDelay = _backoffMs;
  _nextRetryMs = millis() + retryDelay;
  _wsClient.setReconnectInterval(retryDelay);
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
  http.setTimeout(1500);
  http.addHeader("Content-Type", "application/json");
  if (_readerAccessToken.length() > 0) {
    http.addHeader("Authorization", "Bearer " + _readerAccessToken);
  }
  String ip = WiFi.localIP().toString();
  String body = String("{\"id\":\"") + id + "\",\"name\":\"" + name + "\",\"ip\":\"" + ip + "\"}";
  int status = http.POST(body);
  http.end();
  (void)status;  // errors are silently ignored; next heartbeat will retry
}

void SerialWriterForwarderService::pollReaderRest() {
  if (_serialRestUrl.length() == 0 || !_writerService) return;

  HTTPClient http;
  http.begin(_serialRestUrl);
  http.setTimeout(1500);
  if (_readerAccessToken.length() > 0) {
    http.addHeader("Authorization", "Bearer " + _readerAccessToken);
  }

  int status = http.GET();
  if (status != 200) {
    http.end();
    handleReaderPollFailure(String("Reader REST poll failed (HTTP ") + String(status) + ")");
    return;
  }

  String body = http.getString();
  http.end();

  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    handleReaderPollFailure("Reader REST poll returned invalid JSON");
    return;
  }

  markReaderOk();

  if (_activeConnectionMethod == SERIALW_FWD_METHOD_HTTP) {
    bool needMark = false;
    read([&](const SerialWriterForwarderState& s) {
      needMark = !s.connected || s.lastError.length() > 0;
    });
    if (needMark) {
      update([&](SerialWriterForwarderState& s) {
        s.connected = true;
        s.lastError = "";
        return StateUpdateResult::CHANGED;
      }, "fwd-poll-ok");
    }
  }

  String lastLine = doc["last_line"] | "";
  if (lastLine.length() == 0) return;

  bool duplicate = false;
  read([&](const SerialWriterForwarderState& s) {
    duplicate = s.lastReceived == lastLine;
  });
  if (duplicate) return;

  _writerService->transmit(lastLine, TxSource::READER);
  update([&](SerialWriterForwarderState& s) {
    s.lastReceived = lastLine;
    s.lastReceivedAt = millis();
    s.lastError = "";
    return StateUpdateResult::CHANGED;
  }, "fwd-rx");
}

void SerialWriterForwarderService::markReaderOk() {
  _readerPollFailures = 0;
  _lastReaderOkMs = millis();
}

void SerialWriterForwarderService::handleReaderPollFailure(const String& message) {
  if (_readerPollFailures < 255) {
    _readerPollFailures++;
  }

  const bool wsMode = _activeConnectionMethod == SERIALW_FWD_METHOD_WS;
  const bool wsStale = wsMode && (_readerPollFailures >= 3);
  const bool shouldShowError = _activeConnectionMethod == SERIALW_FWD_METHOD_HTTP || !_wsClient.isConnected() || wsStale;

  update([&](SerialWriterForwarderState& s) {
    if (_activeConnectionMethod == SERIALW_FWD_METHOD_HTTP || wsStale) {
      s.connected = false;
    }
    if (shouldShowError) {
      s.lastError = message;
    }
    return StateUpdateResult::CHANGED;
  }, "fwd-error");

  if (wsStale) {
    _wsClient.disconnect();
    _nextRetryMs = millis() + 1000UL;
    if (_backoffMs < 1000) {
      _backoffMs = 1000;
    }
  }
}

String SerialWriterForwarderService::fetchReaderAccessToken(const String& host, uint16_t port) {
  HTTPClient http;
  String url = String("http://") + host + ":" + String(port) + "/rest/signIn";
  http.begin(url);
  http.setTimeout(3000);
  http.addHeader("Content-Type", "application/json");

  int status = http.POST("{\"username\":\"admin\",\"password\":\"admin\"}");
  if (status != 200) {
    http.end();
    Serial.printf("[SerialWriterForwarder] Reader sign-in skipped/failed (HTTP %d); continuing without token\n", status);
    return "";
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, payload) != DeserializationError::Ok) {
    Serial.println(F("[SerialWriterForwarder] Reader sign-in returned invalid JSON; continuing without token"));
    return "";
  }

  return doc["access_token"] | "";
}
