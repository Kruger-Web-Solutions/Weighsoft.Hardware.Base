#include <ESP8266React.h>
#include <examples/led/LedExampleService.h>
#include <examples/serial/SerialService.h>
#include <examples/serial/KnownWritersService.h>
#include <examples/serialwriter/SerialWriterService.h>
#include <examples/serialwriter/SerialWriterForwarderService.h>
#include <examples/diagnostics/DiagnosticsService.h>
#include <examples/weightforwarder/WeightForwarderService.h>
#include <examples/remoteweight/RemoteWeightService.h>
#include <examples/tftdisplay/TftWeightDisplayService.h>
#include <examples/lcddisplay/LcdDisplayService.h>
#include <examples/wsoft/WSoftSettingsService.h>
#include <examples/wsoft/WSoftStatusService.h>
#include <examples/wsoft/WSoftSwitchesService.h>
#include <examples/wsoft/WSoftProductsService.h>
#if FT_ENABLED(FT_WSOFT_RELAY)
#include <examples/relay/RelayBoardService.h>
#endif
#if FT_ENABLED(FT_WSOFT)
#include <wsoft_screen.h>
#include <hilo.h>
#include <weighings.h>
#endif
#include "VersionService.h"
#include "UartModeService.h"
#include "BoardDisplayService.h"
#include "MdnsService.h"
#include "MdnsBrowser.h"
#include "WatchdogService.h"
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
#if FT_ENABLED(FT_WSOFT)
  return "Weighsoft-Esp-Reader";  // a W-Soft board weighs unless it is a Writer (same hotspot name as before)
#else
  if (m->isReader()) return "Weighsoft-Esp-Reader";
  return "Weighsoft-Esp-NEW";
#endif
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
MdnsBrowser* mdnsBrowser;
WatchdogService* watchdogService;
BoardDisplayService* boardDisplayService;
#if FT_ENABLED(FT_REMOTE_WEIGHT)
RemoteWeightService* remoteWeightService;
#endif
#if FT_ENABLED(FT_TFT_WEIGHT_SCREEN)
TftWeightDisplayService* tftWeightDisplayService;
#endif
#if FT_ENABLED(FT_DISPLAY_LCD)
LcdDisplayService* lcdDisplayService;
#endif
#if FT_ENABLED(FT_WSOFT)
WSoftSettingsService* wsoftSettingsService;
WSoftStatusService* wsoftStatusService;
WSoftSwitchesService* wsoftSwitchesService;
WSoftProductsService* wsoftProductsService;
#if FT_ENABLED(FT_WSOFT_RELAY)
RelayBoardService* relayBoardService;
#endif
// The source and the role read at boot; the loop restarts the board when either changes.
WSoftSource wsoftBootSource = WSOFT_SOURCE_RS485;
bool wsoftBootWriter = false;

// RS232 mode: hands the Serial reader's extracted weight to the W-Soft screen task.
static bool wsoftReadSerialLine(char* weight, size_t size, uint32_t* receivedMs, int* unit) {
  (void)unit;  // an RS232 scale line is kg
  if (serialService == nullptr) return false;
  // Latest-reading slot: newest line at full precision, stamped when it arrived.
  return serialService->latestWeight(weight, size, receivedMs);
}

// Writer mode: hands the weight received from the weigh board to the W-Soft screen task.
static bool wsoftReadWriterWeight(char* weight, size_t size, uint32_t* receivedMs, int* unit) {
  if (serialWriterForwarderService == nullptr) return false;
  return serialWriterForwarderService->latestWeight(weight, size, receivedMs, unit);
}
#endif

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

#if FT_ENABLED(FT_WSOFT_RELAY)
  // The LED example's pin is one of the Hi-Lo relay pins on this board (the RGB LED pads),
  // so the example is not created: nothing but the relay service may drive those pins.
  ledExampleService = nullptr;
  Serial.println(F("[6/10] LED example off (its pin is a relay output on this board)"));
#else
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
#endif

