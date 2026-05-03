#include "WatchdogService.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

// === Tunables =============================================================
//
// 4-minute grace period from boot. Even on a permanently-broken network this
// caps restarts at one every ~4 minutes — no death-loop.
static const unsigned long GRACE_MS          = 4UL * 60UL * 1000UL;

// Continuous unhealthy time before a restart fires. Resets to "now" the moment
// the device becomes healthy again, so transient flaps don't accumulate.
static const unsigned long UNHEALTHY_LIMIT   = 4UL * 60UL * 1000UL;

// Health check throttle. Must be << UNHEALTHY_LIMIT to give meaningful
// resolution. 30 s = 8 checks per UNHEALTHY_LIMIT window.
static const unsigned long CHECK_INTERVAL_MS = 30UL * 1000UL;

// Free-heap floor. Below this, we treat the device as unhealthy because new
// allocations (incoming WS frames, JSON requests) will start to fail.
static const uint32_t      LOW_HEAP_BYTES    = 30UL * 1024UL;

WatchdogService::WatchdogService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager)
    : _server(server), _fs(fs), _securityManager(securityManager) {}

void WatchdogService::begin() {
  _bootMs = millis();
  _lastHealthyMs = _bootMs;
  readPreviousReason();
  registerStatusEndpoint();

  // Subscribe the current task (Arduino's loopTask) to the IDF task watchdog.
  // The IDF WDT is already configured at CONFIG_ESP_TASK_WDT_TIMEOUT_S=10 in
  // platformio.ini for esp32s3, but by default only the IDLE tasks are
  // watched. By adding our task we get a hard auto-reset if the main loop
  // hangs for >10s — protecting against the kind of mDNS/blocking-call
  // hang that previously left the chip wedged with the network alive but
  // SerialService frozen and no way to OTA-recover.
  esp_err_t err = esp_task_wdt_add(NULL);
  if (err == ESP_OK) {
    Serial.println(F("[WD] loopTask subscribed to IDF task WDT (10s timeout)"));
  } else {
    Serial.printf("[WD] esp_task_wdt_add failed: 0x%x — main-loop hang protection inactive\n", err);
  }

  Serial.printf("[WD] WatchdogService started — grace=%lus, unhealthy_limit=%lus, check_interval=%lus, heap_floor=%uB\n",
                GRACE_MS / 1000UL,
                UNHEALTHY_LIMIT / 1000UL,
                CHECK_INTERVAL_MS / 1000UL,
                LOW_HEAP_BYTES);

  if (_previousReason.length() > 0) {
    Serial.printf("[WD] Previous restart cause: %s (uptime was %lu ms, heap was %u B)\n",
                  _previousReason.c_str(),
                  _previousUptimeMs,
                  _previousFreeHeap);
  }
}

void WatchdogService::loop() {
  // Feed the IDF task watchdog on every loop iteration. If the main loop
  // hangs (e.g. an mDNS query that never returns), this stops being called
  // and the IDF WDT panics the chip after CONFIG_ESP_TASK_WDT_TIMEOUT_S.
  esp_task_wdt_reset();

  unsigned long now = millis();

  // Throttle: only run the actual checks every CHECK_INTERVAL_MS, except on
  // the very first call so we get an immediate "[WD] healthy" on boot.
  if (_checkedOnce && (now - _lastCheckMs) < CHECK_INTERVAL_MS) return;
  _lastCheckMs = now;
  _checkedOnce = true;

  bool healthy = isHealthy();

  if (healthy) {
    _lastHealthyMs = now;
    Serial.printf("[WD] healthy (heap=%u, sta=%s, ap_clients=%u)\n",
                  ESP.getFreeHeap(),
                  WiFi.status() == WL_CONNECTED ? "up" : "down",
                  (unsigned)WiFi.softAPgetStationNum());
    return;
  }

  unsigned long sinceBoot    = now - _bootMs;
  unsigned long sinceHealthy = now - _lastHealthyMs;

  if (sinceBoot < GRACE_MS) {
    Serial.printf("[WD] unhealthy but in grace period (%lus left)\n",
                  (GRACE_MS - sinceBoot) / 1000UL);
    return;
  }

  if (sinceHealthy < UNHEALTHY_LIMIT) {
    Serial.printf("[WD] unhealthy for %lus (limit %lus)\n",
                  sinceHealthy / 1000UL,
                  UNHEALTHY_LIMIT / 1000UL);
    return;
  }

  // Pick the dominant cause for the persisted reason field.
  const char* reason = "unknown";
  if (ESP.getFreeHeap() < LOW_HEAP_BYTES) {
    reason = "low_heap";
  } else if (WiFi.status() != WL_CONNECTED && WiFi.softAPgetStationNum() == 0) {
    reason = "wifi_unhealthy_4min";
  }

  recordReasonAndRestart(reason);
}

