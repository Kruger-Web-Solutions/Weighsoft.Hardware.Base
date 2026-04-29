#include <ESP8266React.h>
#ifdef ESP32
#include <WiFi.h>
#endif
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
#if ARDUINO_USB_CDC_ON_BOOT
  // CRITICAL: make USB-CDC (HWCDC) writes non-blocking. The default tx timeout is
  // 250 ms — if no PC has the COM port open, the internal TX buffer fills and any
  // Serial.print() blocks for that long. When those calls happen on the AsyncTCP
  // request-handler task (e.g. the RemoteWeight echo on every weight POST, or any
  // Serial.println from a web callback), the web server starves and the device
  // eventually hangs. With timeout 0, writes drop bytes immediately if no host is
  // reading.
  Serial.setTxTimeoutMs(0);
  // With USB CDC bound to Serial, UART0 (GPIO43/44 on ESP32-S3) is no longer
  // auto-initialised by the framework. Bring it up explicitly so the optional
  // weight-echo can mirror to a USB-TTL adapter (e.g. on-board CH343) wired
  // to UART0.
  Serial0.begin(SERIAL_BAUD_RATE);
#endif
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

  // run remote weight receiver housekeeping (audit log + periodic USB echo)
  remoteWeightService->loop();

  // USB-CDC TX liveness watchdog (ESP32-S3 native USB only).
  // HWCDC can get stuck with its internal `connected` flag at false after a
  // brief host-side hiccup (Windows USB Selective Suspend, ScaleCOM read
  // backpressure, etc.). Once stuck, every Serial.write() takes the
  // "not connected" branch (HWCDC.cpp ~419) and only buffers into the ring
  // buffer — no bytes ever reach USB. The recovery loop in HWCDC::write()
  // doesn't run because we set tx_timeout_ms=0 (required to keep AsyncTCP
  // unblocked). Result: COM3 appears dead until the user physically
  // disconnects/reconnects the USB cable.
  //
  // This watchdog detects the stuck state and resets HWCDC (which forces
  // USB re-enumeration via the BUS_RESET handling in HWCDC::begin()), so
  // the host-side COM port handle remains valid and output resumes
  // automatically without manual intervention.
#if ARDUINO_USB_CDC_ON_BOOT
  {
    static unsigned long lastCdcCheck = 0;
    static unsigned long cdcStuckSince = 0;
    unsigned long now = millis();
    if (now - lastCdcCheck >= 5000UL) {
      lastCdcCheck = now;
      // <64 bytes free out of 256 means the ring buffer is essentially full
      // and not draining. Healthy idle state has the full buffer free.
      if (Serial.availableForWrite() < 64) {
        if (cdcStuckSince == 0) {
          cdcStuckSince = now;
        } else if (now - cdcStuckSince >= 30000UL) {
          // Stuck for >=30 s — reset HWCDC to force host re-enumeration.
          Serial.end();
          delay(50);
          Serial.begin(SERIAL_BAUD_RATE);
          Serial.setTxTimeoutMs(0);
          Serial.println(F("[main] USB-CDC reset (was stuck for >=30s)"));
          cdcStuckSince = 0;
        }
      } else {
        cdcStuckSince = 0;  // healthy — reset the stuck timer
      }
    }
  }
#endif

  // Network watchdog. If WiFi has been disconnected for more than 60 s,
  // the chip is stuck in a state the application-level reconnect can't
  // recover from (we've seen this on the N16R8: AsyncTCP/netif silently
  // dies but the CPU keeps running, so the loop ticks forever and no
  // task watchdog fires). Force a full chip restart in that case so the
  // device auto-recovers without needing the user to physically press
  // RST.
  //
  // The 60 s threshold is generous — the SDK's auto-reconnect (enabled
  // in WiFiSettingsService) gets first crack, and our manual reconnect
  // backoff caps at ~30 s, so legitimate transient drops well under 60 s
  // never trigger the reboot.
#ifdef ESP32
  {
    static unsigned long lastWifiCheck = 0;
    static unsigned long wifiDownSince = 0;
    unsigned long now = millis();
    if (now - lastWifiCheck >= 5000UL) {
      lastWifiCheck = now;
      if (WiFi.isConnected()) {
        wifiDownSince = 0;  // healthy
      } else {
        if (wifiDownSince == 0) {
          wifiDownSince = now;
        } else if (now - wifiDownSince >= 60000UL) {
          Serial.println(F("[main] WiFi down for >=60s — forcing ESP.restart() for auto-recovery"));
          delay(100);  // let Serial flush
          ESP.restart();
        }
      }
    }
  }
#endif

#ifdef HAS_TFT_DISPLAY
  // update TFT display
  displayService->loop();
#endif
}
