#include <WiFiSettingsService.h>

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
  // #region agent log
  Serial.printf(
      "{\"sessionId\":\"069613\",\"hypothesisId\":\"H1\",\"location\":\"WiFiSettingsService.cpp:begin\",\"message\":\"after_read_fs\",\"data\":{\"ssidLen\":%u,\"passLen\":%u,\"staticIp\":%s},\"timestamp\":%lu}\n",
      (unsigned)_state.ssid.length(), (unsigned)_state.password.length(), _state.staticIPConfig ? "true" : "false",
      (unsigned long)millis());
  // #endregion
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
  if (!_lastConnectionAttempt || (unsigned long)(currentMillis - _lastConnectionAttempt) >= _reconnectDelay) {
    _lastConnectionAttempt = currentMillis;
    manageSTA();
  }
}

void WiFiSettingsService::manageSTA() {
  if (WiFi.isConnected()) {
    _reconnectDelay = WIFI_RECONNECT_IMMEDIATE_DELAY;
    _reconnectAttempts = 0;
    return;
  }
  if (_state.ssid.length() == 0) {
    // #region agent log
    Serial.printf(
        "{\"sessionId\":\"069613\",\"hypothesisId\":\"H4\",\"location\":\"WiFiSettingsService.cpp:manageSTA\",\"message\":\"skip_no_ssid\",\"data\":{},\"timestamp\":%lu}\n",
        (unsigned long)millis());
    // #endregion
    return;
  }

  // Ensure STA is enabled. After the first WiFi.begin(), STA often stays "on" while disconnected;
  // the old `(WiFi.getMode() & WIFI_STA) == 0` gate then skipped all retries (TASK-display §2.0).
#if defined(ESP32)
  if ((WiFi.getMode() & WIFI_STA) == 0) {
    if ((WiFi.getMode() & WIFI_AP) != 0) {
      WiFi.mode(WIFI_AP_STA);
    } else {
      WiFi.mode(WIFI_STA);
    }
  }
#elif defined(ESP8266)
  if ((WiFi.getMode() & WIFI_STA) == 0) {
    WiFi.mode(((WiFi.getMode() & WIFI_AP) != 0) ? WIFI_AP_STA : WIFI_STA);
  }
#endif

  _reconnectAttempts++;
  if (_reconnectAttempts > 1) {
    _reconnectDelay = min((unsigned long)(WIFI_RECONNECT_IMMEDIATE_DELAY << (_reconnectAttempts - 1)),
                          (unsigned long)WIFI_RECONNECTION_DELAY);
  }
  Serial.printf("[WiFi] Connecting to WiFi (attempt %u, next retry in %lus).\n",
                _reconnectAttempts, _reconnectDelay / 1000);
  // #region agent log
  Serial.printf(
      "{\"sessionId\":\"069613\",\"hypothesisId\":\"H2\",\"location\":\"WiFiSettingsService.cpp:manageSTA\",\"message\":\"before_begin\",\"data\":{\"mode\":%u,\"wlStatus\":%d,\"attempt\":%u,\"ssidLen\":%u},\"timestamp\":%lu}\n",
      (unsigned)WiFi.getMode(), (int)WiFi.status(), (unsigned)_reconnectAttempts, (unsigned)_state.ssid.length(),
      (unsigned long)millis());
  // #endregion
  if (_state.staticIPConfig) {
    WiFi.config(_state.localIP, _state.gatewayIP, _state.subnetMask, _state.dnsIP1, _state.dnsIP2);
  } else {
#ifdef ESP32
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(_state.hostname.c_str());
#elif defined(ESP8266)
    WiFi.config(INADDR_ANY, INADDR_ANY, INADDR_ANY);
    WiFi.hostname(_state.hostname);
#endif
  }
  WiFi.begin(_state.ssid.c_str(), _state.password.c_str());
}

#ifdef ESP32
void WiFiSettingsService::onStationModeDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  // Do not call WiFi.disconnect(true) here — it runs during normal WPA association transitions and
  // aborts the join (TASK-display §2.0b). Schedule a retry via manageSTA() timing instead.
  Serial.printf("[WiFi] STA disconnected (reason=%d)\n", (int)info.wifi_sta_disconnected.reason);
  // #region agent log
  Serial.printf(
      "{\"sessionId\":\"069613\",\"hypothesisId\":\"H5\",\"location\":\"WiFiSettingsService.cpp:onStationModeDisconnected\",\"message\":\"sta_disc\",\"data\":{\"reason\":%d},\"timestamp\":%lu}\n",
      (int)info.wifi_sta_disconnected.reason, (unsigned long)millis());
  // #endregion
  _lastConnectionAttempt = 0;
}
void WiFiSettingsService::onStationModeStop(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (_stopping) {
    // User-initiated disconnect (reconfigureWiFiConnection) - reconnect immediately
    _stopping = false;
  } else {
    // Unintentional disconnect (signal loss, AP reboot etc) - reconnect immediately
    Serial.println(F("[WiFi] Unexpected disconnect - scheduling immediate reconnect."));
  }
  // Always trigger immediate reconnect attempt on next loop tick
  _lastConnectionAttempt = 0;
}
#elif defined(ESP8266)
void WiFiSettingsService::onStationModeDisconnected(const WiFiEventStationModeDisconnected& event) {
  Serial.printf("[WiFi] STA disconnected (reason=%d)\n", (int)event.reason);
  // #region agent log
  Serial.printf(
      "{\"sessionId\":\"069613\",\"hypothesisId\":\"H5\",\"location\":\"WiFiSettingsService.cpp:onStationModeDisconnected8266\",\"message\":\"sta_disc\",\"data\":{\"reason\":%d},\"timestamp\":%lu}\n",
      (int)event.reason, (unsigned long)millis());
  // #endregion
  _lastConnectionAttempt = 0;
}
#endif
