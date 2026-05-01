#include <ESP8266React.h>
#include <examples/led/LedExampleService.h>
#include <examples/serial/SerialService.h>
#include <examples/serial/KnownWritersService.h>
#include <examples/serialwriter/SerialWriterService.h>
#include <examples/serialwriter/SerialWriterForwarderService.h>
#include <examples/diagnostics/DiagnosticsService.h>
#include <examples/weightforwarder/WeightForwarderService.h>
#include "VersionService.h"
#include "UartModeService.h"
#include "MdnsService.h"
#include "version.h"

#ifdef ESP32
#include <esp_system.h>
#include <WiFi.h>
#endif

#ifdef ESP32
// Human-readable label for WiFi.getMode() so the periodic [WiFi] log line is grep-able.
static const char* wifiModeLabel(wifi_mode_t mode) {
  switch (mode) {
    case WIFI_OFF:    return "OFF";
    case WIFI_STA:    return "STA";
    case WIFI_AP:     return "AP";
    case WIFI_AP_STA: return "AP+STA";
    default:          return "OTHER";
  }
}

// One-line snapshot of the current connection state. Logged at boot once
// the framework is up, and again every 60 s alongside [Heap]. Keeps the
// IP / mode / signal strength visible from the COM port stream without
// needing a REST poll.
static void logWifiSnapshot(const char* tag) {
  wifi_mode_t mode = WiFi.getMode();
  bool staConnected = (mode == WIFI_STA || mode == WIFI_AP_STA) && WiFi.isConnected();
  String staIp = staConnected ? WiFi.localIP().toString() : String("0.0.0.0");
  uint8_t apClients = (mode == WIFI_AP || mode == WIFI_AP_STA) ? WiFi.softAPgetStationNum() : 0;
  String apIp = (mode == WIFI_AP || mode == WIFI_AP_STA) ? WiFi.softAPIP().toString() : String("0.0.0.0");
  Serial.printf("[%s] mode=%s sta_ip=%s sta_ssid=%s rssi=%d ap_ip=%s ap_clients=%u\n",
                tag,
                wifiModeLabel(mode),
                staIp.c_str(),
                staConnected ? WiFi.SSID().c_str() : "-",
                staConnected ? (int)WiFi.RSSI() : 0,
                apIp.c_str(),
                (unsigned)apClients);
}
#endif

#define SERIAL_BAUD_RATE 115200

#ifdef ESP32
/** Human-readable ROM reset cause for stability diagnosis (see docs/task-logs/TASK-mandatory-roadmap-decision-log-workflow.md). */
static const char* resetReasonLabel(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_UNKNOWN:
      return "UNKNOWN";
    case ESP_RST_POWERON:
      return "POWERON";
    case ESP_RST_EXT:
      return "EXT_SYS_RST";
    case ESP_RST_SW:
      return "SW_RESET";
    case ESP_RST_PANIC:
      return "PANIC/Exception";
    case ESP_RST_INT_WDT:
      return "INT_WDT";
    case ESP_RST_TASK_WDT:
      return "TASK_WDT";
    case ESP_RST_WDT:
      return "WDT_OTHER";
    case ESP_RST_DEEPSLEEP:
      return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:
      return "BROWNOUT";
    case ESP_RST_SDIO:
      return "SDIO";
    default:
      return "OTHER";
  }
}
#endif

// Compose the mode-aware AP SSID for the current device mode.
static String composeApSsid(UartModeService* m) {
  if (!m) return "Weighsoft-Esp-NEW";
  if (m->isWriter()) return "Weighsoft-Esp-Writer";
  if (m->isDiagnostics()) return "Weighsoft-Esp-Diagnostics";
  if (m->isReader()) return "Weighsoft-Esp-Reader";
  return "Weighsoft-Esp-NEW";
}

// Update the AP SSID to match the current mode, but only if the current SSID
// follows the auto-managed Weighsoft-Esp-* pattern. Hand-edited SSIDs are preserved.
static void syncApSsidToMode(StatefulService<APSettings>* ap, UartModeService* m) {
  if (!ap || !m) return;
  String desired = composeApSsid(m);
  ap->update([&](APSettings& s) {
    bool isAutoManaged = s.ssid.startsWith("Weighsoft-Esp-");
    if (isAutoManaged && s.ssid != desired) {
      s.ssid = desired;
      return StateUpdateResult::CHANGED;
    }
    return StateUpdateResult::UNCHANGED;
  }, "mode-ssid-sync");
}

