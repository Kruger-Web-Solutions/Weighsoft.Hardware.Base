#include <examples/serialwriter/SerialWriterForwarderService.h>
#include <examples/serialwriter/SerialWriterService.h>
#include <WiFi.h>
#include <trade_logic.h>

#ifndef SERIALW_FWD_REST_POLL_INTERVAL_MS
#define SERIALW_FWD_REST_POLL_INTERVAL_MS 1000
#endif

#ifndef SERIALW_FWD_WS_BACKUP_POLL_INTERVAL_MS
#define SERIALW_FWD_WS_BACKUP_POLL_INTERVAL_MS 5000
#endif

// Time without WS traffic before backup REST polling kicks in. Increased from 5s to 20s
// because slow scales emit lines every ~5s and any minor jitter previously triggered
// backup polling, which under load could cause unnecessary WS reconnect cycles.
#ifndef SERIALW_FWD_WS_STALE_MS
#define SERIALW_FWD_WS_STALE_MS 20000
#endif

// A W-Soft weigh board's /ws/wsoftFeed sends every 0.5 s. No message for this long = the
// socket is dead (it can die without a close): drop it and connect again, and poll the REST
// twin every SERIALW_FWD_WSOFT_POLL_MS meanwhile so the weight keeps moving.
#ifndef SERIALW_FWD_WSOFT_STALE_MS
#define SERIALW_FWD_WSOFT_STALE_MS 3000
#endif
#ifndef SERIALW_FWD_WSOFT_POLL_MS
#define SERIALW_FWD_WSOFT_POLL_MS 1000
#endif
#ifndef SERIALW_FWD_WSOFT_MAX_BACKOFF_MS
#define SERIALW_FWD_WSOFT_MAX_BACKOFF_MS 4000
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
  _fsPersistence.disableUpdateHandler();
  addUpdateHandler([this](const String& origin) {
    if (!origin.startsWith("fwd-")) onConfigChanged();
  }, false);

#ifdef ESP32
  WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) { onWiFiGotIP(); },
               WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
#endif
}

void SerialWriterForwarderService::begin() {
  _fsPersistence.readFromFS();
  // Do NOT auto-start — UartModeService::applyMode() calls start() when entering Writer mode.
}

