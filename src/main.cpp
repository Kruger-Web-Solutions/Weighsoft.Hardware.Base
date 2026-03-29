#include <ESP8266React.h>
#include <examples/led/LedExampleService.h>
#include <examples/serial/SerialService.h>
#include <examples/diagnostics/DiagnosticsService.h>
#include <examples/weightforwarder/WeightForwarderService.h>
#include <examples/remoteweight/RemoteWeightService.h>
#ifdef HAS_TFT_DISPLAY
#include <examples/display/DisplayService.h>
#endif
#include "VersionService.h"
#include "UartModeService.h"
#include "version.h"

#define SERIAL_BAUD_RATE 115200

// Use pointers to avoid early construction issues on ESP32
AsyncWebServer* server;
ESP8266React* esp8266React;
LedExampleService* ledExampleService;
SerialService* serialService;
DiagnosticsService* diagnosticsService;
VersionService* versionService;
UartModeService* uartModeService;
WeightForwarderService* weightForwarderService;
RemoteWeightService* remoteWeightService;
#ifdef HAS_TFT_DISPLAY
DisplayService* displayService;
#endif

void setup() {
  // start serial and filesystem
  Serial.begin(SERIAL_BAUD_RATE);
  delay(500);
  
  Serial.println(F("\n\n=== Weighsoft Serial ==="));
  Serial.printf("Version: %s\n", VERSION_STRING);
  Serial.printf("Build: %s %s\n", BUILD_DATE, BUILD_TIME);
  Serial.printf("API: %s\n", API_VERSION);
  #ifdef ESP32
  Serial.print(F("ESP-IDF: "));
  Serial.println(esp_get_idf_version());
  #endif
  Serial.print(F("Free heap: "));
  Serial.println(ESP.getFreeHeap());
  Serial.println();
  
  Serial.println(F("[1/10] Creating web server..."));
  server = new AsyncWebServer(80);
  Serial.println(F("[1/10] Web server created OK"));
  
  Serial.println(F("[2/10] Initializing framework..."));
  esp8266React = new ESP8266React(server);
  Serial.println(F("[2/10] Framework created OK"));
  
  Serial.println(F("[3/10] Starting framework services..."));
  esp8266React->begin();
  Serial.println(F("[3/10] Framework initialized OK"));

  Serial.println(F("[4/10] Initializing version service..."));
  versionService = new VersionService(
      server,
      esp8266React->getSecurityManager()
      );
  versionService->begin();
  Serial.println(F("[4/10] Version service loaded OK"));

  Serial.println(F("[5/10] Initializing UART Mode service..."));
  uartModeService = new UartModeService(
      server,
      esp8266React->getFS(),
      esp8266React->getSecurityManager()
      );
  uartModeService->begin();
  Serial.println(F("[5/10] UART Mode service loaded OK"));

  Serial.println(F("[6/10] Initializing LED example service..."));
  ledExampleService = new LedExampleService(
      server,
      esp8266React->getSecurityManager(),
      esp8266React->getMqttClient()
#if FT_ENABLED(FT_BLE)
      ,nullptr  // BLE server will be configured via callback
#endif
      );
  Serial.println(F("[6/10] LED example service created OK"));

  // load the initial LED settings
  ledExampleService->begin();
  Serial.println(F("[6/10] LED example loaded OK"));

  Serial.println(F("[7/10] Initializing Serial monitor service..."));
  delay(500);  // ESP32-S3: Delay before SerialService initialization
  serialService = new SerialService(
      server,
      esp8266React->getFS(),
      esp8266React->getSecurityManager(),
      esp8266React->getMqttClient()
#if FT_ENABLED(FT_BLE)
      ,nullptr  // BLE server will be configured via callback
#endif
      );
  delay(200);  // ESP32-S3: Delay after SerialService creation
  serialService->begin();
  Serial.println(F("[7/10] Serial service loaded OK"));

  Serial.println(F("[8/10] Initializing UART Diagnostics service..."));
  diagnosticsService = new DiagnosticsService(
      server,
      esp8266React->getSecurityManager()
      );
  diagnosticsService->begin();
  Serial.println(F("[8/10] Diagnostics service loaded OK"));

  // Link services for Serial1 coordination
  diagnosticsService->setSerialService(serialService);
  uartModeService->setSerialService(serialService);
  uartModeService->setDiagnosticsService(diagnosticsService);
  Serial.println(F("[8/10] Services linked for Serial1 coordination"));
  
  // Apply persisted UART mode now that all services are linked
  uartModeService->applyMode();
  Serial.println(F("[8/10] Persisted UART mode applied"));

  Serial.println(F("[9/10] Initializing Weight Forwarder service..."));
  weightForwarderService = new WeightForwarderService(
      server,
      esp8266React->getFS(),
      esp8266React->getSecurityManager(),
      esp8266React->getMqttClient(),
      serialService
      );
  weightForwarderService->begin();
  Serial.println(F("[9/10] Weight Forwarder service loaded OK"));

  Serial.println(F("[9.5/10] Initializing Remote Weight receiver..."));
  remoteWeightService = new RemoteWeightService(
      server,
      esp8266React->getFS(),
      esp8266React->getSecurityManager()
      );
  remoteWeightService->begin();
  Serial.println(F("[9.5/10] Remote Weight receiver loaded OK"));

#ifdef HAS_TFT_DISPLAY
  Serial.println(F("[9.8/10] Initializing TFT Display service..."));
  displayService = new DisplayService(remoteWeightService);
  displayService->begin();
  Serial.println(F("[9.8/10] TFT Display service loaded OK"));
#endif

#if FT_ENABLED(FT_BLE)
  // Register callbacks after both services exist so callback never sees null
  esp8266React->getBleSettingsService()->onBleServerStarted(
    [](BLEServer* bleServer) {
      if (ledExampleService) {
        Serial.println(F("[LED] BLE server ready callback received"));
        ledExampleService->setBleServer(bleServer);
        ledExampleService->configureBle();
      }
      if (serialService) {
        Serial.println(F("[Serial] BLE server ready callback received"));
        serialService->setBleServer(bleServer);
        serialService->configureBle();
      }
    }
  );
  Serial.println(F("[10/10] BLE callbacks registered OK"));
#endif

  Serial.println(F("[10/10] Starting web server..."));
  // start the server
  server->begin();
  Serial.println(F("[10/10] Web server started OK"));
  
  Serial.println(F("=== System Ready! ==="));
  Serial.print(F("Free heap after init: "));
  Serial.println(ESP.getFreeHeap());
}

void loop() {
  // run the framework's loop function
  esp8266React->loop();
  
  // read serial data
  serialService->loop();
  
  // run diagnostic tests
  diagnosticsService->loop();
  
  // process weight forwarding
  weightForwarderService->loop();

#ifdef HAS_TFT_DISPLAY
  // update TFT display
  displayService->loop();
#endif
}
