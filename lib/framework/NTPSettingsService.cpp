#include <NTPSettingsService.h>

#if defined(ESP32) && defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3 && \
    defined(CONFIG_LWIP_TCPIP_CORE_LOCKING)
#include <esp_netif.h>
#include <lwip/priv/tcpip_priv.h>
#define NTP_NEEDS_CORE_LOCK 1
#endif

// Core 3 asserts (and reboots) when sntp_stop() runs without the lwIP core lock.
// Same locking pattern as the core's own configTime() in esp32-hal-time.c.
static void stopSntp() {
#ifdef NTP_NEEDS_CORE_LOCK
  esp_netif_init();
  bool locked = false;
  if (!sys_thread_tcpip(LWIP_CORE_LOCK_QUERY_HOLDER)) {
    LOCK_TCPIP_CORE();
    locked = true;
  }
  sntp_stop();
  if (locked) {
    UNLOCK_TCPIP_CORE();
  }
#else
  sntp_stop();
#endif
}

NTPSettingsService::NTPSettingsService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager) :
    _httpEndpoint(NTPSettings::read, NTPSettings::update, this, server, NTP_SETTINGS_SERVICE_PATH, securityManager),
    _fsPersistence(NTPSettings::read, NTPSettings::update, this, fs, NTP_SETTINGS_FILE),
    _timeHandler(TIME_PATH,
                 securityManager->wrapCallback(
                     std::bind(&NTPSettingsService::configureTime, this, std::placeholders::_1, std::placeholders::_2),
                     AuthenticationPredicates::IS_ADMIN)) {
  _timeHandler.setMethod(HTTP_POST);
  _timeHandler.setMaxContentLength(MAX_TIME_SIZE);
  server->addHandler(&_timeHandler);
#ifdef ESP32
  WiFi.onEvent(
      std::bind(&NTPSettingsService::onStationModeDisconnected, this, std::placeholders::_1, std::placeholders::_2),
      WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(std::bind(&NTPSettingsService::onStationModeGotIP, this, std::placeholders::_1, std::placeholders::_2),
               WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
#elif defined(ESP8266)
  _onStationModeDisconnectedHandler = WiFi.onStationModeDisconnected(
      std::bind(&NTPSettingsService::onStationModeDisconnected, this, std::placeholders::_1));
  _onStationModeGotIPHandler =
      WiFi.onStationModeGotIP(std::bind(&NTPSettingsService::onStationModeGotIP, this, std::placeholders::_1));
#endif
  addUpdateHandler([&](const String& originId) { configureNTP(); }, false);
}

void NTPSettingsService::begin() {
  _fsPersistence.readFromFS();
  configureNTP();
}

#ifdef ESP32
void NTPSettingsService::onStationModeGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println(F("Got IP address, starting NTP Synchronization"));
  configureNTP();
}

void NTPSettingsService::onStationModeDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println(F("WiFi connection dropped, stopping NTP."));
  configureNTP();
}
#elif defined(ESP8266)
void NTPSettingsService::onStationModeGotIP(const WiFiEventStationModeGotIP& event) {
  Serial.println(F("Got IP address, starting NTP Synchronization"));
  configureNTP();
}

void NTPSettingsService::onStationModeDisconnected(const WiFiEventStationModeDisconnected& event) {
  Serial.println(F("WiFi connection dropped, stopping NTP."));
  configureNTP();
}
#endif

void NTPSettingsService::configureNTP() {
  if (WiFi.isConnected() && _state.enabled) {
    Serial.println(F("Starting NTP..."));
#ifdef ESP32
    configTzTime(_state.tzFormat.c_str(), _state.server.c_str());
#elif defined(ESP8266)
    configTime(_state.tzFormat.c_str(), _state.server.c_str());
#endif
  } else {
#ifdef ESP32
    setenv("TZ", _state.tzFormat.c_str(), 1);
    tzset();
#elif defined(ESP8266)
    setTZ(_state.tzFormat.c_str());
#endif
    stopSntp();
  }
}

void NTPSettingsService::configureTime(AsyncWebServerRequest* request, JsonVariant& json) {
  if (!sntp_enabled() && json.is<JsonObject>()) {
    struct tm tm = {0};
    String timeLocal = json["local_time"];
    char* s = strptime(timeLocal.c_str(), "%Y-%m-%dT%H:%M:%S", &tm);
    if (s != nullptr) {
      time_t time = mktime(&tm);
      struct timeval now = {.tv_sec = time};
      settimeofday(&now, nullptr);
      AsyncWebServerResponse* response = request->beginResponse(200);
      request->send(response);
      return;
    }
  }
  AsyncWebServerResponse* response = request->beginResponse(400);
  request->send(response);
}