void SerialWriterForwarderService::loop() {
  if (!_running) return;

  const unsigned long now = millis();

  const bool wifiUp = WiFi.status() == WL_CONNECTED;
  if (!wifiUp) {
    _wifiWasConnected = false;
    if (now >= _nextRetryMs) {
      _nextRetryMs = now + 1000UL;
    }
    return;
  }
  if (!_wifiWasConnected) {
    _wifiWasConnected = true;
    _backoffMs = 1000;
    _nextRetryMs = now;
    _readerPollFailures = 0;
  }

  if (_activeConnectionMethod == SERIALW_FWD_METHOD_WS) {
    _wsClient.loop();
    // millis(), not `now`: the loop above may have just stamped _lastWsDataMs after `now`.
    if (_wsoftFeed && _wsClient.isConnected() &&
        (unsigned long)(millis() - _lastWsDataMs) > (unsigned long)SERIALW_FWD_WSOFT_STALE_MS) {
      Serial.println(F("[SerialWriterForwarder] weigh board feed quiet for 3 s: reconnecting"));
      disconnectWs();
      _backoffMs = 1000;
      _nextRetryMs = now;
    }
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
    if (_wsoftFeed) {
      // Judge the socket by its own messages: a REST poll that works must not hide a dead socket.
      const bool wsLive = _wsClient.isConnected() && _lastWsDataMs > 0 &&
                          (unsigned long)(millis() - _lastWsDataMs) < (unsigned long)SERIALW_FWD_WSOFT_STALE_MS;
      shouldPollRest = _serialRestUrl.length() > 0 && !wsLive;
      pollInterval = SERIALW_FWD_WSOFT_POLL_MS;
    }
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
  // A W-Soft weigh board keeps no Writers list, so there is nothing to announce to.
  _wsoftFeed = path.startsWith("/ws/wsoftFeed");
  _lastWsDataMs = 0;
  _announceUrl = _wsoftFeed ? String() : String("http://") + host + ":" + String(port) + "/rest/writers";
  // The REST twin of the socket: /ws/serial -> /rest/serial, /ws/wsoftFeed -> /rest/wsoftFeed.
  String restPath = path.startsWith("/ws/") ? String("/rest/") + path.substring(4) : String("/rest/serial");
  _serialRestUrl = String("http://") + host + ":" + String(port) + restPath;

  _wsClient.begin(host, port, fullPath);
  // No heartbeat: actual data lines from the Reader (~every 5s) act as natural
  // keepalive. Earlier heartbeat config (15s ping, 3s pong timeout, 2 misses) caused
  // repeated disconnect/reconnect cycles when the Reader's async server was busy
  // serving REST or other WS clients and missed the pong window.
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

void SerialWriterForwarderService::onWiFiGotIP() {
  if (!_running) return;
  Serial.printf("[SerialWriterForwarder] WiFi reconnected (IP: %s) — resetting backoff for immediate Reader reconnect\n",
                WiFi.localIP().toString().c_str());
  _backoffMs = 1000;
  _nextRetryMs = millis();
  _readerPollFailures = 0;
  _lastReaderOkMs = 0;
  _lastAnnounceMs = 0;

  if (_writerService) {
    _writerService->clearPendingTx();
  }
}

void SerialWriterForwarderService::onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      markReaderOk();
      _lastWsDataMs = millis();
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
      StaticJsonDocument<1024> doc;
      if (deserializeJson(doc, payload, length) == DeserializationError::Ok) {
        markReaderOk();
        _lastWsDataMs = millis();
        onReaderData(doc["payload"].is<JsonObject>() ? doc["payload"].as<JsonObject>() : doc.as<JsonObject>());
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
  const uint16_t maxBackoff = _wsoftFeed ? SERIALW_FWD_WSOFT_MAX_BACKOFF_MS : 30000;
  _backoffMs = _backoffMs >= maxBackoff ? maxBackoff : _backoffMs * 2;
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

  onReaderData(doc.as<JsonObject>());
}

// One reading from the Reader (WebSocket push or REST poll): format it, send it out of the
// port, and keep it for the page and the W-Soft screen. A reading already handled (same
// timestamp, e.g. the backup REST poll) is not sent twice. Nothing is sent without a line,
// or when the format needs a weight the reading does not have.
void SerialWriterForwarderService::onReaderData(JsonObject data) {
  const char* line = data["last_line"] | "";
  if (line[0] == '\0' || !_writerService) return;
  uint32_t timestamp = data["timestamp"] | (uint32_t)0;
  if (timestamp != 0 && timestamp == _lastTimestamp) return;
  _lastTimestamp = timestamp;

  WsoftOutputFields fields;
  fields.line = line;
  fields.weight = data["weight"] | "";
  fields.unit = data["unit"] | "";
  fields.has_stable = data["stable"].is<bool>();
  fields.stable = data["stable"] | false;
  String format;
  read([&](const SerialWriterForwarderState& s) { format = s.outputFormat; });
  // The default "{line}" sends the line exactly as received, whatever its length.
  String sent = line;
  bool ok = true;
  if (format.length() > 0 && format != WSOFT_OUTPUT_FORMAT_DEFAULT) {
    char out[WSOFT_OUTPUT_FORMAT_MAX * 2];
    ok = wsoft_format_output(format.c_str(), fields, out, sizeof out);
    sent = ok ? String(out) : String();
  }
  if (ok) _writerService->transmit(sent, TxSource::READER);

  update([&](SerialWriterForwarderState& s) {
    s.lastReceived = line;
    s.lastWeight = fields.weight;
    s.lastUnit = fields.unit;
    s.lastSent = sent;
    s.hasProduct = data.containsKey("product");
    s.lastProduct = data["product"] | "";
    s.lastReceivedAt = millis();
    s.lastError = ok ? "" : "Not sent: the format needs a weight, or the line is too long";
    return StateUpdateResult::CHANGED;
  }, "fwd-rx");
}

bool SerialWriterForwarderService::latestProduct(String* code) {
  bool has = false;
  read([&](const SerialWriterForwarderState& s) {
    has = s.hasProduct;
    *code = s.lastProduct;
  });
  return has;
}

bool SerialWriterForwarderService::latestWeight(char* weight, size_t size, uint32_t* receivedMs, int* unit) {
  bool have = false;
  read([&](const SerialWriterForwarderState& s) {
    if (s.lastWeight.length() == 0 || s.lastReceivedAt == 0) return;
    strlcpy(weight, s.lastWeight.c_str(), size);
    *receivedMs = (uint32_t)s.lastReceivedAt;
    for (int code = 1; code <= 3; code++) {
      if (s.lastUnit == weight_unit_text(code)) *unit = code;
    }
    have = true;
  });
  return have;
}

void SerialWriterForwarderService::markReaderOk() {
  _readerPollFailures = 0;
  _lastReaderOkMs = millis();
}

void SerialWriterForwarderService::handleReaderPollFailure(const String& message) {
  if (_readerPollFailures < 255) {
    _readerPollFailures++;
  }

  // In WS mode, REST polling is only a backup health check. If WS is connected, do NOT
  // force a WS disconnect or flip connected=false just because backup polling failed —
  // this previously caused gratuitous reconnect cycles when the Reader was briefly slow
  // to answer HTTP. Only surface a poll error to the UI if WS is also down.
  const bool wsMode = _activeConnectionMethod == SERIALW_FWD_METHOD_WS;
  const bool wsDown = wsMode && !_wsClient.isConnected();
  const bool shouldShowError = !wsMode || wsDown;

  update([&](SerialWriterForwarderState& s) {
    if (!wsMode || wsDown) {
      s.connected = false;
    }
    if (shouldShowError) {
      s.lastError = message;
    }
    return StateUpdateResult::CHANGED;
  }, "fwd-error");
}
