#ifndef RelayBoardService_h
#define RelayBoardService_h

#include <Features.h>
#if FT_ENABLED(FT_WSOFT_RELAY)

// Relay outputs on a W-Soft board, driven by the Hi-Lo check (LOW / OK / HIGH).
//
// Ported from the relay board firmware (branch RelayBoardEspBuildIn @ b1d0f27,
// src/examples/relay): same state (relay1..relay4, di1, di2), same paths and the same
// setWeightBandRelays() idea, so the page and Edge's "weighsoft" relay format read it
// the same way. What differs on a W-Soft board:
// - Pins and the active level come from the board env (platformio.ini). A relay whose
//   pin is -1 is not fitted: its state stays false and nothing is written.
// - The zone comes from W-Soft's own Hi-Lo check (lib/wsoft_esp relay.h), not from a
//   band typed on this board.
// - Reading and switching need a login (the relay board firmware leaves both public).
//   A relay here can be a gate or a light on a weigh line.
// - No MQTT switch and no digital inputs (this board has no input pins for them).

#include <HttpEndpoint.h>
#include <WebSocketTxRx.h>

#include <examples/wsoft/WSoftSettingsService.h>
#include <wsoft_relay.h>

#ifndef RELAY1_PIN
#define RELAY1_PIN -1
#endif
#ifndef RELAY2_PIN
#define RELAY2_PIN -1
#endif
#ifndef RELAY3_PIN
#define RELAY3_PIN -1
#endif
#ifndef RELAY4_PIN
#define RELAY4_PIN -1
#endif

// 1 = the relay input switches ON when the pin is LOW (the usual opto relay module, and
// the CYD's RGB LED pads, which rest HIGH = OFF at boot). 0 = ON when HIGH.
#ifndef RELAY_ACTIVE_LOW
#define RELAY_ACTIVE_LOW 0
#endif
#if RELAY_ACTIVE_LOW
#define RELAY_ON LOW
#define RELAY_OFF HIGH
#else
#define RELAY_ON HIGH
#define RELAY_OFF LOW
#endif

#define RELAY_BOARD_ENDPOINT_PATH "/rest/relayBoard"
#define RELAY_BOARD_SOCKET_PATH "/ws/relayBoard"
#define RELAY_BOARD_STATUS_PATH "/rest/relayBoardStatus"

class RelayBoardState {
 public:
  bool relay[WSOFT_RELAY_COUNT] = {false, false, false, false};

  static int pinOf(uint8_t index) {
    static const int pins[WSOFT_RELAY_COUNT] = {RELAY1_PIN, RELAY2_PIN, RELAY3_PIN, RELAY4_PIN};
    return index < WSOFT_RELAY_COUNT ? pins[index] : -1;
  }
  static bool fitted(uint8_t index) { return pinOf(index) >= 0; }

  static void read(RelayBoardState& state, JsonObject& root) {
    root["relay1"] = state.relay[0];
    root["relay2"] = state.relay[1];
    root["relay3"] = state.relay[2];
    root["relay4"] = state.relay[3];
    root["di1"] = false;
    root["di2"] = false;
  }

  // Same rule as the relay board firmware: only the relays named in the body change.
  // A relay that is not fitted stays false.
  static StateUpdateResult update(JsonObject& root, RelayBoardState& state) {
    static const char* const keys[WSOFT_RELAY_COUNT] = {"relay1", "relay2", "relay3", "relay4"};
    bool changed = false;
    for (uint8_t i = 0; i < WSOFT_RELAY_COUNT; i++) {
      if (!root.containsKey(keys[i]) || !root[keys[i]].is<bool>()) continue;
      bool v = fitted(i) && root[keys[i]].as<bool>();
      if (state.relay[i] != v) {
        state.relay[i] = v;
        changed = true;
      }
    }
    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }
};

class RelayBoardService : public StatefulService<RelayBoardState> {
 public:
  RelayBoardService(AsyncWebServer* server, SecurityManager* securityManager, WSoftSettingsService* settings);
  void begin();
  // Arduino loop: follows the Hi-Lo zone while the relay output is switched on.
  void loop();

  // Relays relayLow / relayOk / relayHigh (1..4, 0 = none) belong to Hi-Lo: all of them
  // OFF, then the one for the zone ON (zone 0 = none). Other relays are left alone.
  void setWeightBandRelays(uint8_t relayLow, uint8_t relayOk, uint8_t relayHigh, uint8_t zone);

 private:
  HttpEndpoint<RelayBoardState> _httpEndpoint;
  WebSocketTxRx<RelayBoardState> _webSocket;
  AsyncWebServer* _server;
  SecurityManager* _securityManager;
  WSoftSettingsService* _settings;
  bool _driving = false;      // the last loop pass drove the relays from the zone
  uint8_t _drivenOwned = 0;   // the relays Hi-Lo owned on that pass
  WsoftRelayZone _loggedZone = WSOFT_RELAY_NONE;

  void applyOutputs();
  void registerStatusEndpoint();
};

#endif  // FT_WSOFT_RELAY
#endif