#if !FT_ENABLED(FT_WSOFT)
  // W-Soft (FT_WSOFT) owns Serial1 for its RS485 DJB link, so none of the Serial1
  // services below are created on that board (one RS485 master).
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

  boardDisplayService = new BoardDisplayService(
      serialService,
      serialWriterService,
      serialWriterForwarderService,
      uartModeService
      );
  boardDisplayService->begin();
#endif  // !FT_WSOFT

  // Sync AP SSID to the current UART mode at boot
  syncApSsidToMode(esp8266React->getAPSettingsService(), uartModeService);

  // Re-sync AP SSID and refresh mDNS whenever the user changes mode
  uartModeService->addUpdateHandler([&](const String& origin) {
    syncApSsidToMode(esp8266React->getAPSettingsService(), uartModeService);
    if (mdnsService) mdnsService->refresh();
  }, false);

#if !FT_ENABLED(FT_WSOFT)
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
#endif  // !FT_WSOFT

#if FT_ENABLED(FT_WSOFT)
  // W-Soft weigh screen (Waveshare ESP32-S3 4.3 + DJB over RS485): options first,
  // then the live status service, which starts the screen task.
  Serial.println(F("[9/10] Initializing W-Soft services..."));
  wsoftSettingsService = new WSoftSettingsService(
      server,
      esp8266React->getFS(),
      esp8266React->getSecurityManager()
      );
  wsoftSettingsService->begin();
  wsoftBootSource = wsoftSettingsService->source();
  // One port: the board weighs (Weigh, the default) or follows a weigh board over Wi-Fi and
  // sends its weight out of the port (Writer). The Hub link is the board's either way.
  wsoftBootWriter = uartModeService->isWriter();
  wsoftSettingsService->setWriterMode(wsoftBootWriter);
  if (wsoftBootWriter) {
    // The existing Writer services own Serial1; W-Soft shows the weight they receive.
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
    uartModeService->setWriterService(serialWriterService);
    uartModeService->setForwarderService(serialWriterForwarderService);
    uartModeService->applyMode();
    wsoft_screen_set_line_reader(wsoftReadWriterWeight);
    // SAVE on a Writer is saved by the weigh board (Rhyno, 14 Nov 2026), counted once.
    weighings_set_remote_save(SerialWriterForwarderService::requestSave);
    Serial.println(F("[9/10] W-Soft role: Writer (weight from a weigh board, out of the port)"));
  } else if (wsoftBootSource == WSOFT_SOURCE_RS232) {
    // RS232 scale: the existing Serial reader owns Serial1 (its page sets baud, format
    // and the weight pattern); W-Soft reads the weight it extracts. No RS485 poll.
    serialService = new SerialService(
        server,
        esp8266React->getFS(),
        esp8266React->getSecurityManager(),
        esp8266React->getMqttClient()
#if FT_ENABLED(FT_BLE)
        ,nullptr
#endif
        );
    serialService->begin();
    wsoft_screen_set_line_reader(wsoftReadSerialLine);
    Serial.println(F("[9/10] W-Soft source: RS232 scale (Serial reader)"));
  }
  wsoftStatusService = new WSoftStatusService(
      server,
      esp8266React->getSecurityManager(),
      esp8266React->getMqttClient(),
      wsoftSettingsService
      );
  wsoftStatusService->begin();
  // Switches, the screen PIN and the RS485 scale setup on the web (after begin: the stores are loaded).
  wsoftSwitchesService = new WSoftSwitchesService(server, esp8266React->getSecurityManager());
  // Products on the web page: Hub ones read-only, board ones added and edited here.
  wsoftProductsService = new WSoftProductsService(server, esp8266React->getSecurityManager());
#if FT_ENABLED(FT_WSOFT_RELAY)
  // Hi-Lo relay outputs: all OFF here, then they follow the zone the screen task posts.
  relayBoardService = new RelayBoardService(server, esp8266React->getSecurityManager(), wsoftSettingsService);
  relayBoardService->begin();
#endif
  Serial.println(F("[9/10] W-Soft services loaded OK"));
#endif