bool WatchdogService::isHealthy() const {
  if (ESP.getFreeHeap() < LOW_HEAP_BYTES) return false;
  if (WiFi.status() == WL_CONNECTED) return true;
  // STA is down but if a client is paired with our SoftAP, the user can still
  // reach us at 192.168.4.1 to triage — that counts as healthy.
  if (WiFi.softAPgetStationNum() > 0) return true;
  return false;
}

void WatchdogService::recordReasonAndRestart(const char* reason) {
  Serial.printf("[WD] reason=%s — restarting in 1s\n", reason);

  StaticJsonDocument<256> doc;
  doc["reason"]      = reason;
  doc["uptime_ms"]   = millis();
  doc["free_heap"]   = ESP.getFreeHeap();
  doc["wifi_status"] = (int)WiFi.status();
  doc["ap_clients"]  = (unsigned)WiFi.softAPgetStationNum();

  File f = _fs->open(WATCHDOG_REASON_FILE, "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
    Serial.printf("[WD] reason persisted to %s\n", WATCHDOG_REASON_FILE);
  } else {
    Serial.printf("[WD] WARN: could not write %s — restart will still proceed\n", WATCHDOG_REASON_FILE);
  }

  // Tiny delay so the serial buffer flushes before the chip resets.
  delay(1000);
  esp_restart();
}

void WatchdogService::readPreviousReason() {
  if (!_fs) return;
  File f = _fs->open(WATCHDOG_REASON_FILE, "r");
  if (!f) return;

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, f);
  if (err == DeserializationError::Ok) {
    _previousReason   = doc["reason"]    | "";
    _previousUptimeMs = doc["uptime_ms"] | 0UL;
    _previousFreeHeap = doc["free_heap"] | 0U;
  }
  f.close();
}

void WatchdogService::registerStatusEndpoint() {
  _server->on(
      WATCHDOG_STATUS_PATH,
      HTTP_GET,
      _securityManager->wrapRequest(
          [this](AsyncWebServerRequest* request) {
            AsyncJsonResponse* response = new AsyncJsonResponse(false, 512);
            JsonObject root = response->getRoot();
            const unsigned long now = millis();
            root["uptime_ms"]              = now;
            root["last_healthy_ms_ago"]    = now - _lastHealthyMs;
            root["free_heap"]              = ESP.getFreeHeap();
            root["wifi_connected"]         = WiFi.status() == WL_CONNECTED;
            root["ap_clients"]             = (unsigned)WiFi.softAPgetStationNum();
            root["grace_remaining_ms"]     = (now < _bootMs + GRACE_MS) ? (_bootMs + GRACE_MS - now) : 0UL;
            root["last_restart_reason"]    = _previousReason;
            root["last_restart_uptime_ms"] = _previousUptimeMs;
            root["last_restart_free_heap"] = _previousFreeHeap;
            response->setLength();
            request->send(response);
          },
          AuthenticationPredicates::IS_AUTHENTICATED));
}
