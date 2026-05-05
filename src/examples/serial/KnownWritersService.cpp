#include <examples/serial/KnownWritersService.h>

using namespace std::placeholders;

KnownWritersService::KnownWritersService(AsyncWebServer* server,
                                         FS* fs,
                                         SecurityManager* securityManager) :
    _httpEndpoint(KnownWritersState::read,
                  KnownWritersState::update,
                  this,
                  server,
                  KNOWN_WRITERS_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::NONE_REQUIRED),
    _fsPersistence(KnownWritersState::readConfig,
                   KnownWritersState::updateConfig,
                   this,
                   fs,
                   KNOWN_WRITERS_CONFIG_FILE),
    _server(server),
    _securityManager(securityManager) {
  // Writer broadcast timestamps can change with every serial line. Persist only
  // explicit writer list changes so multiple writers do not hammer LittleFS.
  _fsPersistence.disableUpdateHandler();
}

void KnownWritersService::begin() {
  _fsPersistence.readFromFS();
  registerForgetEndpoint();
  registerAnnounceEndpoint();
}

void KnownWritersService::onWriterConnected(const String& id, const String& name, const String& ip) {
  if (id.length() == 0) return;
  update([&](KnownWritersState& s) {
    s.upsert(id, name, ip, millis());
    return StateUpdateResult::CHANGED;
  }, "writer-connect");
  persistAsync();
}

void KnownWritersService::onWriterDisconnected(const String& id) {
  if (id.length() == 0) return;
  update([&](KnownWritersState& s) {
    s.markOffline(id, millis());
    return StateUpdateResult::CHANGED;
  }, "writer-disconnect");
  persistAsync();
}

void KnownWritersService::recordBroadcastToOnlineWriters(const String& message) {
  bool changed = false;
  unsigned long now = millis();
  update([&](KnownWritersState& s) {
    for (auto& w : s.writers) {
      if (w.online) {
        w.lastMessage = message;
        w.lastMessageAt = now;
        w.lastSeenAt = now;
        changed = true;
      }
    }
    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }, "writer-broadcast");
}

size_t KnownWritersService::connectedCount() {
  size_t n = 0;
  read([&](const KnownWritersState& s) {
    for (const auto& w : s.writers) if (w.online) n++;
  });
  return n;
}

void KnownWritersService::persistAsync() {
  _fsPersistence.writeToFS();
}

void KnownWritersService::loop() {
  unsigned long now = millis();
  if (now - _lastStaleCheckMs < 5000) return;  // check every 5 seconds
  _lastStaleCheckMs = now;

  bool anyChanged = false;
  update([&](KnownWritersState& s) {
    for (auto& w : s.writers) {
      if (w.online && (now - w.lastSeenAt) > 30000UL) {
        w.online = false;
        anyChanged = true;
      }
    }
    return anyChanged ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }, "writer-stale-check");
  if (anyChanged) persistAsync();
}

void KnownWritersService::registerAnnounceEndpoint() {
  // POST /rest/writers/announce
  // Body: { "id": "hw-id", "name": "friendly-name" }
  // Registers/refreshes the writer in KnownWritersService.
  // Auth: intentionally open (private network v1) — mirrors the pattern in
  // SerialWriterService::registerSendEndpoint() which also uses no auth wrapper.
  _server->on(
      "/rest/writers/announce",
      HTTP_POST,
      [](AsyncWebServerRequest* req) {},
      nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, data, len)) {
          req->send(400, "application/json", "{\"error\":\"invalid json\"}");
          return;
        }
        if (!doc.is<JsonObject>()) {
          req->send(400, "application/json", "{\"error\":\"object expected\"}");
          return;
        }
        String id   = doc["id"]   | "";
        String name = doc["name"] | "";
        if (id.length() == 0) {
          req->send(400, "application/json", "{\"error\":\"id required\"}");
          return;
        }
        String ip = req->client()->remoteIP().toString();
        onWriterConnected(id, name, ip);
        req->send(200, "application/json", "{\"ok\":true}");
      });
}

void KnownWritersService::registerForgetEndpoint() {
  // DELETE /rest/writers?id=<writer-id>
  //
  // Routing note: ASYNCWEBSERVER_REGEX is NOT defined in this project's build flags,
  // so regex-based path parameters (pathArg) are unavailable. Instead, the writer id
  // is passed as a query parameter: DELETE /rest/writers?id=abc123. This matches how
  // the existing codebase uses _server->on() with a plain path string and wrapRequest
  // for authentication (see APStatus.cpp, FactoryResetService.cpp).
  _server->on(
      KNOWN_WRITERS_ENDPOINT_PATH,
      HTTP_DELETE,
      _securityManager->wrapRequest(
          [this](AsyncWebServerRequest* req) {
            if (!req->hasParam("id")) {
              req->send(400, "application/json", "{\"error\":\"id param required\"}");
              return;
            }
            String id = req->getParam("id")->value();
            bool removed = false;
            update([&](KnownWritersState& s) {
              removed = s.removeById(id);
              return removed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
            }, "writer-forget");
            if (removed) persistAsync();
            req->send(removed ? 200 : 404, "application/json",
                      removed ? "{\"removed\":true}" : "{\"error\":\"not found\"}");
          },
          AuthenticationPredicates::IS_AUTHENTICATED));
}
