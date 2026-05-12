#include <WiFiSettingsService.h>
#include <WiFiScanner.h>  // for g_wifiScanInProgress / g_wifiScanInProgressUntilMs

#ifdef ESP32
#include <esp_wifi.h>
#endif

WiFiSettingsService::WiFiSettingsService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager) :
    _httpEndpoint(WiFiSettings::read, WiFiSettings::update, this, server, WIFI_SETTINGS_SERVICE_PATH, securityManager),
    _fsPersistence(WiFiSettings::read, WiFiSettings::update, this, fs, WIFI_SETTINGS_FILE),
    _lastConnectionAttempt(0),
    _reconnectDelay(WIFI_RECONNECT_IMMEDIATE_DELAY),
    _reconnectAttempts(0) {
  // We want the device to come up in opmode=0 (WIFI_OFF), when erasing the flash this is not the default.
  // If needed, we save opmode=0 before disabling persistence so the device boots with WiFi disabled in the future.
  if (WiFi.getMode() != WIFI_OFF) {
    WiFi.mode(WIFI_OFF);
  }

  // Disable WiFi config persistance and auto reconnect
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
#ifdef ESP32
  // Init the wifi driver on ESP32
  WiFi.mode(WIFI_MODE_MAX);
  WiFi.mode(WIFI_MODE_NULL);
  WiFi.onEvent(
      std::bind(&WiFiSettingsService::onStationModeDisconnected, this, std::placeholders::_1, std::placeholders::_2),
      WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(std::bind(&WiFiSettingsService::onStationModeStop, this, std::placeholders::_1, std::placeholders::_2),
               WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_STOP);
#elif defined(ESP8266)
  _onStationModeDisconnectedHandler = WiFi.onStationModeDisconnected(
      std::bind(&WiFiSettingsService::onStationModeDisconnected, this, std::placeholders::_1));
#endif

  addUpdateHandler([&](const String& originId) { reconfigureWiFiConnection(); }, false);
}

void WiFiSettingsService::begin() {
  _fsPersistence.readFromFS();
  reconfigureWiFiConnection();
}

void WiFiSettingsService::reconfigureWiFiConnection() {
  // reset last connection attempt to force loop to reconnect immediately
  _lastConnectionAttempt = 0;
  _reconnectDelay = WIFI_RECONNECT_IMMEDIATE_DELAY;
  _reconnectAttempts = 0;

// disconnect and de-configure wifi
#ifdef ESP32
  if (WiFi.disconnect(true)) {
    _stopping = true;
  }
#elif defined(ESP8266)
  WiFi.disconnect(true);
#endif
}

void WiFiSettingsService::loop() {
  unsigned long currentMillis = millis();

  // While a WiFi scan is in progress, avoid kicking off STA reconnect attempts — the
  // shared 2.4 GHz radio cannot serve a STA association attempt and a scan at the same
  // time, and doing so blanks the scan result and frequently resets the AP-served
  // captive-portal HTTP connection mid-response. The guard auto-expires after
  // WIFI_SCAN_GUARD_MS so a stuck scan can't permanently disable reconnects.
  if (g_wifiScanInProgress) {
    if (g_wifiScanInProgressUntilMs != 0 &&
        (long)(currentMillis - g_wifiScanInProgressUntilMs) >= 0) {
      g_wifiScanInProgress = false;
    } else {
      return;
    }
  }

  if (!_lastConnectionAttempt || (unsigned long)(currentMillis - _lastConnectionAttempt) >= _reconnectDelay) {
    _lastConnectionAttempt = currentMillis;
    manageSTA();
  }
}

void WiFiSettingsService::manageSTA() {
  if (WiFi.isConnected() || _state.ssid.length() == 0) {
    if (WiFi.isConnected()) {
      _reconnectDelay = WIFI_RECONNECT_IMMEDIATE_DELAY;
      _reconnectAttempts = 0;
    }
    return;
  }

  _reconnectAttempts++;
  if (_reconnectAttempts > 1) {
    _reconnectDelay = min((unsigned long)(WIFI_RECONNECT_IMMEDIATE_DELAY << (_reconnectAttempts - 1)),
                          (unsigned long)WIFI_RECONNECTION_DELAY);
  }
  Serial.printf("[WiFi] Connecting to WiFi \"%s\" (attempt %u, next retry in %lus).\n",
                _state.ssid.c_str(), _reconnectAttempts, _reconnectDelay / 1000);

#ifdef ESP32
  WiFi.setHostname(_state.hostname.c_str());
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
#endif
  if (_state.staticIPConfig) {
    WiFi.config(_state.localIP, _state.gatewayIP, _state.subnetMask, _state.dnsIP1, _state.dnsIP2);
  } else {
#ifdef ESP32
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
#elif defined(ESP8266)
    WiFi.config(INADDR_ANY, INADDR_ANY, INADDR_ANY);
    WiFi.hostname(_state.hostname);
#endif
  }
  WiFi.begin(_state.ssid.c_str(), _state.password.c_str());
}

#ifdef ESP32
void WiFiSettingsService::onStationModeDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (g_wifiScanInProgress) {
    return;
  }
  if (_stopping) {
    return;
  }
  WiFi.disconnect(true);
}
void WiFiSettingsService::onStationModeStop(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (_stopping) {
    // User-initiated disconnect (reconfigureWiFiConnection) - reconnect immediately
    _stopping = false;
  } else {
    // Unintentional disconnect (signal loss, AP reboot etc) - reconnect immediately
    Serial.println(F("[WiFi] Unexpected disconnect - scheduling immediate reconnect."));
  }
  // Always trigger immediate reconnect attempt on next loop tick, unless we are mid-scan.
  if (!g_wifiScanInProgress) {
    _lastConnectionAttempt = 0;
  }
}
#elif defined(ESP8266)
void WiFiSettingsService::onStationModeDisconnected(const WiFiEventStationModeDisconnected& event) {
  WiFi.disconnect(true);
}
#endif
