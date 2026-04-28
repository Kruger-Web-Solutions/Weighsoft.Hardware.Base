#include <examples/remoteweight/RemoteWeightService.h>

#include <AsyncJson.h>
#include <Esp.h>

namespace {
inline bool heapBelowThreshold() {
  return ESP.getFreeHeap() < REMOTE_WEIGHT_MIN_FREE_HEAP;
}
}  // namespace

RemoteWeightService::RemoteWeightService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager) :
    _server(server),
    _securityManager(securityManager),
    _httpEndpoint(RemoteWeightState::read,
                  // Wrap the canonical update function with a heap-pressure
                  // guard. When the device is starving for heap we drop the
                  // POST entirely instead of propagating into AsyncWebSocket
                  // broadcasts, FS writes, etc. — those are exactly the
                  // operations that fragment the heap further.
                  [this](JsonObject& root, RemoteWeightState& state) {
                    if (heapBelowThreshold()) {
                      registerPostDropped();
                      return StateUpdateResult::UNCHANGED;
                    }
                    StateUpdateResult result = RemoteWeightState::update(root, state);
                    bool isWeightPayload = root.containsKey("weight") || root.containsKey("last_line");
                    if (isWeightPayload) {
                      registerPostReceived();
                      if (result == StateUpdateResult::CHANGED) {
                        recordAudit(RemoteWeightAuditReason::WEIGHT_CHANGED);
                      }
                    }
                    return result;
                  },
                  this,
                  server,
                  REMOTE_WEIGHT_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED),
    _fsPersistence(RemoteWeightState::readConfig,
                   RemoteWeightState::updateConfig,
                   this,
                   fs,
                   REMOTE_WEIGHT_CONFIG_FILE),
    _webSocket(RemoteWeightState::read,
               RemoteWeightState::update,
               this,
               server,
               REMOTE_WEIGHT_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_AUTHENTICATED) {
  _fsPersistence.disableUpdateHandler();

  addUpdateHandler([this](const String& originId) {
    // Detect "config update" vs "weight payload":
    //   - weight payloads bump _state.timestamp (set in RemoteWeightState::update)
    //   - config saves leave timestamp untouched
    bool isWeightUpdate = (_state.timestamp != _lastSeenTimestamp);
    _lastSeenTimestamp = _state.timestamp;

    if (!isWeightUpdate) {
      // CONFIG SAVE — persist to flash. FSPersistence::writeToFS() has no
      // skip-if-unchanged guard, so writing on every weight payload would
      // allocate a 1 KB JSON doc and open a flash file thousands of times
      // an hour, draining the heap until the OOM guard triggers and POSTs
      // get dropped. Persisting only on config changes (rare) keeps heap
      // stable for weeks of continuous operation.
      _fsPersistence.writeToFS();
      return;
    }

    // Real-time USB echo on every weight payload from the Forwarder. Safe
    // on the AsyncTCP task: Serial (HWCDC) has setTxTimeoutMs(0) and
    // Serial0 (UART0) is non-blocking by default.
    if (_state.usbEchoEnabled && !_state.lastLine.isEmpty()) {
      Serial.println(_state.lastLine);
#if ARDUINO_USB_CDC_ON_BOOT
      Serial0.println(_state.lastLine);
#endif
      recordAudit(RemoteWeightAuditReason::ECHO_FIRED);
    }
  }, false);
}

void RemoteWeightService::begin() {
  _fsPersistence.readFromFS();
  _state.weight = "";
  _state.lastLine = "";
  _state.timestamp = 0;
  _lastCleanupMillis = millis();
  _lastPeriodicMillis = millis();
  _minHeapSeen = ESP.getFreeHeap();

  registerAuditEndpoint();
  recordAudit(RemoteWeightAuditReason::BOOT);

  Serial.printf("[RemoteWeight] Service ready — enabled=%d displayEnabled=%d usbEcho=%d freeHeap=%lu\n",
                _state.enabled, _state.displayEnabled, _state.usbEchoEnabled,
                (unsigned long)ESP.getFreeHeap());
}

