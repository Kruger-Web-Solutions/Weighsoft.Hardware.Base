#include <APSettingsService.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

APSettingsService::APSettingsService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager) :
    _httpEndpoint(APSettings::read, APSettings::update, this, server, AP_SETTINGS_SERVICE_PATH, securityManager),
    _fsPersistence(APSettings::read, APSettings::update, this, fs, AP_SETTINGS_FILE),
    _dnsServer(nullptr),
    _lastManaged(0),
    _reconfigureAp(false) {
  addUpdateHandler([&](const String& originId) { reconfigureAP(); }, false);
}

void APSettingsService::begin() {
  _fsPersistence.readFromFS();
  reconfigureAP();
}

void APSettingsService::reconfigureAP() {
  _lastManaged = millis() - MANAGE_NETWORK_DELAY;
  _reconfigureAp = true;
}

void APSettingsService::loop() {
  unsigned long currentMillis = millis();
  unsigned long manageElapsed = (unsigned long)(currentMillis - _lastManaged);
  if (manageElapsed >= MANAGE_NETWORK_DELAY) {
    _lastManaged = currentMillis;
    manageAP();
  }
  handleDNS();
}

void APSettingsService::manageAP() {
  WiFiMode_t currentWiFiMode = WiFi.getMode();
  if (_state.provisionMode == AP_MODE_ALWAYS ||
      (_state.provisionMode == AP_MODE_DISCONNECTED && WiFi.status() != WL_CONNECTED)) {
    // Bringing up softAP while STA is in IDLE/SCAN breaks WPA handshake on many routers (§2.0b).
    // Only defer when STA is enabled and a STA SSID is configured (`WiFi.begin` path); otherwise
    // WL_IDLE with no SSID is normal for provisioning and the soft AP must be allowed to start.
#if defined(ESP32) || defined(ESP8266)
    const wl_status_t st = WiFi.status();
    const bool staAssociationInProgress =
        ((WiFi.getMode() & WIFI_STA) != 0) && (WiFi.SSID().length() > 0) &&
        ((st == WL_IDLE_STATUS) || (st == WL_SCAN_COMPLETED));
#else
    const bool staAssociationInProgress = false;
#endif
    if (!staAssociationInProgress &&
        (_reconfigureAp || currentWiFiMode == WIFI_OFF || currentWiFiMode == WIFI_STA)) {
      startAP();
    }
  } else if ((currentWiFiMode == WIFI_AP || currentWiFiMode == WIFI_AP_STA) &&
             (_reconfigureAp || !WiFi.softAPgetStationNum())) {
    stopAP();
  }
  _reconfigureAp = false;
}

void APSettingsService::startAP() {
  // #region agent log
  Serial.printf(
      "{\"sessionId\":\"069613\",\"hypothesisId\":\"H3\",\"location\":\"APSettingsService.cpp:startAP\",\"message\":\"softap_up\",\"data\":{\"mode\":%u,\"wlStatus\":%d},\"timestamp\":%lu}\n",
      (unsigned)WiFi.getMode(), (int)WiFi.status(), (unsigned long)millis());
  // #endregion
  Serial.println(F("Starting software access point"));
  WiFi.softAPConfig(_state.localIP, _state.gatewayIP, _state.subnetMask);
  WiFi.softAP(_state.ssid.c_str(), _state.password.c_str(), _state.channel, _state.ssidHidden, _state.maxClients);
  if (!_dnsServer) {
    IPAddress apIp = WiFi.softAPIP();
    Serial.print(F("Starting captive portal on "));
    Serial.println(apIp);
    _dnsServer = new DNSServer;
    _dnsServer->start(DNS_PORT, "*", apIp);
  }
}

void APSettingsService::stopAP() {
  if (_dnsServer) {
    Serial.println(F("Stopping captive portal"));
    _dnsServer->stop();
    delete _dnsServer;
    _dnsServer = nullptr;
  }
  Serial.println(F("Stopping software access point"));
  WiFi.softAPdisconnect(true);
}

void APSettingsService::handleDNS() {
  if (_dnsServer) {
    _dnsServer->processNextRequest();
  }
}

APNetworkStatus APSettingsService::getAPNetworkStatus() {
  WiFiMode_t currentWiFiMode = WiFi.getMode();
  bool apActive = currentWiFiMode == WIFI_AP || currentWiFiMode == WIFI_AP_STA;
  if (apActive && _state.provisionMode != AP_MODE_ALWAYS && WiFi.status() == WL_CONNECTED) {
    return APNetworkStatus::LINGERING;
  }
  return apActive ? APNetworkStatus::ACTIVE : APNetworkStatus::INACTIVE;
}
