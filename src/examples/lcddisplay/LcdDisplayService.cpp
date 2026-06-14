#include <Features.h>
#if FT_ENABLED(FT_DISPLAY_LCD)

#include <examples/lcddisplay/LcdDisplayService.h>

LcdDisplayService::LcdDisplayService(AsyncWebServer* server,
                                     SecurityManager* securityManager,
                                     AsyncMqttClient* mqttClient
#if FT_ENABLED(FT_BLE)
                                     ,BLEServer* bleServer
#endif
                                     ) :
    _httpEndpoint(LcdDisplayState::read,
                  LcdDisplayState::update,
                  this,
                  server,
                  LCD_DISPLAY_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED),
    _mqttPubSub(LcdDisplayState::read, LcdDisplayState::update, this, mqttClient),
    _webSocket(LcdDisplayState::read,
               LcdDisplayState::update,
               this,
               server,
               LCD_DISPLAY_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_AUTHENTICATED),
    _mqttClient(mqttClient),
    _lcd(nullptr),
    _wsClient(nullptr)
#if FT_ENABLED(FT_BLE)
    ,_blePubSub(LcdDisplayState::read, LcdDisplayState::update, this, bleServer),
    _bleServer(bleServer),
    _bleService(nullptr),
    _bleCharacteristic(nullptr),
    _bleClient(nullptr),
    _bleRemoteChar(nullptr),
    _bleScanning(false)
#endif
{
  #if FT_ENABLED(FT_BLE)
  _instance = this;
  #endif
  
  // Inline MQTT configuration using SettingValue placeholders
  // Single-layer pattern - no separate settings service needed
  _mqttBasePath = SettingValue::format("weighsoft/display/#{unique_id}");
  _mqttName = SettingValue::format("display-#{unique_id}");
  _mqttUniqueId = SettingValue::format("display-#{unique_id}");
  
  // configure MQTT callback
  _mqttClient->onConnect(std::bind(&LcdDisplayService::configureMqtt, this));
  
  // Initialize WebSocket client for Serial bridge
  _wsClient = new WebSocketsClient();
  _wsClient->onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
    if (type == WStype_TEXT) {
      DynamicJsonDocument doc(256);
      deserializeJson(doc, payload, length);
      const char* lastLine = doc["last_line"] | "";
      onSerialDataReceived(String(lastLine), "websocket");
    } else if (type == WStype_CONNECTED) {
      Serial.println("[Display] WebSocket bridge connected");
    } else if (type == WStype_DISCONNECTED) {
      Serial.println("[Display] WebSocket bridge disconnected");
    }
  });
  
  // Set up MQTT subscription callback for Serial bridge
  _mqttClient->onMessage([this](char* topic, char* payload,
      AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    LcdDisplayState state = _state;
    if (state.bridgeMode == "mqtt" && String(topic) == state.serialMqttTopic) {
      DynamicJsonDocument doc(256);
      deserializeJson(doc, payload, len);
      const char* lastLine = doc["last_line"] | "";
      onSerialDataReceived(String(lastLine), "mqtt");
    }
  });

  // Note: BLE will be configured via callback when BLE server is ready
  // This prevents initialization order issues

  // configure update handler to update LCD state for ALL channels
  // Origin tracking prevents feedback loops automatically
  addUpdateHandler([&](const String& originId) { onConfigUpdated(); }, false);
}

void LcdDisplayService::begin() {
  // Initialize state with defaults
  _state.line1 = "Weighsoft";
  _state.line2 = "Display Ready";
  _state.i2cAddress = 0x27;
  _state.backlight = true;
  
  // Initialize Serial bridge defaults
  _state.bridgeMode = "off";
  _state.serialDeviceIP = "";
  _state.serialDevicePort = 80;
  _state.serialMqttTopic = "";
  _state.serialBleServiceUuid = "";
  _state.serialBleCharUuid = "";
  
  // Initialize LCD with current I2C address
  _lcd = new LiquidCrystal_I2C(_state.i2cAddress, 16, 2);
  _lcd->init();
  _lcd->backlight();
  
  // Display welcome message
  onConfigUpdated();
}

void LcdDisplayService::loop() {
  if (_wsClient) {
    _wsClient->loop();
  }
}

