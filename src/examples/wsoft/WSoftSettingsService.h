#ifndef WSoftSettingsService_h
#define WSoftSettingsService_h

#include <Features.h>
#if FT_ENABLED(FT_WSOFT)

// W-Soft web options (Waveshare ESP32-S3 4.3 + DJB over RS485, env wsoft_waveshare43).
// Only options the W-Soft web services own: publish interval, the MQTT status topic and the
// Home Assistant discovery name/id (like the LED example).
// Approval and Auto save, the Hub pairing code, Wi-Fi and OTA are NOT here (W-Soft switches,
// the Hub tab, framework pages).
// Validation and defaults live in lib/wsoft_core (wsoft_settings), host-tested.

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <WebSocketTxRx.h>
#include <SettingValue.h>

#include <wsoft_settings.h>

#define WSOFT_SETTINGS_ENDPOINT_PATH "/rest/wsoftSettings"
#define WSOFT_SETTINGS_SOCKET_PATH "/ws/wsoftSettings"
#define WSOFT_SETTINGS_FILE "/config/wsoftSettings.json"

// Boards whose env wires the Serial reader's UART for an RS232 scale (the CYD).
#ifndef WSOFT_HAS_RS232
#define WSOFT_HAS_RS232 0
#endif

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
    root["source"] = wsoft_source_text(settings.data.source);
    root["relay_enabled"] = settings.data.relay_enabled;
    root["relay_low"] = settings.data.relay_low;
    root["relay_ok"] = settings.data.relay_ok;
    root["relay_high"] = settings.data.relay_high;
    root["relay_writer_follow"] = settings.data.relay_writer_follow;
  }

  // Stored fields plus the resolved status topic, for the settings page.
  static void read(WSoftSettings& settings, JsonObject& root) {
    readConfig(settings, root);
    root["mqtt_status_topic"] = statusTopic(settings);
    root["publish_interval_min_ms"] = WSOFT_PUBLISH_MIN_MS;
    root["publish_interval_max_ms"] = WSOFT_PUBLISH_MAX_MS;
    root["rs232_available"] = WSOFT_HAS_RS232 != 0;
    root["relay_available"] = FT_ENABLED(FT_WSOFT_RELAY) != 0;
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
    in.mqtt_base_path = root["mqtt_base_path"].as<const char*>();
    in.mqtt_name = root["mqtt_name"].as<const char*>();
    in.mqtt_unique_id = root["mqtt_unique_id"].as<const char*>();
    in.source = root["source"].as<const char*>();
#if !WSOFT_HAS_RS232
    // This board has no RS232 wiring: the source stays RS485 whatever is sent.
    if (in.source != nullptr && strcmp(in.source, "rs485") != 0) return StateUpdateResult::ERROR;
#endif
    if (root["relay_enabled"].is<bool>()) {
      in.has_relay_enabled = true;
      in.relay_enabled = root["relay_enabled"].as<bool>();
    }
    if (root["relay_writer_follow"].is<bool>()) {
      in.has_relay_writer_follow = true;
      in.relay_writer_follow = root["relay_writer_follow"].as<bool>();
    }
    // A relay number must be a whole number; anything else is refused, never rounded.
    if (!relayNumber(root["relay_low"], &in.has_relay_low, &in.relay_low) ||
        !relayNumber(root["relay_ok"], &in.has_relay_ok, &in.relay_ok) ||
        !relayNumber(root["relay_high"], &in.has_relay_high, &in.relay_high)) {
      return StateUpdateResult::ERROR;
    }
#if !FT_ENABLED(FT_WSOFT_RELAY)
    // This board has no relay outputs: the switch stays off whatever is sent.
    if (in.relay_enabled) return StateUpdateResult::ERROR;
#endif

    switch (wsoft_settings_update(in, &settings.data)) {
      case WSOFT_SETTINGS_CHANGED:
        return StateUpdateResult::CHANGED;
      case WSOFT_SETTINGS_ERROR:
        return StateUpdateResult::ERROR;
      default:
        return StateUpdateResult::UNCHANGED;
    }
  }

  // Missing or null = not sent (takes the default). A whole number = sent. Else refused.
  static bool relayNumber(JsonVariant value, bool* has, int64_t* out) {
    if (value.isNull()) return true;
    if (!value.is<long>()) return false;
    *has = true;
    *out = value.as<long>();
    return true;
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
  // RS485 (DJB) or RS232 (scale line). Read at boot; a change takes effect after a restart.
  WSoftSource source();
  // Empty when MQTT is switched off for W-Soft.
  String mqttStatusTopic();
  // Home Assistant discovery name and unique id, placeholders expanded.
  String mqttName();
  String mqttUniqueId();
  // Hi-Lo relay output: the switch and the relay (1..4, 0 = none) for each zone.
  void relayConfig(bool* enabled, uint8_t* low, uint8_t* ok, uint8_t* high);

 private:
  HttpEndpoint<WSoftSettings> _httpEndpoint;
  FSPersistence<WSoftSettings> _fsPersistence;
  WebSocketTxRx<WSoftSettings> _webSocket;
};

#endif  // FT_WSOFT
#endif
