#include <Features.h>
#if FT_ENABLED(FT_WSOFT)

#include <examples/wsoft/WSoftSettingsService.h>

// Settings are admin-only, read and write, like the framework's own settings pages.
WSoftSettingsService::WSoftSettingsService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager) :
    _httpEndpoint(WSoftSettings::read,
                  WSoftSettings::update,
                  this,
                  server,
                  WSOFT_SETTINGS_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_ADMIN),
    _fsPersistence(WSoftSettings::readConfig, WSoftSettings::update, this, fs, WSOFT_SETTINGS_FILE),
    _webSocket(WSoftSettings::read,
               WSoftSettings::update,
               this,
               server,
               WSOFT_SETTINGS_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_ADMIN) {
}

void WSoftSettingsService::begin() {
  _fsPersistence.readFromFS();
  Serial.printf("[WSoft] Settings loaded: publish=%lums mqtt=%s topic=%s\n",
                (unsigned long)publishIntervalMs(),
                _state.data.mqtt_enabled ? "on" : "off",
                WSoftSettings::statusTopic(_state).c_str());
}

uint32_t WSoftSettingsService::publishIntervalMs() {
  uint32_t ms = WSOFT_PUBLISH_DEFAULT_MS;
  read([&](const WSoftSettings& s) { ms = s.data.publish_interval_ms; });
  return ms;
}

String WSoftSettingsService::mqttStatusTopic() {
  String topic;
  read([&](const WSoftSettings& s) {
    if (s.data.mqtt_enabled) topic = WSoftSettings::statusTopic(s);
  });
  return topic;
}

#endif  // FT_WSOFT
