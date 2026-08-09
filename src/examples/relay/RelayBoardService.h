#ifndef RelayBoardService_h
#define RelayBoardService_h

#include <functional>
#include <HttpEndpoint.h>
#include <MqttPubSub.h>
#include <WebSocketTxRx.h>
#include <SettingValue.h>

// LC-style ESP-12F 4-ch relay board (verify with click test)
#ifndef RELAY1_PIN
#define RELAY1_PIN 16
#endif
#ifndef RELAY2_PIN
#define RELAY2_PIN 14
#endif
#ifndef RELAY3_PIN
#define RELAY3_PIN 12
#endif
#ifndef RELAY4_PIN
#define RELAY4_PIN 13
#endif

// ESP12F_Relay_X4: transistor drive, HIGH energizes relay
// (confirmed by Tasmota/ESPHome device configs for LC-Relay-ESP12-4R-MV)
#define RELAY_ON HIGH
#define RELAY_OFF LOW

// Digital inputs on the IO4 / IO5 breakout pins (INPUT_PULLUP, close to GND = active)
#ifndef DI1_PIN
#define DI1_PIN 4
#endif
#ifndef DI2_PIN
#define DI2_PIN 5
#endif
#define DI_POLL_INTERVAL_MS 50

#define DEFAULT_RELAY_STATE false

#define RELAY_BOARD_ENDPOINT_PATH "/rest/relayBoard"
#define RELAY_BOARD_SOCKET_PATH "/ws/relayBoard"
#define RELAY_BOARD_STATUS_PATH "/rest/relayBoardStatus"

class RelayBoardState {
 public:
  bool relay1;
  bool relay2;
  bool relay3;
  bool relay4;
  bool di1;
  bool di2;

  static void read(RelayBoardState& state, JsonObject& root) {
    root["relay1"] = state.relay1;
    root["relay2"] = state.relay2;
    root["relay3"] = state.relay3;
    root["relay4"] = state.relay4;
    root["di1"] = state.di1;
    root["di2"] = state.di2;
  }

  static StateUpdateResult update(JsonObject& root, RelayBoardState& state) {
    bool changed = false;

    if (root.containsKey("relay1")) {
      bool v = root["relay1"];
      if (state.relay1 != v) {
        state.relay1 = v;
        changed = true;
      }
    }
    if (root.containsKey("relay2")) {
      bool v = root["relay2"];
      if (state.relay2 != v) {
        state.relay2 = v;
        changed = true;
      }
    }
    if (root.containsKey("relay3")) {
      bool v = root["relay3"];
      if (state.relay3 != v) {
        state.relay3 = v;
        changed = true;
      }
    }
    if (root.containsKey("relay4")) {
      bool v = root["relay4"];
      if (state.relay4 != v) {
        state.relay4 = v;
        changed = true;
      }
    }

    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }
};

class RelayBoardService : public StatefulService<RelayBoardState> {
 public:
  using DiEdgeCallback = std::function<void(uint8_t diIndex, bool active)>;

  RelayBoardService(AsyncWebServer* server, SecurityManager* securityManager, AsyncMqttClient* mqttClient);
  void begin();
  void loop();

  // Live Weight range control: turn on one of three mapped relays (1–4), others in the map off
  void setWeightBandRelays(uint8_t relayLow, uint8_t relayOk, uint8_t relayHigh, uint8_t zone);
  void setDiEdgeCallback(DiEdgeCallback cb);

 private:
  HttpEndpoint<RelayBoardState> _httpEndpoint;
  MqttPubSub<RelayBoardState> _mqttPubSub;
  WebSocketTxRx<RelayBoardState> _webSocket;
  AsyncMqttClient* _mqttClient;
  AsyncWebServer* _server;
  SecurityManager* _securityManager;
  DiEdgeCallback _diEdgeCallback;

  String _mqttBasePath;
  String _mqttName;
  String _mqttUniqueId;
  unsigned long _lastDiPoll = 0;

  void configureMqtt();
  void onConfigUpdated();
  void applyOutputs();
  void pollInputs();
  void registerStatusEndpoint();
};

#endif
