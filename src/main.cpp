#include <ESP8266React.h>
#include <examples/led/LedExampleService.h>
#include <examples/diagnostics/DiagnosticsService.h>
#include <examples/serialwriter/SerialWriterService.h>
#include <examples/serialwriter/SerialWriterForwarderService.h>
#include "VersionService.h"
#include "UartModeService.h"
#include "version.h"

#ifdef ESP32
#include <USB.h>
#include <USBCDC.h>
#endif

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

#ifdef ESP32
// Dedicated native USB CDC stream used as the forwarder data sink. Kept fully
// separate from `Serial` (UART0/logs) so that PC-side scale tools see a clean
// weight-only stream on the native USB-OTG port. See docs/SERIAL-WRITER-EXAMPLE.md.
USBCDC dataUsbCdc(0);
#endif

void setup() {
  // start serial and filesystem
  Serial.begin(SERIAL_BAUD_RATE);
  delay(500);

#ifdef ESP32
  // Bring up the native USB CDC port as a dedicated forwarder data sink. This is
  // independent of `Serial` (UART0); PC-side scale tools see clean weight strings
  // here while developer logs continue to flow to the on-board UART bridge port.
  USB.begin();
  dataUsbCdc.begin();
#endif

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
  versionService = new VersionService(server, esp8266React->getSecurityManager());
  versionService->begin();
  Serial.println(F("[4/10] Version service loaded OK"));

  Serial.println(F("[5/10] Initializing UART Mode service..."));
  uartModeService = new UartModeService(server, esp8266React->getFS(), esp8266React->getSecurityManager());
  uartModeService->begin();
  Serial.println(F("[5/10] UART Mode service loaded OK"));

  Serial.println(F("[6/10] Initializing LED example service..."));
  ledExampleService = new LedExampleService(server,
                                            esp8266React->getSecurityManager(),
                                            esp8266React->getMqttClient()
#if FT_ENABLED(FT_BLE)
                                            ,
                                            nullptr  // BLE server will be configured via callback
#endif
  );
  ledExampleService->begin();
  Serial.println(F("[6/10] LED example service loaded OK"));

  Serial.println(F("[7/10] Initializing Serial Writer service..."));
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
#ifdef ESP32
  serialWriterService->setDataUsbCdc(&dataUsbCdc);
#endif
  serialWriterService->begin();
  Serial.println(F("[7/10] Serial Writer service loaded OK"));

  Serial.println(F("[8/10] Initializing Serial Writer Forwarder service..."));
  serialWriterForwarderService = new SerialWriterForwarderService(
      server, esp8266React->getFS(), esp8266React->getSecurityManager(), serialWriterService);
  serialWriterForwarderService->begin();
  Serial.println(F("[8/10] Serial Writer Forwarder service loaded OK"));

  Serial.println(F("[9/10] Initializing UART Diagnostics service..."));
  diagnosticsService = new DiagnosticsService(server, esp8266React->getSecurityManager());
  diagnosticsService->begin();
  Serial.println(F("[9/10] Diagnostics service loaded OK"));

  // Link services for Serial1 coordination
  uartModeService->setSerialWriterService(serialWriterService);
  uartModeService->setDiagnosticsService(diagnosticsService);

  // Apply persisted UART mode now that all services are linked
  uartModeService->applyMode();
  Serial.println(F("Services linked and UART mode applied"));

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
  Serial.println(F("[10/10] BLE callbacks registered OK"));
#endif

  Serial.println(F("[10/10] Starting web server..."));
  server->begin();
  Serial.println(F("[10/10] Web server started OK"));

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
