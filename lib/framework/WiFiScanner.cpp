#include <WiFiScanner.h>

#ifdef ESP32
#include <esp_wifi.h>
#endif

// Defined here so other framework services can include WiFiScanner.h and observe it.
volatile bool g_wifiScanInProgress = false;
unsigned long g_wifiScanInProgressUntilMs = 0;

// Hard cap on how long the "scan in progress" guard stays active before WiFiSettingsService
// is allowed to resume STA reconnect attempts. A typical scan with 400 ms/channel × ~13
// channels finishes in ~6 s; we add headroom so listNetworks() polls can pick up the result.
#ifndef WIFI_SCAN_GUARD_MS
#define WIFI_SCAN_GUARD_MS 15000
#endif

WiFiScanner::WiFiScanner(AsyncWebServer* server, SecurityManager* securityManager) {
  server->on(SCAN_NETWORKS_SERVICE_PATH,
             HTTP_GET,
             securityManager->wrapRequest(std::bind(&WiFiScanner::scanNetworks, this, std::placeholders::_1),
                                          AuthenticationPredicates::IS_ADMIN));
  server->on(LIST_NETWORKS_SERVICE_PATH,
             HTTP_GET,
             securityManager->wrapRequest(std::bind(&WiFiScanner::listNetworks, this, std::placeholders::_1),
                                          AuthenticationPredicates::IS_ADMIN));
};

void WiFiScanner::scanNetworks(AsyncWebServerRequest* request) {
  if (WiFi.scanComplete() != -1) {
    WiFi.scanDelete();
#ifdef ESP32
    // Ensure STA is available and stop any in-flight reconnect attempt so the scan radio
    // is not pre-empted. Switch to AP_STA when AP is up (captive portal case) or pure STA
    // otherwise; setSleep(false) and esp_wifi_set_ps(WIFI_PS_NONE) keep the radio responsive.
    wifi_mode_t prevMode = WiFi.getMode();
    bool apActive = (prevMode == WIFI_AP || prevMode == WIFI_AP_STA);
    WiFi.disconnect(false);  // stop any pending STA connect attempt but keep AP up
    WiFi.mode(apActive ? WIFI_AP_STA : WIFI_STA);
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
#endif
    // Set guard BEFORE starting the scan so WiFiSettingsService::manageSTA() in the next
    // loop tick already sees it and refrains from kicking off a new connect attempt.
    g_wifiScanInProgress = true;
    g_wifiScanInProgressUntilMs = millis() + WIFI_SCAN_GUARD_MS;
    Serial.println(F("[WiFi] Scan started (passive, 400ms/chan, STA reconnect paused)"));
    // Passive scan listens for AP beacons (every ~100 ms) rather than active probing;
    // it is more reliable in noisy / crowded RF environments and avoids transmit bursts
    // that conflict with serving the captive portal.
    WiFi.scanNetworks(true /*async*/, true /*show_hidden*/, true /*passive*/, 400 /*max_ms_per_chan*/);
  }
  request->send(202);
}

void WiFiScanner::listNetworks(AsyncWebServerRequest* request) {
  int numNetworks = WiFi.scanComplete();
  if (numNetworks > -1) {
    // Scan complete (success or zero-result). Lift the STA-reconnect guard so the
    // device can resume reconnect attempts immediately.
    g_wifiScanInProgress = false;
    AsyncJsonResponse* response = new AsyncJsonResponse(false, MAX_WIFI_SCANNER_SIZE);
    JsonObject root = response->getRoot();
    JsonArray networks = root.createNestedArray("networks");
    for (int i = 0; i < numNetworks; i++) {
      JsonObject network = networks.createNestedObject();
      network["rssi"] = WiFi.RSSI(i);
      network["ssid"] = WiFi.SSID(i);
      network["bssid"] = WiFi.BSSIDstr(i);
      network["channel"] = WiFi.channel(i);
#ifdef ESP32
      network["encryption_type"] = (uint8_t)WiFi.encryptionType(i);
#elif defined(ESP8266)
      network["encryption_type"] = convertEncryptionType(WiFi.encryptionType(i));
#endif
    }
    response->setLength();
    request->send(response);
  } else if (numNetworks == -1) {
    request->send(202);
  } else {
    scanNetworks(request);
  }
}

#ifdef ESP8266
/*
 * Convert encryption type to standard used by ESP32 rather than the translated form which the esp8266 libaries expose.
 *
 * This allows us to use a single set of mappings in the UI.
 */
uint8_t WiFiScanner::convertEncryptionType(uint8_t encryptionType) {
  switch (encryptionType) {
    case ENC_TYPE_NONE:
      return AUTH_OPEN;
    case ENC_TYPE_WEP:
      return AUTH_WEP;
    case ENC_TYPE_TKIP:
      return AUTH_WPA_PSK;
    case ENC_TYPE_CCMP:
      return AUTH_WPA2_PSK;
    case ENC_TYPE_AUTO:
      return AUTH_WPA_WPA2_PSK;
  }
  return -1;
}
#endif
