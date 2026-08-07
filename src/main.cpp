#include <ESP8266React.h>
#include <examples/led/LedExampleService.h>
#include <examples/relay/RelayBoardService.h>

#define SERIAL_BAUD_RATE 115200

#ifdef ESP8266
// Route the ADC to measure the 3.3V supply so the twin can report VCC
ADC_MODE(ADC_VCC);
#endif

// Use pointers to avoid early construction issues on ESP32
AsyncWebServer* server;
ESP8266React* esp8266React;
LedExampleService* ledExampleService;
RelayBoardService* relayBoardService;

void setup() {
  // start serial and filesystem
  Serial.begin(SERIAL_BAUD_RATE);
  delay(500);

  Serial.println(F("\n\n=== Weighsoft Relay Board ESP Built-In ==="));
#ifdef ESP32
  Serial.print(F("ESP-IDF version: "));
  Serial.println(esp_get_idf_version());
#endif
  Serial.print(F("Free heap: "));
  Serial.println(ESP.getFreeHeap());

  Serial.println(F("[1/7] Creating web server..."));
  server = new AsyncWebServer(80);
  Serial.println(F("[1/7] Web server created OK"));

  Serial.println(F("[2/7] Initializing framework..."));
  esp8266React = new ESP8266React(server);
  Serial.println(F("[2/7] Framework created OK"));

  Serial.println(F("[3/7] Starting framework services..."));
  esp8266React->begin();
  Serial.println(F("[3/7] Framework initialized OK"));

  Serial.println(F("[4/7] Initializing LED example service..."));
  ledExampleService = new LedExampleService(server,
                                            esp8266React->getSecurityManager(),
                                            esp8266React->getMqttClient()
#if FT_ENABLED(FT_BLE)
                                                ,
                                            nullptr
#endif
  );
  ledExampleService->begin();
  Serial.println(F("[4/7] LED example loaded OK"));

  Serial.println(F("[5/7] Initializing relay board service..."));
  relayBoardService =
      new RelayBoardService(server, esp8266React->getSecurityManager(), esp8266React->getMqttClient());
  relayBoardService->begin();
  Serial.println(F("[5/7] Relay board service loaded OK"));

#if FT_ENABLED(FT_BLE)
  esp8266React->getBleSettingsService()->onBleServerStarted([](BLEServer* bleServer) {
    Serial.println(F("[LED] BLE server ready callback received"));
    if (ledExampleService) {
      ledExampleService->setBleServer(bleServer);
      ledExampleService->configureBle();
    }
  });
#endif

  Serial.println(F("[6/7] Starting web server..."));
  server->begin();
  Serial.println(F("[6/7] Web server started OK"));

  Serial.println(F("=== System Ready! ==="));
  Serial.print(F("Free heap after init: "));
  Serial.println(ESP.getFreeHeap());
}

void loop() {
  esp8266React->loop();
  relayBoardService->loop();
}