void RemoteWeightService::loop() {
  unsigned long now = millis();

  // Reap disconnected WebSocket clients. The framework leaves stale entries
  // in the client list when a browser tab closes / refreshes / network drops.
  // Each entry holds a TX-queue buffer; left unreaped, they accumulate and
  // fragment the heap.
  //
  // We also flip each live client into "close-on-queue-full" mode here so a
  // slow / hung browser tab can't grow its TX queue without bound — when its
  // queue fills, AsyncWebSocket closes the socket and the next cleanupClients
  // tick reaps it. This is the second half of the heap-leak protection.
  if (now - _lastCleanupMillis >= REMOTE_WEIGHT_WS_CLEANUP_INTERVAL_MS) {
    _lastCleanupMillis = now;
    AsyncWebSocket& ws = _webSocket.getWebSocket();
    for (AsyncWebSocketClient& c : ws.getClients()) {
      if (!c.willCloseClientOnQueueFull()) {
        c.setCloseClientOnQueueFull(true);
      }
    }
    ws.cleanupClients();
  }

  // Periodic heap snapshot every 30 s. Helps catch slow leaks early.
  if (now - _lastPeriodicMillis >= 30000UL) {
    _lastPeriodicMillis = now;
    uint32_t h = ESP.getFreeHeap();
    if (h < _minHeapSeen) _minHeapSeen = h;
    recordAudit(RemoteWeightAuditReason::PERIODIC);

    Serial.printf("[RemoteWeight] heap=%lu min=%lu posts=%u dropped=%u ws_clients=%u\n",
                  (unsigned long)h, (unsigned long)_minHeapSeen,
                  _postsTotal, _postsDropped,
                  (unsigned)_webSocket.getWebSocket().count());
  }
}

void RemoteWeightService::registerPostReceived() {
  _postsTotal++;
  uint32_t h = ESP.getFreeHeap();
  if (h < _minHeapSeen) _minHeapSeen = h;
  recordAudit(RemoteWeightAuditReason::POST_RECEIVED);
}

void RemoteWeightService::registerPostDropped() {
  _postsDropped++;
  uint32_t h = ESP.getFreeHeap();
  if (h < _minHeapSeen) _minHeapSeen = h;
  recordAudit(RemoteWeightAuditReason::POST_DROPPED_HEAP);

  // Rate-limit the console log so a sustained heap-pressure event doesn't
  // flood the USB-CDC port (one line per dropped POST at 5 Hz fills the
  // terminal in seconds and obscures everything else). Drops are still
  // counted in _postsDropped and logged in the periodic 30 s summary.
  static unsigned long lastDropLog = 0;
  unsigned long now = millis();
  if (now - lastDropLog >= 5000UL) {
    lastDropLog = now;
    Serial.printf("[RemoteWeight] DROPPING POSTs — free_heap=%lu min=%lu threshold=%u dropped_total=%u\n",
                  (unsigned long)h, (unsigned long)_minHeapSeen,
                  REMOTE_WEIGHT_MIN_FREE_HEAP, _postsDropped);
  }
}

void RemoteWeightService::recordAudit(RemoteWeightAuditReason reason) {
  RemoteWeightAuditEntry& e = _audit[_auditHead];
  e.millis_at = millis();
  e.free_heap = ESP.getFreeHeap();
  e.min_heap = _minHeapSeen;
  e.max_alloc = ESP.getMaxAllocHeap();
  e.posts_total = _postsTotal;
  e.posts_dropped = _postsDropped;
  e.reason = static_cast<uint8_t>(reason);
  e.ws_clients = static_cast<uint8_t>(_webSocket.getWebSocket().count());

  _auditHead = (_auditHead + 1) % REMOTE_WEIGHT_AUDIT_CAPACITY;
  if (_auditCount < REMOTE_WEIGHT_AUDIT_CAPACITY) _auditCount++;
}

void RemoteWeightService::registerAuditEndpoint() {
  _server->on(REMOTE_WEIGHT_AUDIT_PATH, HTTP_GET,
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

void RemoteWeightService::readAuditAndStats(RemoteWeightService* self, JsonObject& root) {
  root["now_ms"] = millis();
  root["free_heap"] = ESP.getFreeHeap();
  root["min_heap_seen"] = self->_minHeapSeen;
  root["max_alloc_heap"] = ESP.getMaxAllocHeap();
  root["posts_total"] = self->_postsTotal;
  root["posts_dropped"] = self->_postsDropped;
  root["heap_threshold"] = REMOTE_WEIGHT_MIN_FREE_HEAP;
  root["ws_clients"] = self->_webSocket.getWebSocket().count();
  root["last_seen_timestamp"] = self->_lastSeenTimestamp;

  JsonArray entries = root.createNestedArray("entries");
  uint8_t cap = REMOTE_WEIGHT_AUDIT_CAPACITY;
  uint8_t start = (cap + self->_auditHead - self->_auditCount) % cap;
  for (uint8_t i = 0; i < self->_auditCount; i++) {
    const RemoteWeightAuditEntry& e = self->_audit[(start + i) % cap];
    JsonObject obj = entries.createNestedObject();
    obj["t"] = e.millis_at;
    obj["heap"] = e.free_heap;
    obj["min"] = e.min_heap;
    obj["alloc"] = e.max_alloc;
    obj["posts"] = e.posts_total;
    obj["dropped"] = e.posts_dropped;
    obj["ws"] = e.ws_clients;
    obj["reason"] = e.reason;
  }
}
