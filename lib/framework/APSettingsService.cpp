#include <APSettingsService.h>

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

  // "Really connected" check — stronger than just WiFi.status() because
  // ESP32-S3 has been observed reporting WL_CONNECTED while netif is dead
  // (no valid IP, no actual data path). Require both a valid status AND
  // a non-zero local IP so the AP fallback fires for the "stuck" case.
  bool wifiActive = (WiFi.status() == WL_CONNECTED)
#ifdef ESP32
                    && (WiFi.localIP() != IPAddress(0, 0, 0, 0))
#endif
      ;

  if (_state.provisionMode == AP_MODE_ALWAYS ||
      (_state.provisionMode == AP_MODE_DISCONNECTED && !wifiActive)) {
    if (_reconfigureAp || currentWiFiMode == WIFI_OFF || currentWiFiMode == WIFI_STA) {
      startAP();
    }
  } else if ((currentWiFiMode == WIFI_AP || currentWiFiMode == WIFI_AP_STA) &&
             (_reconfigureAp || !WiFi.softAPgetStationNum())) {
    stopAP();
  }
  _reconfigureAp = false;
}

void APSettingsService::startAP() {
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
