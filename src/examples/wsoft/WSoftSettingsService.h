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

// Boards whose env names two pins for a load cell on an HX711 board (the CYD, on CN1).
#ifndef WSOFT_HAS_LOADCELL
#define WSOFT_HAS_LOADCELL 0
#endif

// Boards whose env names the pin of an on-board RGB LED (WS2812) for the "ESP LED" switch.
#ifdef WSOFT_RGB_LED_PIN
#define WSOFT_HAS_ESP_LED 1
#else
#define WSOFT_HAS_ESP_LED 0
#endif

// Boards whose own light sits on the Hi-Lo relay pins (the yellow board's LED1): the "Hi-Lo
// light" switch (relay_light) shows the zone on it. Needs the relay outputs.
#if defined(WSOFT_RELAY_LIGHT) && FT_ENABLED(FT_WSOFT_RELAY)
#define WSOFT_HAS_RELAY_LIGHT 1
#else
#define WSOFT_HAS_RELAY_LIGHT 0
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
    root["alarm_test_save"] = settings.data.alarm_test_save;
    root["esp_led"] = settings.data.esp_led;
    root["relay_light"] = settings.data.relay_light;
    JsonObject packSet = root.createNestedObject("pack_set");
    packSet["enabled"] = settings.data.pack_set.enabled;
    packSet["tolerance_g"] = settings.data.pack_set.tolerance_g;
    JsonArray targets = packSet.createNestedArray("targets_g");
    for (uint8_t i = 0; i < settings.data.pack_set.count && i < WSOFT_PACK_SET_MAX; i++) {
      targets.add(settings.data.pack_set.targets_g[i]);
    }
  }

  // Stored fields plus the resolved status topic, for the settings page.
  static void read(WSoftSettings& settings, JsonObject& root) {
    readConfig(settings, root);
    root["mqtt_status_topic"] = statusTopic(settings);
    root["publish_interval_min_ms"] = WSOFT_PUBLISH_MIN_MS;
    root["publish_interval_max_ms"] = WSOFT_PUBLISH_MAX_MS;
    root["pack_set_max"] = WSOFT_PACK_SET_MAX;
    root["rs232_available"] = WSOFT_HAS_RS232 != 0;
    root["loadcell_available"] = WSOFT_HAS_LOADCELL != 0;
    root["relay_available"] = FT_ENABLED(FT_WSOFT_RELAY) != 0;
    root["esp_led_available"] = WSOFT_HAS_ESP_LED != 0;
    root["relay_light_available"] = WSOFT_HAS_RELAY_LIGHT != 0;
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
    // A source this board is not wired for is refused, never silently changed.
    if (in.source != nullptr && strcmp(in.source, "rs232") == 0 && !WSOFT_HAS_RS232) return StateUpdateResult::ERROR;
    if (in.source != nullptr && strcmp(in.source, "loadcell") == 0 && !WSOFT_HAS_LOADCELL) return StateUpdateResult::ERROR;
    if (root["relay_enabled"].is<bool>()) {
      in.has_relay_enabled = true;
      in.relay_enabled = root["relay_enabled"].as<bool>();
    }
    if (root["alarm_test_save"].is<bool>()) {
      in.has_alarm_test_save = true;
      in.alarm_test_save = root["alarm_test_save"].as<bool>();
    }
    if (root["esp_led"].is<bool>()) {
      in.has_esp_led = true;
      in.esp_led = root["esp_led"].as<bool>();
    }
    if (root["relay_light"].is<bool>()) {
      in.has_relay_light = true;
      in.relay_light = root["relay_light"].as<bool>();
    }
    // This board's light is not on the relay pins: the switch stays off whatever is sent.
    if (in.relay_light && !WSOFT_HAS_RELAY_LIGHT) return StateUpdateResult::ERROR;
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
    JsonVariant packSetVar = root["pack_set"];
    if (!packSetVar.isNull()) {
      if (!packSetVar.is<JsonObject>()) return StateUpdateResult::ERROR;
      JsonObject packSet = packSetVar.as<JsonObject>();
      if (packSet.containsKey("enabled")) {
        if (!packSet["enabled"].is<bool>()) return StateUpdateResult::ERROR;
        in.has_pack_set_enabled = true;
        in.pack_set_enabled = packSet["enabled"].as<bool>();
      }
      if (!relayNumber(packSet["tolerance_g"], &in.has_pack_set_tolerance_g, &in.pack_set_tolerance_g)) {
        return StateUpdateResult::ERROR;
      }
      JsonVariant targetsVar = packSet["targets_g"];
      if (!targetsVar.isNull()) {
        if (!targetsVar.is<JsonArray>()) return StateUpdateResult::ERROR;
        JsonArray targets = targetsVar.as<JsonArray>();
        if (targets.size() > WSOFT_PACK_SET_MAX) return StateUpdateResult::ERROR;
        in.has_pack_set_targets_g = true;
        in.pack_set_count = 0;
        for (JsonVariant item : targets) {
          if (!item.is<long>()) return StateUpdateResult::ERROR;
          in.pack_set_targets_g[in.pack_set_count++] = item.as<long>();
        }
      }
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
  // RS485 (DJB), RS232 (scale line) or a load cell (HX711). Read at boot; a change takes effect after a restart.
  WSoftSource source();
  // Empty when MQTT is switched off for W-Soft.
  String mqttStatusTopic();
  // Home Assistant discovery name and unique id, placeholders expanded.
  String mqttName();
  String mqttUniqueId();
  // Hi-Lo relay output: the switch and the relay (1..4, 0 = none) for each zone.
  // In Writer mode the output is on only when "relays follow the weigh board" is on too.
  // The Hi-Lo light switch (yellow board) drives the same outputs by itself; *light says so.
  void relayConfig(bool* enabled, uint8_t* low, uint8_t* ok, uint8_t* high, bool* light = nullptr);
  // The "ESP LED" switch (the board's RGB LED shows the Hi-Lo zone).
  bool espLed();
  // Set once at boot from the board's role (UartModeService): true = Writer mode.
  void setWriterMode(bool writer) { _writerMode = writer; }
  bool writerMode() const { return _writerMode; }

 private:
  // Hands the alarm test switch and pack-set config to the weighing rules.
  void pushWeighingSettings();
  bool _writerMode = false;
  HttpEndpoint<WSoftSettings> _httpEndpoint;
  FSPersistence<WSoftSettings> _fsPersistence;
  WebSocketTxRx<WSoftSettings> _webSocket;
};

#endif  // FT_WSOFT
#endif
