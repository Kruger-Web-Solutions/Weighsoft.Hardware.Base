#ifndef WSoftSettingsService_h
#define WSoftSettingsService_h

#include <Features.h>
#if FT_ENABLED(FT_WSOFT)

// W-Soft web options (Waveshare ESP32-S3 4.3 + DJB over RS485, env wsoft_waveshare43).
// Only options the W-Soft web services own: publish interval, the MQTT status topic and the
// Home Assistant discovery name/id (like the LED example), and auto-save (on/off and how many
// seconds a load must be still), which this service hands to save_queue (lib/wsoft_esp).
// Approval (USB only), the Hub pairing code (Hub tab, WSoftHubService), Wi-Fi and OTA
// (framework pages) are NOT here.
// Validation and defaults live in lib/wsoft_core (wsoft_settings), host-tested.

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <WebSocketTxRx.h>
#include <SettingValue.h>

#include <wsoft_settings.h>

#define WSOFT_SETTINGS_ENDPOINT_PATH "/rest/wsoftSettings"
#define WSOFT_SETTINGS_SOCKET_PATH "/ws/wsoftSettings"
#define WSOFT_SETTINGS_FILE "/config/wsoftSettings.json"

class WSoftSettings {
 public:
  WSoftSettingsData data;

  WSoftSettings() { wsoft_settings_defaults(&data); }

  // Stored fields only (FSPersistence).
  static void readConfig(WSoftSettings& settings, JsonObject& root) {
    root["publish_interval_ms"] = settings.data.publish_interval_ms;
    root["mqtt_enabled"] = settings.data.mqtt_enabled;
    root["mqtt_base_path"] = settings.data.mqtt_base_path;
    root["mqtt_name"] = settings.data.mqtt_name;
    root["mqtt_unique_id"] = settings.data.mqtt_unique_id;
    root["auto_save"] = settings.data.auto_save;
    root["auto_save_stable_s"] = settings.data.auto_save_stable_s;
    root["save_ignore_alarm"] = settings.data.save_ignore_alarm;
  }

  // Stored fields plus the resolved status topic, for the settings page.
  static void read(WSoftSettings& settings, JsonObject& root) {
    readConfig(settings, root);
    root["mqtt_status_topic"] = statusTopic(settings);
    root["publish_interval_min_ms"] = WSOFT_PUBLISH_MIN_MS;
    root["publish_interval_max_ms"] = WSOFT_PUBLISH_MAX_MS;
    root["auto_save_stable_min_s"] = WSOFT_AUTO_SAVE_MIN_S;
    root["auto_save_stable_max_s"] = WSOFT_AUTO_SAVE_MAX_S;
  }

  static StateUpdateResult update(JsonObject& root, WSoftSettings& settings) {
    WSoftSettingsInput in;
    JsonVariant interval = root["publish_interval_ms"];
    if (interval.is<long>()) {
      in.has_publish_interval_ms = true;
      in.publish_interval_ms = interval.as<long>();
    } else if (interval.is<double>()) {
      in.has_publish_interval_ms = true;
      double ms = interval.as<double>();
      in.publish_interval_ms = ms > (double)WSOFT_PUBLISH_MAX_MS ? (int64_t)WSOFT_PUBLISH_MAX_MS : (int64_t)ms;
    }
    if (root["mqtt_enabled"].is<bool>()) {
      in.has_mqtt_enabled = true;
      in.mqtt_enabled = root["mqtt_enabled"].as<bool>();
    }
    if (root["save_ignore_alarm"].is<bool>()) {
      in.has_save_ignore_alarm = true;
      in.save_ignore_alarm = root["save_ignore_alarm"].as<bool>();
    }
    if (root["auto_save"].is<bool>()) {
      in.has_auto_save = true;
      in.auto_save = root["auto_save"].as<bool>();
    }
    JsonVariant stable = root["auto_save_stable_s"];
    if (stable.is<long>()) {
      in.has_auto_save_stable_s = true;
      in.auto_save_stable_s = stable.as<long>();
    } else if (stable.is<double>()) {
      in.has_auto_save_stable_s = true;
      double seconds = stable.as<double>();
      in.auto_save_stable_s = seconds > (double)WSOFT_AUTO_SAVE_MAX_S ? (int64_t)WSOFT_AUTO_SAVE_MAX_S : (int64_t)seconds;
    }
    in.mqtt_base_path = root["mqtt_base_path"].as<const char*>();
    in.mqtt_name = root["mqtt_name"].as<const char*>();
    in.mqtt_unique_id = root["mqtt_unique_id"].as<const char*>();

    switch (wsoft_settings_update(in, &settings.data)) {
      case WSOFT_SETTINGS_CHANGED:
        return StateUpdateResult::CHANGED;
      case WSOFT_SETTINGS_ERROR:
        return StateUpdateResult::ERROR;
      default:
        return StateUpdateResult::UNCHANGED;
    }
  }

  // Placeholders such as #{unique_id} expanded.
  static String statusTopic(const WSoftSettings& settings) {
    return SettingValue::format(String(settings.data.mqtt_base_path)) + "/status";
  }
};

class WSoftSettingsService : public StatefulService<WSoftSettings> {
 public:
  WSoftSettingsService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager);
  void begin();

  uint32_t publishIntervalMs();
  // Empty when MQTT is switched off for W-Soft.
  String mqttStatusTopic();
  // Home Assistant discovery name and unique id, placeholders expanded.
  String mqttName();
  String mqttUniqueId();
  // Hands auto-save on/off and the stable time to the save rules (save_queue).
  void applyAutoSave();

 private:
  HttpEndpoint<WSoftSettings> _httpEndpoint;
  FSPersistence<WSoftSettings> _fsPersistence;
  WebSocketTxRx<WSoftSettings> _webSocket;
};

#endif  // FT_WSOFT
#endif
