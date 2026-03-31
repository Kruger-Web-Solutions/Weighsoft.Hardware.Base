#include <ESP8266React.h>
#include <examples/led/LedExampleService.h>
#include <examples/growbox/GrowboxSettingsService.h>
#include <examples/growbox/GrowboxAutomationService.h>
#include <examples/growbox/GrowboxService.h>

#define SERIAL_BAUD_RATE 115200

// Use pointers to avoid early construction issues on ESP32
AsyncWebServer* server;
ESP8266React* esp8266React;
LedExampleService* ledExampleService;
GrowboxSettingsService* growboxSettingsService;
GrowboxAutomationService* growboxAutomationService;
GrowboxService* growboxService;

void setup() {
  // start serial and filesystem
  Serial.begin(SERIAL_BAUD_RATE);
  delay(500);

  Serial.println(F("\n\n=== Weighsoft Hardware UI Starting ==="));
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
  ledExampleService = new LedExampleService(
      server,
      esp8266React->getSecurityManager(),
      esp8266React->getMqttClient()
#if FT_ENABLED(FT_BLE)
      ,
      nullptr  // BLE server will be configured via callback
#endif
  );
  ledExampleService->begin();
  Serial.println(F("[4/7] LED example service OK"));

  Serial.println(F("[5/7] Initializing growbox settings service..."));
  growboxSettingsService = new GrowboxSettingsService(
      server,
      esp8266React->getFS(),
      esp8266React->getSecurityManager());
  growboxSettingsService->begin();
  Serial.println(F("[5/7] Growbox settings service OK"));

  Serial.println(F("[6/7] Initializing growbox automation service..."));
  growboxAutomationService = new GrowboxAutomationService(
      server,
      esp8266React->getFS(),
      esp8266React->getSecurityManager());
  growboxAutomationService->begin();
  Serial.println(F("[6/7] Growbox automation service OK"));

  Serial.println(F("[6/7] Initializing growbox service..."));
  growboxService = new GrowboxService(
      server,
      esp8266React->getSecurityManager(),
      esp8266React->getMqttClient(),
      growboxSettingsService,
      growboxAutomationService);
  growboxService->begin();
  Serial.println(F("[6/7] Growbox service OK"));

#if FT_ENABLED(FT_BLE)
  // Register BLE callbacks for all services that need BLE when the server is ready
  esp8266React->getBleSettingsService()->onBleServerStarted(
      [](BLEServer* bleServer) {
        Serial.println(F("[BLE] BLE server ready callback received"));
        if (ledExampleService) {
          ledExampleService->setBleServer(bleServer);
          ledExampleService->configureBle();
        }
        // Phase 5: growbox BLE
        if (growboxService) {
          growboxService->setBleServer(bleServer);
          growboxService->configureBle();
        }
      });
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
  growboxService->loop();
}
