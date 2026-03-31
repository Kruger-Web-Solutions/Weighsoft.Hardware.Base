#include <ESP8266React.h>
#include <examples/led/LedExampleService.h>
#include <examples/diagnostics/DiagnosticsService.h>
#include <examples/serialwriter/SerialWriterService.h>
#include <examples/serialwriter/SerialWriterForwarderService.h>
#include "VersionService.h"
#include "UartModeService.h"
#include "version.h"

#define SERIAL_BAUD_RATE 115200

// Use pointers to avoid early construction issues on ESP32
AsyncWebServer* server;
ESP8266React* esp8266React;
LedExampleService* ledExampleService;
DiagnosticsService* diagnosticsService;
VersionService* versionService;
UartModeService* uartModeService;
SerialWriterService* serialWriterService;
SerialWriterForwarderService* serialWriterForwarderService;

void setup() {
  // start serial and filesystem
  Serial.begin(SERIAL_BAUD_RATE);
  delay(500);

  Serial.println(F("\n\n=== Weighsoft Serial Writer ==="));
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

  Serial.println(F("[1/9] Creating web server..."));
  server = new AsyncWebServer(80);
  Serial.println(F("[1/9] Web server created OK"));

  Serial.println(F("[2/9] Initializing framework..."));
  esp8266React = new ESP8266React(server);
  Serial.println(F("[2/9] Framework created OK"));

  Serial.println(F("[3/9] Starting framework services..."));
  esp8266React->begin();
  Serial.println(F("[3/9] Framework initialized OK"));

  Serial.println(F("[4/9] Initializing version service..."));
  versionService = new VersionService(server, esp8266React->getSecurityManager());
  versionService->begin();
  Serial.println(F("[4/9] Version service loaded OK"));

  Serial.println(F("[5/9] Initializing UART Mode service..."));
  uartModeService = new UartModeService(server, esp8266React->getFS(), esp8266React->getSecurityManager());
  uartModeService->begin();
  Serial.println(F("[5/9] UART Mode service loaded OK"));

  Serial.println(F("[6/9] Initializing LED example service..."));
  ledExampleService = new LedExampleService(server,
                                            esp8266React->getSecurityManager(),
                                            esp8266React->getMqttClient()
#if FT_ENABLED(FT_BLE)
                                            ,
                                            nullptr  // BLE server will be configured via callback
#endif
  );
  ledExampleService->begin();
  Serial.println(F("[6/9] LED example service loaded OK"));

  Serial.println(F("[7/9] Initializing Serial Writer service..."));
  delay(200);  // ESP32-S3: allow hardware to settle before Serial1 init
  serialWriterService = new SerialWriterService(server,
                                                esp8266React->getFS(),
                                                esp8266React->getSecurityManager(),
                                                esp8266React->getMqttClient()
#if FT_ENABLED(FT_BLE)
                                                ,
                                                nullptr  // BLE server will be configured via callback
#endif
  );
  serialWriterService->begin();
  Serial.println(F("[7/9] Serial Writer service loaded OK"));

  Serial.println(F("[7/9] Initializing Serial Writer Forwarder service..."));
  serialWriterForwarderService = new SerialWriterForwarderService(
      server, esp8266React->getFS(), esp8266React->getSecurityManager(), serialWriterService);
  serialWriterForwarderService->begin();
  Serial.println(F("[7/9] Serial Writer Forwarder service loaded OK"));

  Serial.println(F("[8/9] Initializing UART Diagnostics service..."));
  diagnosticsService = new DiagnosticsService(server, esp8266React->getSecurityManager());
  diagnosticsService->begin();
  Serial.println(F("[8/9] Diagnostics service loaded OK"));

  // Link services for Serial1 coordination
  uartModeService->setSerialWriterService(serialWriterService);
  uartModeService->setDiagnosticsService(diagnosticsService);

  // Apply persisted UART mode now that all services are linked
  uartModeService->applyMode();
  Serial.println(F("[8/9] Services linked and UART mode applied"));

#if FT_ENABLED(FT_BLE)
  esp8266React->getBleSettingsService()->onBleServerStarted([](BLEServer* bleServer) {
    if (ledExampleService) {
      Serial.println(F("[LED] BLE server ready callback received"));
      ledExampleService->setBleServer(bleServer);
      ledExampleService->configureBle();
    }
    if (serialWriterService) {
      Serial.println(F("[SerialWriter] BLE server ready callback received"));
      serialWriterService->setBleServer(bleServer);
      serialWriterService->configureBle();
    }
  });
  Serial.println(F("[9/9] BLE callbacks registered OK"));
#endif

  Serial.println(F("[9/9] Starting web server..."));
  server->begin();
  Serial.println(F("[9/9] Web server started OK"));

  Serial.println(F("=== System Ready! ==="));
  Serial.print(F("Free heap after init: "));
  Serial.println(ESP.getFreeHeap());
}

void loop() {
  esp8266React->loop();
  diagnosticsService->loop();
  serialWriterService->loop();
  serialWriterForwarderService->loop();
}