void LcdDisplayService::onConfigUpdated() {
  if (_lcd == nullptr) {
    return;
  }
  
  LcdDisplayState state = _state;
  
  // Handle I2C address change (requires reinit)
  static uint8_t lastAddress = 0x27;
  if (state.i2cAddress != lastAddress) {
    delete _lcd;
    _lcd = new LiquidCrystal_I2C(state.i2cAddress, 16, 2);
    _lcd->init();
    lastAddress = state.i2cAddress;
  }
  
  // Update display content
  _lcd->clear();
  _lcd->setCursor(0, 0);
  _lcd->print(state.line1);
  _lcd->setCursor(0, 1);
  _lcd->print(state.line2);
  
  // Control backlight
  if (state.backlight) {
    _lcd->backlight();
  } else {
    _lcd->noBacklight();
  }
  
  // Handle Serial bridge mode changes
  handleBridgeModeChange();
}

void LcdDisplayService::configureMqtt() {
  if (!_mqttClient->connected()) {
    return;
  }
  
  // Build topics from inline configuration
  String subTopic = _mqttBasePath + "/set";
  String pubTopic = _mqttBasePath + "/data";

  // Configure MqttPubSub topics
  _mqttPubSub.configureTopics(pubTopic, subTopic);
}

#if FT_ENABLED(FT_BLE)
void LcdDisplayService::configureBle() {
  if (_bleServer == nullptr) {
    Serial.println("[Display] BLE server not available, skipping BLE configuration");
    return;
  }
  
  Serial.println("[Display] Configuring BLE service...");
  
  // Create BLE service with inline UUID
  _bleService = _bleServer->createService(BLE_SERVICE_UUID);
  
  // Create BLE characteristic with inline UUID
  _bleCharacteristic = _bleService->createCharacteristic(
      BLE_CHAR_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY
  );
  
  // Configure BlePubSub to use this characteristic
  _blePubSub.configureCharacteristic(_bleCharacteristic);
  
  // Start the service
  _bleService->start();
  
  Serial.printf("[Display] BLE service configured - Service UUID: %s, Char UUID: %s\n", 
                BLE_SERVICE_UUID, BLE_CHAR_UUID);
}
#endif

// Serial bridge implementation

void LcdDisplayService::onSerialDataReceived(const String& lastLine, const char* source) {
  Serial.printf("[Display] Received from %s: %s\n", source, lastLine.c_str());
  
  // Split line across two rows if needed (16 chars per row)
  _state.line1 = lastLine.substring(0, 16);
  _state.line2 = lastLine.length() > 16 ? lastLine.substring(16, 32) : "";
  
  // Pad with spaces to clear previous content
  while (_state.line1.length() < 16) _state.line1 += " ";
  while (_state.line2.length() < 16) _state.line2 += " ";
  
  // Update LCD
  if (_lcd != nullptr) {
    _lcd->clear();
    _lcd->setCursor(0, 0);
    _lcd->print(_state.line1);
    _lcd->setCursor(0, 1);
    _lcd->print(_state.line2);
  }
  
  // Broadcast state update to all channels
  update([](LcdDisplayState& state) { return StateUpdateResult::CHANGED; }, "serial_bridge");
}

void LcdDisplayService::handleBridgeModeChange() {
  LcdDisplayState state = _state;
  
  // Disconnect all bridges first
  disconnectWebSocketBridge();
  disconnectMqttBridge();
  #if FT_ENABLED(FT_BLE)
  disconnectBleBridge();
  #endif
  
  // Connect based on selected mode
  if (state.bridgeMode == "websocket") {
    connectWebSocketBridge();
  } else if (state.bridgeMode == "mqtt") {
    connectMqttBridge();
  }
  #if FT_ENABLED(FT_BLE)
  else if (state.bridgeMode == "ble") {
    connectBleBridge();
  }
  #endif
}

void LcdDisplayService::connectWebSocketBridge() {
  LcdDisplayState state = _state;
  
  if (state.serialDeviceIP.isEmpty()) {
    Serial.println("[Display] Cannot connect WS bridge: no IP configured");
    return;
  }
  
  String wsPath = "/ws/serial";
  _wsClient->begin(
    state.serialDeviceIP.c_str(),
    state.serialDevicePort,
    wsPath.c_str()
  );
  
  Serial.printf("[Display] Connecting WebSocket bridge to %s:%d%s\n",
    state.serialDeviceIP.c_str(), state.serialDevicePort, wsPath.c_str());
}

