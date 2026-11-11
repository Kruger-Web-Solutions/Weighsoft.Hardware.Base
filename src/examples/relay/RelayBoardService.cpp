#include <Features.h>
#if FT_ENABLED(FT_WSOFT_RELAY)

#include <examples/relay/RelayBoardService.h>
#include <AsyncJson.h>
#include <WiFi.h>

#include <relay.h>

RelayBoardService::RelayBoardService(AsyncWebServer* server,
                                     SecurityManager* securityManager,
                                     WSoftSettingsService* settings) :
    _httpEndpoint(RelayBoardState::read,
                  RelayBoardState::update,
                  this,
                  server,
                  RELAY_BOARD_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED),
    _webSocket(RelayBoardState::read,
               RelayBoardState::update,
               this,
               server,
               RELAY_BOARD_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_AUTHENTICATED),
    _server(server),
    _securityManager(securityManager),
    _settings(settings) {
  // OFF level first, then output: the pin never shows ON while it is being set up.
  for (uint8_t i = 0; i < WSOFT_RELAY_COUNT; i++) {
    if (!RelayBoardState::fitted(i)) continue;
    digitalWrite(RelayBoardState::pinOf(i), RELAY_OFF);
    pinMode(RelayBoardState::pinOf(i), OUTPUT);
    digitalWrite(RelayBoardState::pinOf(i), RELAY_OFF);
  }
  addUpdateHandler([&](const String& originId) { applyOutputs(); }, false);
  registerStatusEndpoint();
}

void RelayBoardService::begin() {
  update(
      [&](RelayBoardState& state) {
        for (uint8_t i = 0; i < WSOFT_RELAY_COUNT; i++) state.relay[i] = false;
        return StateUpdateResult::CHANGED;
      },
      "boot");
  applyOutputs();
  Serial.printf("[Relay] pins %d %d %d %d, ON = %s, all OFF\n", RELAY1_PIN, RELAY2_PIN, RELAY3_PIN, RELAY4_PIN,
                RELAY_ACTIVE_LOW ? "LOW" : "HIGH");
}

void RelayBoardService::loop() {
  bool enabled = false;
  uint8_t low = 0, ok = 0, high = 0;
  _settings->relayConfig(&enabled, &low, &ok, &high);
  uint8_t owned = wsoft_relay_owned(low, ok, high);
  if (!enabled) {
    // Switched off: the Hi-Lo relays go OFF once, then the page may switch relays by hand
    // (the bench click test).
    if (_driving) {
      setWeightBandRelays(low, ok, high, WSOFT_RELAY_NONE);
      _driving = false;
      _loggedZone = WSOFT_RELAY_NONE;
      Serial.println(F("[Relay] Hi-Lo output off: relays OFF"));
    }
    return;
  }
  // A changed map: the relays the old map owned go OFF before the new map drives.
  if (_driving && owned != _drivenOwned) {
    uint8_t dropped = _drivenOwned & (uint8_t)~owned;
    update(
        [&](RelayBoardState& state) {
          bool changed = false;
          for (uint8_t i = 0; i < WSOFT_RELAY_COUNT; i++) {
            if ((dropped & (1u << i)) && state.relay[i]) {
              state.relay[i] = false;
              changed = true;
            }
          }
          return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
        },
        "hilo");
  }
  WsoftRelayZone zone = relay_zone();
  setWeightBandRelays(low, ok, high, zone);
  _driving = true;
  _drivenOwned = owned;
  if (zone != _loggedZone) {
    _loggedZone = zone;
    Serial.printf("[Relay] Hi-Lo zone %s\n", wsoft_relay_zone_text(zone));
  }
}

void RelayBoardService::setWeightBandRelays(uint8_t relayLow, uint8_t relayOk, uint8_t relayHigh, uint8_t zone) {
  WsoftRelayZone z = zone <= WSOFT_RELAY_HIGH ? (WsoftRelayZone)zone : WSOFT_RELAY_NONE;
  uint8_t owned = wsoft_relay_owned(relayLow, relayOk, relayHigh);
  uint8_t on = wsoft_relay_mask(z, relayLow, relayOk, relayHigh);
  update(
      [&](RelayBoardState& state) {
        bool changed = false;
        for (uint8_t i = 0; i < WSOFT_RELAY_COUNT; i++) {
          if (!(owned & (1u << i))) continue;
          bool next = RelayBoardState::fitted(i) && (on & (1u << i)) != 0;
          if (state.relay[i] != next) {
            state.relay[i] = next;
            changed = true;
          }
        }
        return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
      },
      "hilo");
}

// OFF writes first, then ON: two Hi-Lo relays are never ON together, not even for a moment.
void RelayBoardService::applyOutputs() {
  for (uint8_t i = 0; i < WSOFT_RELAY_COUNT; i++) {
    if (RelayBoardState::fitted(i) && !_state.relay[i]) digitalWrite(RelayBoardState::pinOf(i), RELAY_OFF);
  }
  for (uint8_t i = 0; i < WSOFT_RELAY_COUNT; i++) {
    if (RelayBoardState::fitted(i) && _state.relay[i]) digitalWrite(RelayBoardState::pinOf(i), RELAY_ON);
  }
}

void RelayBoardService::registerStatusEndpoint() {
  _server->on(
      RELAY_BOARD_STATUS_PATH,
      HTTP_GET,
      _securityManager->wrapRequest(
          [this](AsyncWebServerRequest* request) {
            AsyncJsonResponse* response = new AsyncJsonResponse(false, 1024);
            JsonObject root = response->getRoot();

            root["board"] = "W-Soft board with Hi-Lo relays";
            root["platform"] = "esp32";
            root["free_heap"] = ESP.getFreeHeap();
            root["uptime_ms"] = millis();
            root["ip"] = WiFi.localIP().toString();
            root["relay_active"] = RELAY_ACTIVE_LOW ? "low" : "high";

            JsonObject pins = root.createNestedObject("pins");
            pins["ry1"] = RELAY1_PIN;
            pins["ry2"] = RELAY2_PIN;
            pins["ry3"] = RELAY3_PIN;
            pins["ry4"] = RELAY4_PIN;

            bool enabled = false;
            uint8_t low = 0, ok = 0, high = 0;
            _settings->relayConfig(&enabled, &low, &ok, &high);
            JsonObject hilo = root.createNestedObject("hilo");
            hilo["enabled"] = enabled;
            hilo["relay_low"] = low;
            hilo["relay_ok"] = ok;
            hilo["relay_high"] = high;
            hilo["zone"] = wsoft_relay_zone_text(enabled ? relay_zone() : WSOFT_RELAY_NONE);

            // Same object the relay board firmware sends (Edge reads only this).
            JsonObject relays = root.createNestedObject("relays");
            read([&](const RelayBoardState& state) {
              relays["relay1"] = state.relay[0];
              relays["relay2"] = state.relay[1];
              relays["relay3"] = state.relay[2];
              relays["relay4"] = state.relay[3];
            });
            relays["di1"] = false;
            relays["di2"] = false;

            response->setLength();
            request->send(response);
          },
          AuthenticationPredicates::IS_AUTHENTICATED));
}

#endif  // FT_WSOFT_RELAY
