#include <ESP8266React.h>
#include <examples/led/LedExampleService.h>
#include <examples/weighing/WeighingService.h>
#include "VersionService.h"
#include "version.h"

#define SERIAL_BAUD_RATE 115200

AsyncWebServer*    server;
ESP8266React*      esp8266React;
LedExampleService* ledExampleService;
WeighingService*   weighingService;
VersionService*    versionService;

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(500);

  Serial.println(F("\n\n=== Weighsoft Weighing Board Starting ==="));
  Serial.printf("Version: %s  Build: %s %s\n", VERSION_STRING, BUILD_DATE, BUILD_TIME);
  #ifdef ESP32
  Serial.print(F("ESP-IDF: "));
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

  Serial.println(F("[4/7] Initializing version service (OIML 6.2.1)..."));
  versionService = new VersionService(
      server,
      esp8266React->getSecurityManager()
  );
  versionService->begin();
  Serial.println(F("[4/7] Version service loaded OK"));

  Serial.println(F("[5/7] Initializing LED example service..."));
  ledExampleService = new LedExampleService(
      server,
      esp8266React->getSecurityManager(),
      esp8266React->getMqttClient()
#if FT_ENABLED(FT_BLE)
      , nullptr  // BLE server will be configured via callback
#endif
  );
  ledExampleService->begin();
  Serial.println(F("[5/7] LED example service loaded OK"));

  Serial.println(F("[6/7] Initializing weighing service..."));
  weighingService = new WeighingService(
      server,
      esp8266React->getFS(),
      esp8266React->getSecurityManager(),
      esp8266React->getMqttClient()
#if FT_ENABLED(FT_BLE)
      , nullptr  // BLE server will be configured via callback
#endif
  );
  weighingService->begin();
  Serial.println(F("[6/7] Weighing service loaded OK"));

#if FT_ENABLED(FT_BLE)
  // Single BLE callback for both LED and Weighing services
  esp8266React->getBleSettingsService()->onBleServerStarted(
    [](BLEServer* bleServer) {
      Serial.println(F("[BLE] Server ready - configuring services..."));
      if (ledExampleService) {
        ledExampleService->setBleServer(bleServer);
        ledExampleService->configureBle();
      }
      if (weighingService) {
        weighingService->setBleServer(bleServer);
        weighingService->configureBle();
      }
    }
  );
  Serial.println(F("[6/7] BLE callbacks registered OK"));
#endif

  Serial.println(F("[7/7] Starting web server..."));
  server->begin();
  Serial.println(F("[7/7] Web server started OK"));

  Serial.println(F("=== System Ready! ==="));
  Serial.print(F("Free heap after init: "));
  Serial.println(ESP.getFreeHeap());
}

void loop() {
  esp8266React->loop();
  weighingService->loop();
}
