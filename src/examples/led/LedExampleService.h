#ifndef LedExampleService_h
#define LedExampleService_h

#include <HttpEndpoint.h>
#include <MqttPubSub.h>
#include <WebSocketTxRx.h>
#include <SettingValue.h>

#if FT_ENABLED(FT_BLE)
#include <BlePubSub.h>
#include <BLEServer.h>
#include <BLEService.h>
#include <BLECharacteristic.h>
#endif

// ESP32-S3 has RGB NeoPixel LED on GPIO48 (ESP32-S3-DevKitC-1)
// ESP32 has built-in LED on GPIO2 (most boards)
// ESP8266 has built-in LED on GPIO2 (NodeMCU)
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define LED_PIN 48
#define HAS_RGB_LED true
#else
#define LED_PIN 2
#define HAS_RGB_LED false
#endif

#define DEFAULT_LED_STATE false
#define DEFAULT_LED_RED 255
#define DEFAULT_LED_GREEN 255
#define DEFAULT_LED_BLUE 255
#define OFF_STATE "OFF"
#define ON_STATE "ON"

// Note that the built-in LED is on when the pin is low on most NodeMCU boards.
// This is because the anode is tied to VCC and the cathode to the GPIO 4 (Arduino pin 2).
#ifdef ESP32
#define LED_ON 0x1
#define LED_OFF 0x0
#elif defined(ESP8266)
#define LED_ON 0x0
#define LED_OFF 0x1
#endif

#define LED_EXAMPLE_ENDPOINT_PATH "/rest/ledExample"
#define LED_EXAMPLE_SOCKET_PATH "/ws/ledExample"

class LedExampleState {
 public:
  bool ledOn;
  uint8_t red;    // RGB red value (0-255)
  uint8_t green;  // RGB green value (0-255)
  uint8_t blue;   // RGB blue value (0-255)

  static void read(LedExampleState& settings, JsonObject& root) {
    root["led_on"] = settings.ledOn;
    root["red"] = settings.red;
    root["green"] = settings.green;
    root["blue"] = settings.blue;
    root["has_rgb"] = HAS_RGB_LED;
  }

  static StateUpdateResult update(JsonObject& root, LedExampleState& ledState) {
    boolean newState = root["led_on"] | DEFAULT_LED_STATE;
    uint8_t newRed = root["red"] | ledState.red;
    uint8_t newGreen = root["green"] | ledState.green;
    uint8_t newBlue = root["blue"] | ledState.blue;
    
    bool changed = false;
    
    if (ledState.ledOn != newState) {
      ledState.ledOn = newState;
      changed = true;
    }
    
    if (ledState.red != newRed) {
      ledState.red = newRed;
      changed = true;
    }
    
    if (ledState.green != newGreen) {
      ledState.green = newGreen;
      changed = true;
    }
    
    if (ledState.blue != newBlue) {
      ledState.blue = newBlue;
      changed = true;
    }
    
    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }

  static void haRead(LedExampleState& settings, JsonObject& root) {
    root["state"] = settings.ledOn ? ON_STATE : OFF_STATE;
  }

  static StateUpdateResult haUpdate(JsonObject& root, LedExampleState& ledState) {
    String state = root["state"];
    // parse new led state 
    boolean newState = false;
    if (state.equals(ON_STATE)) {
      newState = true;
    } else if (!state.equals(OFF_STATE)) {
      return StateUpdateResult::ERROR;
    }
    // change the new state, if required
    if (ledState.ledOn != newState) {
      ledState.ledOn = newState;
      return StateUpdateResult::CHANGED;
    }
    return StateUpdateResult::UNCHANGED;
  }
};

class LedExampleService : public StatefulService<LedExampleState> {
 public:
  LedExampleService(AsyncWebServer* server,
                    SecurityManager* securityManager,
                    AsyncMqttClient* mqttClient
#if FT_ENABLED(FT_BLE)
                    ,BLEServer* bleServer
#endif
                    );
  void begin();

#if FT_ENABLED(FT_BLE)
  void setBleServer(BLEServer* bleServer) { _bleServer = bleServer; }
  void configureBle();
#endif

 private:
  HttpEndpoint<LedExampleState> _httpEndpoint;
  MqttPubSub<LedExampleState> _mqttPubSub;
  WebSocketTxRx<LedExampleState> _webSocket;
  AsyncMqttClient* _mqttClient;

  // Inline MQTT configuration - single-layer pattern
  String _mqttBasePath;
  String _mqttName;
  String _mqttUniqueId;

#if FT_ENABLED(FT_BLE)
  BlePubSub<LedExampleState> _blePubSub;
  BLEServer* _bleServer;
  BLEService* _bleService;
  BLECharacteristic* _bleCharacteristic;
  
  // Inline BLE configuration - single-layer pattern
  static constexpr const char* BLE_SERVICE_UUID = "19b10000-e8f2-537e-4f6c-d104768a1214";
  static constexpr const char* BLE_CHAR_UUID = "19b10001-e8f2-537e-4f6c-d104768a1214";
#endif

  void configureMqtt();
  void onConfigUpdated();
};

#endif