#if FT_ENABLED(FT_REMOTE_WEIGHT)
  // ESP-to-ESP weight receiver: accepts weight POSTs from a remote Forwarder
  // at /rest/remoteWeight, pushes to /ws/remoteWeight, optionally echoes to
  // USB and (with FT_TFT_WEIGHT_SCREEN) drives a TFT screen. Compiled in only
  // on boards that set FT_REMOTE_WEIGHT (the S3 bridge + the TFT viewer envs).
  Serial.println(F("[9/10] Initializing Remote Weight receiver..."));
  remoteWeightService = new RemoteWeightService(
      server,
      esp8266React->getFS(),
      esp8266React->getSecurityManager()
      );
  remoteWeightService->begin();
  Serial.println(F("[9/10] Remote Weight receiver loaded OK"));

#if FT_ENABLED(FT_TFT_WEIGHT_SCREEN)
  // TFT weight screen — renders the received weight on an ILI9488/ILI9341
  // panel. Reads from the RemoteWeightService above (hard dependency), so it is
  // built here after the receiver is up. Observe-only: never touches Serial1 or
  // UartModeService. The ~4s boot colour-test runs before server->begin().
  Serial.println(F("[9/10] Initializing TFT weight screen..."));
  tftWeightDisplayService = new TftWeightDisplayService(remoteWeightService);
  tftWeightDisplayService->begin();
  Serial.println(F("[9/10] TFT weight screen loaded OK"));
#endif
#endif

  // mDNS announcement — defers MDNS.begin() until WiFi is up, then adds the
  // _weighsoft._tcp service to the responder ArduinoOTA already started.
  // The coexistence fix in commit 0f91835 prevents the WiFi-scan conflict.
  mdnsService = new MdnsService(uartModeService);
  mdnsService->begin();

  // mDNS browser — DISABLED. The MDNS.queryService() call inside scan() hangs
  // the Arduino main loop indefinitely on this ESP32-S3 setup, freezing
  // SerialService and other periodic services. Web server keeps responding
  // (different task) so it looks alive in the UI but no data flows.
  // /rest/discovered is still served (returns empty list) so the pickup UI
  // doesn't break; we just don't actively browse.
  // TODO: replace with non-blocking mDNS browse via ESP-IDF mdns_query_async,
  // or run the scan on a separate FreeRTOS task.
  // mdnsBrowser = new MdnsBrowser(server, esp8266React->getSecurityManager());
  // mdnsBrowser->begin();
  mdnsBrowser = nullptr;

  // App-level watchdog: 4-min grace from boot, restarts the chip after 4 min
  // of continuous unhealthy state (no STA + no AP client, or low heap). The
  // grace + the 4-min unhealthy limit together guarantee at most one restart
  // per ~4 minutes — anti-death-loop.
  watchdogService = new WatchdogService(
      server,
      esp8266React->getFS(),
      esp8266React->getSecurityManager()
      );
  watchdogService->begin();

#if FT_ENABLED(FT_DISPLAY_LCD)
  // Character-LCD device (16x2 I2C). Observe-only — never touches Serial1 or
  // UartModeService. Compiled in only on boards that set FT_DISPLAY_LCD.
  Serial.println(F("[10/10] Initializing LCD display service..."));
  lcdDisplayService = new LcdDisplayService(
      server,
      esp8266React->getSecurityManager(),
      esp8266React->getMqttClient()
#if FT_ENABLED(FT_BLE)
      ,nullptr  // BLE server configured via the callback below
#endif
      );
  lcdDisplayService->begin();
  Serial.println(F("[10/10] LCD display service loaded OK"));
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
#if FT_ENABLED(FT_DISPLAY_LCD)
      if (lcdDisplayService) {
        lcdDisplayService->setBleServer(bleServer);
        lcdDisplayService->configureBle();
      }
#endif
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

#if !FT_ENABLED(FT_WSOFT)
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
#endif

