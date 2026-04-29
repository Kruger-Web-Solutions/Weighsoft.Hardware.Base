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

  // Disable WiFi config persistence (we manage it via FSPersistence).
  WiFi.persistent(false);

  // Enable the SDK's built-in auto-reconnect. This makes the WiFi driver
  // silently re-associate on transient drops (signal dips, brief AP
  // reboots, beacon misses) without bouncing through the
  // disconnect → STA_STOP → manual reconnect cycle that the application-
  // level loop runs. The application-level reconnect remains as a
  // backstop for cases where the SDK's silent retry can't recover (e.g.
  // SSID disappears entirely, password changed at the AP, etc.) but the
  // happy path now is "stay associated through transient noise" instead
  // of "tear down and rebuild the netif on every glitch".
  WiFi.setAutoReconnect(true);
#ifdef ESP32
  // Init the wifi driver on ESP32. The MAX → NULL transition forces the
  // framework to bring up esp_netif + the default event handlers, then
  // immediately disable the radio so we control connection lifecycle
  // ourselves below.
  //
  // On boards with OPI PSRAM (e.g. Sixspan ESP32-S3 N16R8) PSRAM init
  // delays the boot sequence enough that, without a settle window between
  // the two mode flips, the second WiFi.mode() returns before esp_netif
  // has finished registering its netstack callback. The subsequent
  // WiFi.onEvent() calls then race against a not-yet-ready event system,
  // and esp_wifi_init() prints
  //     E (1429) wifi_init_default: netstack cb reg failed with 12289
  // (12289 == ESP_ERR_NETIF_INVALID_PARAMS). After that the chip stays
  // alive but the WiFi stack is permanently broken until reboot.
  // The 100 ms delay below gives netif/event-loop init time to complete.
  WiFi.mode(WIFI_MODE_MAX);
  delay(100);
  WiFi.mode(WIFI_MODE_NULL);
  delay(50);
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
  if (!_lastConnectionAttempt || (unsigned long)(currentMillis - _lastConnectionAttempt) >= _reconnectDelay) {
    _lastConnectionAttempt = currentMillis;
    manageSTA();
  }
}

void WiFiSettingsService::manageSTA() {
  // Abort if already connected, or if we have no SSID
  if (WiFi.isConnected() || _state.ssid.length() == 0) {
    if (WiFi.isConnected()) {
      // Connected successfully - reset backoff
      _reconnectDelay = WIFI_RECONNECT_IMMEDIATE_DELAY;
      _reconnectAttempts = 0;
    }
    return;
  }
  // Connect or reconnect as required
  if ((WiFi.getMode() & WIFI_STA) == 0) {
    _reconnectAttempts++;
    // Exponential backoff: 2s → 4s → 8s → 16s → 30s cap
    if (_reconnectAttempts > 1) {
      _reconnectDelay = min((unsigned long)(WIFI_RECONNECT_IMMEDIATE_DELAY << (_reconnectAttempts - 1)),
                            (unsigned long)WIFI_RECONNECTION_DELAY);
    }
    Serial.printf("[WiFi] Connecting to WiFi (attempt %u, next retry in %lus).\n",
                  _reconnectAttempts, _reconnectDelay / 1000);
    if (_state.staticIPConfig) {
      // configure for static IP
      WiFi.config(_state.localIP, _state.gatewayIP, _state.subnetMask, _state.dnsIP1, _state.dnsIP2);
    } else {
      // configure for DHCP
#ifdef ESP32
      WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
      WiFi.setHostname(_state.hostname.c_str());
#elif defined(ESP8266)
      WiFi.config(INADDR_ANY, INADDR_ANY, INADDR_ANY);
      WiFi.hostname(_state.hostname);
#endif
    }
    // attempt to connect to the network
    WiFi.begin(_state.ssid.c_str(), _state.password.c_str());

    // Reliability tuning applied right after WiFi.begin() (calls are
    // idempotent — safe to repeat on every retry):
    //   1. Disable WiFi modem sleep. With sleep enabled the radio
    //      periodically powers down between beacons, which on noisy
    //      networks causes the chip to miss enough beacons to be
    //      kicked off the AP. The cost is a few mA of extra current
    //      draw — negligible for a USB-powered weigh-bridge ESP that
    //      is always plugged in.
    //   2. Set TX power to the chip max (19.5 dBm). Default is
    //      board-dependent and sometimes lower; pinning to max gives
    //      the best reach to the AP.
    //
    // ESP32-S3 only has a 2.4 GHz radio (no 5 GHz support in silicon),
    // so band selection is not a setting we control here — the chip
    // physically cannot associate to a 5 GHz BSSID. If the configured
    // SSID is broadcast on both bands the chip will pick whichever
    // 2.4 GHz BSSID is strongest; a 5 GHz-only SSID will simply never
    // be visible to the chip.
    WiFi.setSleep(false);
#ifdef ESP32
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
#endif
  }
}

#ifdef ESP32
void WiFiSettingsService::onStationModeDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  // The disconnect reason code helps diagnose flapping connections —
  // 200/201/202 are auth/assoc timeouts (signal too weak, AP rejected),
  // 8 is the AP kicking us off (admin disconnect / max client limit),
  // 4/15 are the AP losing track of us (typically signal-related),
  // 39 is "expired" (we've been idle too long for the AP's NAT table).
  // We log without tearing down the connection — with WiFi.setAutoReconnect(true)
  // applied in the constructor, the SDK silently re-associates for transient
  // drops. We only fall through to the manual reconnect path when the SDK's
  // silent retry has been failing for a while (handled in manageSTA()'s
  // exponential backoff loop).
  uint8_t reason = info.wifi_sta_disconnected.reason;
  Serial.printf("[WiFi] Disconnect event reason=%u — letting SDK auto-reconnect retry silently\n", reason);

  // Bias the manual reconnect loop towards trying again soon if the
  // SDK retry doesn't recover within its window. We don't call
  // WiFi.disconnect(true) here any more — that was forcing a full
  // STA-STOP on every transient glitch and adding 5–15 s of downtime.
  _lastConnectionAttempt = 0;
}
void WiFiSettingsService::onStationModeStop(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (_stopping) {
    // User-initiated disconnect (reconfigureWiFiConnection) — reconnect immediately
    _stopping = false;
  } else {
    Serial.println(F("[WiFi] STA stopped unexpectedly — scheduling reconnect."));
  }
  _lastConnectionAttempt = 0;
}
#elif defined(ESP8266)
void WiFiSettingsService::onStationModeDisconnected(const WiFiEventStationModeDisconnected& event) {
  WiFi.disconnect(true);
}
#endif
