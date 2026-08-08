#include <examples/relay/RelayBoardService.h>
#include <AsyncJson.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif

RelayBoardService::RelayBoardService(AsyncWebServer* server,
                                     SecurityManager* securityManager,
                                     AsyncMqttClient* mqttClient) :
    _httpEndpoint(RelayBoardState::read,
                  RelayBoardState::update,
                  this,
                  server,
                  RELAY_BOARD_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED),
    _mqttPubSub(RelayBoardState::read, RelayBoardState::update, this, mqttClient),
    _webSocket(RelayBoardState::read,
               RelayBoardState::update,
               this,
               server,
               RELAY_BOARD_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_AUTHENTICATED),
    _mqttClient(mqttClient),
    _server(server),
    _securityManager(securityManager) {
  _mqttBasePath = SettingValue::format("homeassistant/switch/#{unique_id}");
  _mqttName = SettingValue::format("relay-board-#{unique_id}");
  _mqttUniqueId = SettingValue::format("relay-#{unique_id}");

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, RELAY_OFF);
  digitalWrite(RELAY2_PIN, RELAY_OFF);
  digitalWrite(RELAY3_PIN, RELAY_OFF);
  digitalWrite(RELAY4_PIN, RELAY_OFF);

#if RELAY_BOARD_HAS_BUZZER
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
#endif

  pinMode(DI1_PIN, INPUT_PULLUP);
  pinMode(DI2_PIN, INPUT_PULLUP);

  _mqttClient->onConnect(std::bind(&RelayBoardService::configureMqtt, this));
  addUpdateHandler([&](const String& originId) { onConfigUpdated(); }, false);
  registerStatusEndpoint();
}

void RelayBoardService::begin() {
  _state.relay1 = DEFAULT_RELAY_STATE;
  _state.relay2 = DEFAULT_RELAY_STATE;
  _state.relay3 = DEFAULT_RELAY_STATE;
  _state.relay4 = DEFAULT_RELAY_STATE;
  _state.buzzer = false;
  _state.di1 = digitalRead(DI1_PIN) == LOW;
  _state.di2 = digitalRead(DI2_PIN) == LOW;
  onConfigUpdated();
}

void RelayBoardService::loop() {
  unsigned long now = millis();
  if ((unsigned long)(now - _lastDiPoll) < DI_POLL_INTERVAL_MS) {
    return;
  }
  _lastDiPoll = now;
  pollInputs();
}

void RelayBoardService::pollInputs() {
  bool di1 = digitalRead(DI1_PIN) == LOW;
  bool di2 = digitalRead(DI2_PIN) == LOW;
  update(
      [&](RelayBoardState& state) {
        if (state.di1 == di1 && state.di2 == di2) {
          return StateUpdateResult::UNCHANGED;
        }
        state.di1 = di1;
        state.di2 = di2;
        return StateUpdateResult::CHANGED;
      },
      "gpio");
}

void RelayBoardService::onConfigUpdated() {
  applyOutputs();
}

void RelayBoardService::applyOutputs() {
  digitalWrite(RELAY1_PIN, _state.relay1 ? RELAY_ON : RELAY_OFF);
  digitalWrite(RELAY2_PIN, _state.relay2 ? RELAY_ON : RELAY_OFF);
  digitalWrite(RELAY3_PIN, _state.relay3 ? RELAY_ON : RELAY_OFF);
  digitalWrite(RELAY4_PIN, _state.relay4 ? RELAY_ON : RELAY_OFF);
#if RELAY_BOARD_HAS_BUZZER
  digitalWrite(BUZZER_PIN, _state.buzzer ? HIGH : LOW);
#endif
}

void RelayBoardService::registerStatusEndpoint() {
  _server->on(
      RELAY_BOARD_STATUS_PATH,
      HTTP_GET,
      _securityManager->wrapRequest(
          [this](AsyncWebServerRequest* request) {
            AsyncJsonResponse* response = new AsyncJsonResponse(false, 2048);
            JsonObject root = response->getRoot();

            root["board"] = "ESP12F_Relay_X4 (LC-Relay-ESP12-4R-MV)";
            root["mcu"] = "ESP-12F";
            root["platform"] = "esp8266";
            root["chip_id"] = String(ESP.getChipId(), HEX);
            root["cpu_freq_mhz"] = ESP.getCpuFreqMHz();
            root["free_heap"] = ESP.getFreeHeap();
            root["heap_fragmentation"] = ESP.getHeapFragmentation();
            root["flash_chip_size"] = ESP.getFlashChipSize();
            root["flash_chip_speed"] = ESP.getFlashChipSpeed();
            root["sketch_size"] = ESP.getSketchSize();
            root["free_sketch_space"] = ESP.getFreeSketchSpace();
            root["sdk_version"] = ESP.getSdkVersion();
            root["core_version"] = ESP.getCoreVersion();
            root["uptime_ms"] = millis();
            root["reset_reason"] = ESP.getResetReason();
            root["vcc_mv"] = ESP.getVcc();
            root["wifi_ssid"] = WiFi.SSID();
            root["wifi_rssi"] = WiFi.RSSI();
            root["ip"] = WiFi.localIP().toString();
            root["mac"] = WiFi.macAddress();
            root["has_temp_sensor"] = false;
            root["has_buzzer"] = RELAY_BOARD_HAS_BUZZER == 1;
            root["power_led"] = "hardwired";
            root["relay_active"] = "high";

            JsonObject pins = root.createNestedObject("pins");
            pins["ry1"] = RELAY1_PIN;
            pins["ry2"] = RELAY2_PIN;
            pins["ry3"] = RELAY3_PIN;
            pins["ry4"] = RELAY4_PIN;
            pins["di1"] = DI1_PIN;
            pins["di2"] = DI2_PIN;
#if RELAY_BOARD_HAS_BUZZER
            pins["buzzer"] = BUZZER_PIN;
#endif

            JsonObject gpio = root.createNestedObject("gpio_legend");
            gpio["16"] = "DO RY1 (pulses at boot)";
            gpio["14"] = "DO RY2";
            gpio["12"] = "DO RY3";
            gpio["13"] = "DO RY4";
            gpio["4"] = "DI 1 (pullup)";
            gpio["5"] = "DI 2 (pullup + blue LED)";
#if RELAY_BOARD_HAS_BUZZER
            gpio["15"] = "DO buzzer";
#else
            gpio["15"] = "BOOT strap";
#endif
            gpio["0"] = "BOOT strap / flash";
            gpio["2"] = "BOOT strap / ESP LED";
            gpio["1"] = "TX0 UART";
            gpio["3"] = "RX0 UART";

            JsonObject relays = root.createNestedObject("relays");
            relays["relay1"] = _state.relay1;
            relays["relay2"] = _state.relay2;
            relays["relay3"] = _state.relay3;
            relays["relay4"] = _state.relay4;
            relays["buzzer"] = _state.buzzer;
            relays["di1"] = _state.di1;
            relays["di2"] = _state.di2;

            response->setLength();
            request->send(response);
          },
          AuthenticationPredicates::IS_AUTHENTICATED));
}

void RelayBoardService::configureMqtt() {
  if (!_mqttClient->connected()) {
    return;
  }

  String pubTopic = _mqttBasePath + "/state";
  String subTopic = _mqttBasePath + "/set";
  _mqttPubSub.configureTopics(pubTopic, subTopic);
}