#if FT_ENABLED(FT_WSOFT)
  if (serialService) serialService->loop();
  if (serialWriterService) serialWriterService->loop();
  if (serialWriterForwarderService) {
    serialWriterForwarderService->loop();
    // Writer: follows the weigh board's product when the weigh board picks or changes one, so
    // screen, relays and Hub match. Between changes a product picked on this board stays
    // (Rhyno, 14 Nov 2026: the Writer picks its own product too). A weigh board with no
    // product at first contact leaves this board's pick alone.
    static bool feedProductSeen = false;
    static String feedProduct;
    String product;
    if (serialWriterForwarderService->latestProduct(&product)) {
      if (feedProductSeen ? product != feedProduct : product.length() > 0) hilo_follow(product.c_str());
      feedProductSeen = true;
      feedProduct = product;
    }
    // Writer: the weigh board's "what stops a save" and each of its Save results on this
    // screen. No feed for 3 s = no signal (the same 3 s rule as the weight).
    static uint32_t saveCountSeen = 0;
    static bool saveCountKnown = false;
    int saveWaiting = 0, saveResult = 0;
    uint32_t saveCount = 0;
    char feedWeight[24];
    uint32_t feedMs = 0;
    int feedUnit = 0;
    bool feedLive = serialWriterForwarderService->latestWeight(feedWeight, sizeof feedWeight, &feedMs, &feedUnit) &&
                    millis() - feedMs < 3000;
    if (serialWriterForwarderService->latestSaveStatus(&saveWaiting, &saveCount, &saveResult) && feedLive) {
      weighings_remote_status((WsoftSaveResult)saveWaiting);
      if (saveCountKnown && saveCount != saveCountSeen) weighings_remote_result((WsoftSaveResult)saveResult);
      saveCountKnown = true;
      saveCountSeen = saveCount;
    } else {
      weighings_remote_status(WSOFT_SAVE_NO_SIGNAL);
    }
  }
  if (wsoftStatusService) wsoftStatusService->loop();
#if FT_ENABLED(FT_WSOFT_RELAY)
  if (relayBoardService) relayBoardService->loop();
#endif
  // RS485, RS232 and the Writer share Serial1, so a new source or role starts cleanly after a restart.
  {
    static unsigned long sourceChangedMs = 0;
    if ((wsoftSettingsService && wsoftSettingsService->source() != wsoftBootSource) ||
        uartModeService->isWriter() != wsoftBootWriter) {
      if (sourceChangedMs == 0) {
        sourceChangedMs = millis();
        Serial.println(F("[WSoft] Source or role changed: restarting in 2 s"));
      } else if (millis() - sourceChangedMs > 2000UL) {
        ESP.restart();
      }
    }
  }
#endif

#if FT_ENABLED(FT_REMOTE_WEIGHT)
  if (remoteWeightService) remoteWeightService->loop();
#endif

#if FT_ENABLED(FT_TFT_WEIGHT_SCREEN)
  if (tftWeightDisplayService) tftWeightDisplayService->loop();
#endif

#if FT_ENABLED(FT_DISPLAY_LCD)
  if (lcdDisplayService) lcdDisplayService->loop();
#endif

  if (boardDisplayService) {
    boardDisplayService->loop();
  }

  // mDNS announcement (deferred start — runs only once the network is up)
  if (mdnsService) mdnsService->loop();

  // mDNS browser — 30s periodic scan, 90s entry TTL.
  if (mdnsBrowser) mdnsBrowser->loop();

  // App-level watchdog: cheap 30s health check, restarts chip if stuck >4min.
  if (watchdogService) watchdogService->loop();

#ifdef ESP32
  {
    static unsigned long heapLogMs = 0;
    const unsigned long now = millis();
    if (heapLogMs == 0) {
      heapLogMs = now;
    } else if ((unsigned long)(now - heapLogMs) >= 60000UL) {
      heapLogMs = now;
      if (!g_usbOutputActive) {
        Serial.printf("[Heap] free=%u min_free=%u max_alloc_heap=%u\n",
                      (unsigned)ESP.getFreeHeap(),
                      (unsigned)ESP.getMinFreeHeap(),
                      (unsigned)ESP.getMaxAllocHeap());
        logWifiSnapshot("WiFi");
      }
    }
  }
  // Let IDLE/Wi‑Fi stack run between heavy loop iterations (stability under load).
  yield();
#endif
}
