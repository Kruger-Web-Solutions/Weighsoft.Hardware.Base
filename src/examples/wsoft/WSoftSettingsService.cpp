#include <Features.h>
#if FT_ENABLED(FT_WSOFT)

#include <examples/wsoft/WSoftSettingsService.h>

#include <weighings.h>

// Settings are admin-only, read and write, like the framework's own settings pages.
WSoftSettingsService::WSoftSettingsService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager) :
    _httpEndpoint(WSoftSettings::read,
                  WSoftSettings::update,
                  this,
                  server,
                  WSOFT_SETTINGS_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_ADMIN,
                  2048),
    _fsPersistence(WSoftSettings::readConfig, WSoftSettings::update, this, fs, WSOFT_SETTINGS_FILE, 2048),
    _webSocket(WSoftSettings::read,
               WSoftSettings::update,
               this,
               server,
               WSOFT_SETTINGS_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_ADMIN,
               2048) {
}

void WSoftSettingsService::begin() {
  _fsPersistence.readFromFS();
  // Hand alarm test switch and pack-set config to the weighing rules now and after every change.
  pushWeighingSettings();
  addUpdateHandler([this](const String&) { pushWeighingSettings(); }, false);
  Serial.printf("[WSoft] Settings loaded: source=%s publish=%lums mqtt=%s topic=%s\n",
                wsoft_source_text(source()),
                (unsigned long)publishIntervalMs(),
                _state.data.mqtt_enabled ? "on" : "off",
                WSoftSettings::statusTopic(_state).c_str());
}

WSoftSource WSoftSettingsService::source() {
  WSoftSource source = WSOFT_SOURCE_RS485;
  read([&](const WSoftSettings& s) { source = s.data.source; });
  if (source == WSOFT_SOURCE_RS232 && !WSOFT_HAS_RS232) return WSOFT_SOURCE_RS485;
  if (source == WSOFT_SOURCE_LOADCELL && !WSOFT_HAS_LOADCELL) return WSOFT_SOURCE_RS485;
  return source;
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

String WSoftSettingsService::mqttName() {
  String name;
  read([&](const WSoftSettings& s) { name = SettingValue::format(String(s.data.mqtt_name)); });
  return name;
}

String WSoftSettingsService::mqttUniqueId() {
  String uid;
  read([&](const WSoftSettings& s) { uid = SettingValue::format(String(s.data.mqtt_unique_id)); });
  return uid;
}

void WSoftSettingsService::pushWeighingSettings() {
  bool on = false;
  WsoftPackSet packSet;
  read([&](const WSoftSettings& s) {
    on = s.data.alarm_test_save;
    packSet = s.data.pack_set;
  });
  weighings_set_alarm_test(on);
  weighings_set_pack_set(packSet);
}

void WSoftSettingsService::relayConfig(bool* enabled, uint8_t* low, uint8_t* ok, uint8_t* high, bool* light) {
  read([&](const WSoftSettings& s) {
    bool lightOn = WSOFT_HAS_RELAY_LIGHT && s.data.relay_light;
    *enabled = FT_ENABLED(FT_WSOFT_RELAY) &&
               wsoft_relay_drive(s.data.relay_enabled, _writerMode, s.data.relay_writer_follow, lightOn);
    if (light) *light = lightOn;
    *low = s.data.relay_low;
    *ok = s.data.relay_ok;
    *high = s.data.relay_high;
  });
}

bool WSoftSettingsService::espLed() {
  bool on = false;
  read([&](const WSoftSettings& s) { on = s.data.esp_led; });
  return on;
}

#endif  // FT_WSOFT