void LcdDisplayService::disconnectWebSocketBridge() {
  if (_wsClient && _wsClient->isConnected()) {
    _wsClient->disconnect();
    Serial.println("[Display] WebSocket bridge disconnected");
  }
}

void LcdDisplayService::connectMqttBridge() {
  LcdDisplayState state = _state;
  
  if (state.serialMqttTopic.isEmpty()) {
    Serial.println("[Display] Cannot connect MQTT bridge: no topic configured");
    return;
  }
  
  if (_mqttClient->connected()) {
    _mqttClient->subscribe(state.serialMqttTopic.c_str(), 0);
    _currentMqttSub = state.serialMqttTopic;
    Serial.printf("[Display] Subscribed to MQTT topic: %s\n", state.serialMqttTopic.c_str());
  } else {
    Serial.println("[Display] MQTT client not connected, will subscribe on connect");
  }
}

void LcdDisplayService::disconnectMqttBridge() {
  if (!_currentMqttSub.isEmpty() && _mqttClient->connected()) {
    _mqttClient->unsubscribe(_currentMqttSub.c_str());
    Serial.printf("[Display] Unsubscribed from MQTT topic: %s\n", _currentMqttSub.c_str());
    _currentMqttSub = "";
  }
}

#if FT_ENABLED(FT_BLE)
LcdDisplayService* LcdDisplayService::_instance = nullptr;

void LcdDisplayService::connectBleBridge() {
  LcdDisplayState state = _state;
  
  if (state.serialBleServiceUuid.isEmpty() || state.serialBleCharUuid.isEmpty()) {
    Serial.println("[Display] Cannot connect BLE bridge: UUIDs not configured");
    return;
  }
  
  // Initialize BLE if not already done
  if (_bleClient == nullptr) {
    _bleClient = BLEDevice::createClient();
    Serial.println("[Display] BLE client created for bridge");
  }
  
  // Start scanning for Serial device
  Serial.printf("[Display] Scanning for BLE device with service UUID: %s\n", 
    state.serialBleServiceUuid.c_str());
  _bleScanning = true;
  
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  
  BLEScanResults foundDevices = pBLEScan->start(5, false);
  _bleScanning = false;
  
  // Look for device advertising our service UUID
  for (int i = 0; i < foundDevices.getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);
    
    if (device.haveServiceUUID() && 
        device.isAdvertisingService(BLEUUID(state.serialBleServiceUuid.c_str()))) {
      
      Serial.printf("[Display] Found Serial device: %s\n", device.getAddress().toString().c_str());
      
      // Connect to device
      if (_bleClient->connect(&device)) {
        Serial.println("[Display] BLE connected");
        
        // Get service
        BLERemoteService* pRemoteService = _bleClient->getService(state.serialBleServiceUuid.c_str());
        if (pRemoteService == nullptr) {
          Serial.println("[Display] Failed to find service");
          _bleClient->disconnect();
          return;
        }
        
        // Get characteristic
        _bleRemoteChar = pRemoteService->getCharacteristic(state.serialBleCharUuid.c_str());
        if (_bleRemoteChar == nullptr) {
          Serial.println("[Display] Failed to find characteristic");
          _bleClient->disconnect();
          return;
        }
        
        // Register for notifications
        if (_bleRemoteChar->canNotify()) {
          _bleRemoteChar->registerForNotify(bleNotifyCallback);
          Serial.println("[Display] Subscribed to BLE notifications");
        }
        
        return;
      }
    }
  }
  
  Serial.println("[Display] Serial device not found via BLE scan");
}

void LcdDisplayService::disconnectBleBridge() {
  if (_bleClient && _bleClient->isConnected()) {
    _bleClient->disconnect();
    Serial.println("[Display] BLE bridge disconnected");
  }
  _bleRemoteChar = nullptr;
}

void LcdDisplayService::bleNotifyCallback(BLERemoteCharacteristic* pChar, 
                                        uint8_t* pData, size_t length, bool isNotify) {
  DynamicJsonDocument doc(256);
  deserializeJson(doc, pData, length);
  const char* lastLine = doc["last_line"] | "";
  
  if (_instance) {
    _instance->onSerialDataReceived(String(lastLine), "ble");
  }
}
#endif

#endif  // FT_DISPLAY_LCD
