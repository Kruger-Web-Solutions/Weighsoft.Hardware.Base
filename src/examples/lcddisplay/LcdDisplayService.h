#ifndef LcdDisplayService_h
#define LcdDisplayService_h

#include <Features.h>
#if FT_ENABLED(FT_DISPLAY_LCD)

#include <HttpEndpoint.h>
#include <MqttPubSub.h>
#include <WebSocketTxRx.h>
#include <SettingValue.h>
#include <LiquidCrystal_I2C.h>
#include <WebSocketsClient.h>

#if FT_ENABLED(FT_BLE)
#include <BlePubSub.h>
#include <BLEServer.h>
#include <BLEService.h>
#include <BLECharacteristic.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#endif

#define LCD_DISPLAY_ENDPOINT_PATH "/rest/display"
#define LCD_DISPLAY_SOCKET_PATH "/ws/display"

class LcdDisplayState {
 public:
  String line1;
  String line2;
  uint8_t i2cAddress;
  bool backlight;
  
  // Serial bridge configuration
  String bridgeMode;
  String serialDeviceIP;
  uint16_t serialDevicePort;
  String serialMqttTopic;
  String serialBleServiceUuid;
  String serialBleCharUuid;

  static void read(LcdDisplayState& state, JsonObject& root) {
    root["line1"] = state.line1;
    root["line2"] = state.line2;
    root["i2c_address"] = state.i2cAddress;
    root["backlight"] = state.backlight;
    
    // Serial bridge fields
    root["bridge_mode"] = state.bridgeMode;
    root["serial_device_ip"] = state.serialDeviceIP;
    root["serial_device_port"] = state.serialDevicePort;
    root["serial_mqtt_topic"] = state.serialMqttTopic;
    root["serial_ble_service_uuid"] = state.serialBleServiceUuid;
    root["serial_ble_char_uuid"] = state.serialBleCharUuid;
  }

  static StateUpdateResult update(JsonObject& root, LcdDisplayState& state) {
    bool changed = false;
    
    String newLine1 = root["line1"] | "";
    if (newLine1.length() > 16) newLine1 = newLine1.substring(0, 16);
    if (state.line1 != newLine1) {
      state.line1 = newLine1;
      changed = true;
    }
    
    String newLine2 = root["line2"] | "";
    if (newLine2.length() > 16) newLine2 = newLine2.substring(0, 16);
    if (state.line2 != newLine2) {
      state.line2 = newLine2;
      changed = true;
    }
    
    uint8_t newAddress = root["i2c_address"] | 0x27;
    if (state.i2cAddress != newAddress) {
      state.i2cAddress = newAddress;
      changed = true;
    }
    
    bool newBacklight = root["backlight"] | true;
    if (state.backlight != newBacklight) {
      state.backlight = newBacklight;
      changed = true;
    }
    
    // Serial bridge fields
    String newMode = root["bridge_mode"] | "off";
    if (state.bridgeMode != newMode) {
      state.bridgeMode = newMode;
      changed = true;
    }
    
    String newIP = root["serial_device_ip"] | "";
    if (state.serialDeviceIP != newIP) {
      state.serialDeviceIP = newIP;
      changed = true;
    }
    
    uint16_t newPort = root["serial_device_port"] | 80;
    if (state.serialDevicePort != newPort) {
      state.serialDevicePort = newPort;
      changed = true;
    }
    
    String newTopic = root["serial_mqtt_topic"] | "";
    if (state.serialMqttTopic != newTopic) {
      state.serialMqttTopic = newTopic;
      changed = true;
    }
    
    String newBleService = root["serial_ble_service_uuid"] | "";
    if (state.serialBleServiceUuid != newBleService) {
      state.serialBleServiceUuid = newBleService;
      changed = true;
    }
    
    String newBleChar = root["serial_ble_char_uuid"] | "";
    if (state.serialBleCharUuid != newBleChar) {
      state.serialBleCharUuid = newBleChar;
      changed = true;
    }
    
    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }
};

class LcdDisplayService : public StatefulService<LcdDisplayState> {
 public:
  LcdDisplayService(AsyncWebServer* server,
                    SecurityManager* securityManager,
                    AsyncMqttClient* mqttClient
#if FT_ENABLED(FT_BLE)
                    ,BLEServer* bleServer
#endif
                    );
  void begin();
  void loop();

#if FT_ENABLED(FT_BLE)
  void setBleServer(BLEServer* bleServer) { _bleServer = bleServer; }
  void configureBle();
#endif

 private:
  HttpEndpoint<LcdDisplayState> _httpEndpoint;
  MqttPubSub<LcdDisplayState> _mqttPubSub;
  WebSocketTxRx<LcdDisplayState> _webSocket;
  AsyncMqttClient* _mqttClient;

  // Hardware
  LiquidCrystal_I2C* _lcd;
  
  // Serial bridge - WebSocket client
  WebSocketsClient* _wsClient;
  String _currentMqttSub;

  // Inline MQTT configuration - single-layer pattern
  String _mqttBasePath;
  String _mqttName;
  String _mqttUniqueId;

#if FT_ENABLED(FT_BLE)
  BlePubSub<LcdDisplayState> _blePubSub;
  BLEServer* _bleServer;
  BLEService* _bleService;
  BLECharacteristic* _bleCharacteristic;
  
  // Serial bridge - BLE client
  BLEClient* _bleClient;
  BLERemoteCharacteristic* _bleRemoteChar;
  bool _bleScanning;
  static LcdDisplayService* _instance;
  
  // Inline BLE configuration - single-layer pattern
  static constexpr const char* BLE_SERVICE_UUID = "a8f3d5e0-8b2c-4f1a-9d6e-3c7b4a5f1e8d";
  static constexpr const char* BLE_CHAR_UUID = "a8f3d5e1-8b2c-4f1a-9d6e-3c7b4a5f1e8d";
#endif

  void configureMqtt();
  void onConfigUpdated();
  
  // Serial bridge handlers
  void onSerialDataReceived(const String& lastLine, const char* source);
  void connectWebSocketBridge();
  void disconnectWebSocketBridge();
  void connectMqttBridge();
  void disconnectMqttBridge();
  void handleBridgeModeChange();
  
#if FT_ENABLED(FT_BLE)
  void connectBleBridge();
  void disconnectBleBridge();
  static void bleNotifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify);
#endif
};

#endif  // FT_DISPLAY_LCD
#endif