// Use pointers to avoid early construction issues on ESP32
AsyncWebServer* server;
ESP8266React* esp8266React;
LedExampleService* ledExampleService;
SerialService* serialService;
KnownWritersService* knownWritersService;
SerialWriterService* serialWriterService;
SerialWriterForwarderService* serialWriterForwarderService;
DiagnosticsService* diagnosticsService;
VersionService* versionService;
UartModeService* uartModeService;
WeightForwarderService* weightForwarderService;
MdnsService* mdnsService;

void setup() {
  // start serial and filesystem
  Serial.begin(SERIAL_BAUD_RATE);
  delay(500);
  
  Serial.println(F("\n\n=== Weighsoft Serial ==="));
#ifdef ESP32
  {
    esp_reset_reason_t rr = esp_reset_reason();
    Serial.printf("[Boot] esp_reset_reason=%d (%s)\n", (int)rr, resetReasonLabel(rr));
  }
#endif
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

  knownWritersService = new KnownWritersService(
      server,
      esp8266React->getFS(),
      esp8266React->getSecurityManager()
      );
  knownWritersService->begin();
  serialService->setKnownWritersService(knownWritersService);
  Serial.println(F("[7/10] KnownWriters service loaded and wired OK"));

  serialWriterService = new SerialWriterService(
      server,
      esp8266React->getFS(),
      esp8266React->getSecurityManager(),
      esp8266React->getMqttClient()
      );
  serialWriterService->begin();

  serialWriterForwarderService = new SerialWriterForwarderService(
      server,
      esp8266React->getFS(),
      esp8266React->getSecurityManager(),
      serialWriterService
      );
  serialWriterForwarderService->begin();
  Serial.println(F("[7/10] Serial Writer services loaded OK"));

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
  uartModeService->setWriterService(serialWriterService);
  uartModeService->setForwarderService(serialWriterForwarderService);
  uartModeService->setDiagnosticsService(diagnosticsService);
  Serial.println(F("[8/10] Services linked for Serial1 coordination"));
  
  // Apply persisted UART mode now that all services are linked
  uartModeService->applyMode();
  Serial.println(F("[8/10] Persisted UART mode applied"));

  // Sync AP SSID to the current UART mode at boot
  syncApSsidToMode(esp8266React->getAPSettingsService(), uartModeService);

  // Re-sync AP SSID and refresh mDNS whenever the user changes mode
  uartModeService->addUpdateHandler([&](const String& origin) {
    syncApSsidToMode(esp8266React->getAPSettingsService(), uartModeService);
    if (mdnsService) mdnsService->refresh();
  }, false);

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

  mdnsService = new MdnsService(uartModeService);
  mdnsService->begin();

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
#ifdef ESP32
  Serial.printf("[Heap] at boot: free=%u max_alloc_heap=%u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
  // WiFi may not have associated yet at this point — the framework's
  // manageSTA loop kicks in shortly. Log whatever we have now so the
  // boot stream shows mode/IP, and the periodic [WiFi] log in loop()
  // will pick up the live IP once association completes.
  logWifiSnapshot("WiFi");
#endif
}

void loop() {
  // run the framework's loop function
  esp8266React->loop();

  // read serial data
  serialService->loop();
  if (knownWritersService) knownWritersService->loop();

  // write serial data
  serialWriterService->loop();
  serialWriterForwarderService->loop();

  // run diagnostic tests
  diagnosticsService->loop();

  // process weight forwarding
  weightForwarderService->loop();

#ifdef ESP32
  {
    static unsigned long heapLogMs = 0;
    const unsigned long now = millis();
    if (heapLogMs == 0) {
      heapLogMs = now;
    } else if ((unsigned long)(now - heapLogMs) >= 60000UL) {
      heapLogMs = now;
      Serial.printf("[Heap] free=%u min_free=%u max_alloc_heap=%u\n",
                    (unsigned)ESP.getFreeHeap(),
                    (unsigned)ESP.getMinFreeHeap(),
                    (unsigned)ESP.getMaxAllocHeap());
      // Pair the heap log with a WiFi snapshot so the COM port stream
      // tells you both "is the chip healthy?" and "is it on the network?"
      // in one timestamp. Same 60 s cadence — cheap, just reads accessors.
      logWifiSnapshot("WiFi");
    }
  }
  // Let IDLE/Wi‑Fi stack run between heavy loop iterations (stability under load).
  yield();
#endif
}
