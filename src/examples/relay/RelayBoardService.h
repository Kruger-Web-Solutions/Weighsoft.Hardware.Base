#ifndef RelayBoardService_h
#define RelayBoardService_h

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

// Most opto-isolated boards: LOW energizes relay
#define RELAY_ON LOW
#define RELAY_OFF HIGH

#ifndef RELAY_BOARD_HAS_BUZZER
#define RELAY_BOARD_HAS_BUZZER 0
#endif
#ifndef BUZZER_PIN
#define BUZZER_PIN 4
#endif

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
  bool buzzer;

  static void read(RelayBoardState& state, JsonObject& root) {
    root["relay1"] = state.relay1;
    root["relay2"] = state.relay2;
    root["relay3"] = state.relay3;
    root["relay4"] = state.relay4;
    root["buzzer"] = state.buzzer;
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
    if (root.containsKey("buzzer")) {
      bool v = root["buzzer"];
      if (state.buzzer != v) {
        state.buzzer = v;
        changed = true;
      }
    }

    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }
};

class RelayBoardService : public StatefulService<RelayBoardState> {
 public:
  RelayBoardService(AsyncWebServer* server, SecurityManager* securityManager, AsyncMqttClient* mqttClient);
  void begin();

 private:
  HttpEndpoint<RelayBoardState> _httpEndpoint;
  MqttPubSub<RelayBoardState> _mqttPubSub;
  WebSocketTxRx<RelayBoardState> _webSocket;
  AsyncMqttClient* _mqttClient;
  AsyncWebServer* _server;
  SecurityManager* _securityManager;

  String _mqttBasePath;
  String _mqttName;
  String _mqttUniqueId;

  void configureMqtt();
  void onConfigUpdated();
  void applyOutputs();
  void registerStatusEndpoint();
};

#endif
