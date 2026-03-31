#ifndef GrowboxService_h
#define GrowboxService_h

#include <Arduino.h>
#include <Wire.h>
#include <HttpEndpoint.h>
#include <WebSocketTxRx.h>
#include <StatefulService.h>

#include <SHT2x.h>
#include <DHT.h>
#include <MqttPubSub.h>
#include <SettingValue.h>
#include <Features.h>

#if FT_ENABLED(FT_BLE)
#include <BlePubSub.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEDevice.h>
#endif

#include <examples/growbox/GrowboxSettingsService.h>
#include <examples/growbox/GrowboxAutomationService.h>

// BLE UUIDs — unique to the Growbox integration
#define GROWBOX_BLE_SERVICE_UUID "4b6e5d7f-bf21-4e9a-9c87-6e5d1042a3b1"
#define GROWBOX_BLE_CONTROLS_CHAR_UUID "a3b4c5d6-bf21-4e9a-9c87-6e5d1042a3b2"
#define GROWBOX_BLE_SENSORS_CHAR_UUID "b5c6d7e8-bf21-4e9a-9c87-6e5d1042a3b3"
#define GROWBOX_BLE_AUTOMATION_CHAR_UUID "c7d8e9f0-bf21-4e9a-9c87-6e5d1042a3b4"

#define GROWBOX_ENDPOINT_PATH "/rest/growbox"
#define GROWBOX_SOCKET_PATH "/ws/growbox"

// Use a larger buffer because GrowboxState has many fields
#define GROWBOX_STATE_BUFFER_SIZE 2048

class GrowboxState {
 public:
  // ── Relay outputs ──────────────────────────────────────────────────────────
  bool lightsOn;   // master: controls both light relays together
  bool lightAOn;   // derived from lightsOn (or individual test)
  bool lightBOn;   // derived from lightsOn (or individual test)
  bool fanInOn;
  bool fanOutOn;

  // ── Operating modes ───────────────────────────────────────────────────────
  // "manual" | "schedule" | "auto"
  String lightsMode;
  String fanInMode;
  String fanOutMode;

  // ── Sensors (populated in Phase 2) ────────────────────────────────────────
  float insideTemperature;
  float insideHumidity;
  float outsideTemperature;
  float outsideHumidity;
  int moistureRaw;
  float moisturePercent;
  bool moistureCalibrated;

  // ── Alarms (activated in Phase 3) ─────────────────────────────────────────
  bool moistureAlarmActive;
  bool moistureAlarmAcknowledged;

  // ── Sensor health (populated in Phase 2) ──────────────────────────────────
  bool insideSensorFault;
  bool outsideSensorFault;
  bool moistureSensorFault;

  // ── Human-readable reason strings (populated in Phase 3) ──────────────────
  String lightsReason;
  String fanInReason;
  String fanOutReason;
  String moistureAlarmReason;

  // ── Diagnostics ───────────────────────────────────────────────────────────
  bool scheduleActive;
  bool manualOverrideActive;
  unsigned long uptimeMs;
  uint32_t bootCount;
  bool ntpSynced;
  bool wifiConnected;

  static void read(GrowboxState& state, JsonObject& root) {
    root["lights_on"] = state.lightsOn;
    root["light_a_on"] = state.lightAOn;
    root["light_b_on"] = state.lightBOn;
    root["fan_in_on"] = state.fanInOn;
    root["fan_out_on"] = state.fanOutOn;
    root["lights_mode"] = state.lightsMode;
    root["fan_in_mode"] = state.fanInMode;
    root["fan_out_mode"] = state.fanOutMode;
    root["inside_temperature"] = state.insideTemperature;
    root["inside_humidity"] = state.insideHumidity;
    root["outside_temperature"] = state.outsideTemperature;
    root["outside_humidity"] = state.outsideHumidity;
    root["moisture_raw"] = state.moistureRaw;
    root["moisture_percent"] = state.moisturePercent;
    root["moisture_calibrated"] = state.moistureCalibrated;
    root["moisture_alarm_active"] = state.moistureAlarmActive;
    root["moisture_alarm_acknowledged"] = state.moistureAlarmAcknowledged;
    root["inside_sensor_fault"] = state.insideSensorFault;
    root["outside_sensor_fault"] = state.outsideSensorFault;
    root["moisture_sensor_fault"] = state.moistureSensorFault;
    root["lights_reason"] = state.lightsReason;
    root["fan_in_reason"] = state.fanInReason;
    root["fan_out_reason"] = state.fanOutReason;
    root["moisture_alarm_reason"] = state.moistureAlarmReason;
    root["schedule_active"] = state.scheduleActive;
    root["manual_override_active"] = state.manualOverrideActive;
    root["uptime_ms"] = state.uptimeMs;
    root["boot_count"] = state.bootCount;
    root["ntp_synced"] = state.ntpSynced;
    root["wifi_connected"] = state.wifiConnected;
  }

  // ── External-protocol update (only writable fields) ───────────────────────
  // Only relay states and alarm acknowledgement are writable from external channels.
  // Sensor readings, modes, reason strings, and diagnostics are read-only.
  static StateUpdateResult update(JsonObject& root, GrowboxState& state) {
    bool changed = false;

    if (root.containsKey("lights_on")) {
      bool v = root["lights_on"];
      if (state.lightsOn != v) {
        state.lightsOn = v;
        state.lightAOn = v;
        state.lightBOn = v;
        changed = true;
      }
    }
    if (root.containsKey("light_a_on")) {
      bool v = root["light_a_on"];
      if (state.lightAOn != v) {
        state.lightAOn = v;
        // Keep lightsOn in sync: true only when both lights are on
        state.lightsOn = state.lightAOn && state.lightBOn;
        changed = true;
      }
    }
    if (root.containsKey("light_b_on")) {
      bool v = root["light_b_on"];
      if (state.lightBOn != v) {
        state.lightBOn = v;
        state.lightsOn = state.lightAOn && state.lightBOn;
        changed = true;
      }
    }
    if (root.containsKey("fan_in_on")) {
      bool v = root["fan_in_on"];
      if (state.fanInOn != v) {
        state.fanInOn = v;
        changed = true;
      }
    }
    if (root.containsKey("fan_out_on")) {
      bool v = root["fan_out_on"];
      if (state.fanOutOn != v) {
        state.fanOutOn = v;
        changed = true;
      }
    }
    if (root.containsKey("moisture_alarm_acknowledged")) {
      bool v = root["moisture_alarm_acknowledged"];
      if (state.moistureAlarmAcknowledged != v) {
        state.moistureAlarmAcknowledged = v;
        changed = true;
      }
    }

    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }

  // ── Home Assistant MQTT format helpers ────────────────────────────────────
  // Each relay entity uses HA's MQTT switch/light schema: {"state": "ON/OFF"}

  static void haReadLights(GrowboxState& state, JsonObject& root) {
    root["state"] = state.lightsOn ? "ON" : "OFF";
  }

  static StateUpdateResult haUpdateLights(JsonObject& root, GrowboxState& state) {
    bool v = String(root["state"] | "OFF") == "ON";
    if (state.lightsOn != v) {
      state.lightsOn = v;
      state.lightAOn = v;
      state.lightBOn = v;
      return StateUpdateResult::CHANGED;
    }
    return StateUpdateResult::UNCHANGED;
  }

  static void haReadFanIn(GrowboxState& state, JsonObject& root) {
    root["state"] = state.fanInOn ? "ON" : "OFF";
  }

  static StateUpdateResult haUpdateFanIn(JsonObject& root, GrowboxState& state) {
    bool v = String(root["state"] | "OFF") == "ON";
    if (state.fanInOn != v) {
      state.fanInOn = v;
      return StateUpdateResult::CHANGED;
    }
    return StateUpdateResult::UNCHANGED;
  }

  static void haReadFanOut(GrowboxState& state, JsonObject& root) {
    root["state"] = state.fanOutOn ? "ON" : "OFF";
  }

  static StateUpdateResult haUpdateFanOut(JsonObject& root, GrowboxState& state) {
    bool v = String(root["state"] | "OFF") == "ON";
    if (state.fanOutOn != v) {
      state.fanOutOn = v;
      return StateUpdateResult::CHANGED;
    }
    return StateUpdateResult::UNCHANGED;
  }

#if FT_ENABLED(FT_BLE)
  // ── BLE compact format — controls characteristic (R/W/Notify) ────────────
  // Keys abbreviated to keep payload well under BLE MTU (~250 bytes negotiated)
  static void readBleControls(GrowboxState& state, JsonObject& root) {
    root["la"] = state.lightAOn;
    root["lb"] = state.lightBOn;
    root["fi"] = state.fanInOn;
    root["fo"] = state.fanOutOn;
    root["ma"] = state.moistureAlarmActive;
    root["mak"] = state.moistureAlarmAcknowledged;
  }

  static StateUpdateResult updateBleControls(JsonObject& root, GrowboxState& state) {
    bool changed = false;
    if (root.containsKey("la")) {
      bool v = root["la"];
      if (state.lightAOn != v) {
        state.lightAOn = v;
        state.lightsOn = state.lightAOn && state.lightBOn;
        changed = true;
      }
    }
    if (root.containsKey("lb")) {
      bool v = root["lb"];
      if (state.lightBOn != v) {
        state.lightBOn = v;
        state.lightsOn = state.lightAOn && state.lightBOn;
        changed = true;
      }
    }
    if (root.containsKey("fi")) {
      bool v = root["fi"];
      if (state.fanInOn != v) {
        state.fanInOn = v;
        changed = true;
      }
    }
    if (root.containsKey("fo")) {
      bool v = root["fo"];
      if (state.fanOutOn != v) {
        state.fanOutOn = v;
        changed = true;
      }
    }
    if (root.containsKey("mak")) {
      bool v = root["mak"];
      if (state.moistureAlarmAcknowledged != v) {
        state.moistureAlarmAcknowledged = v;
        changed = true;
      }
    }
    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }

  // ── BLE compact format — sensors characteristic (R/Notify only) ──────────
  static void readBleSensors(GrowboxState& state, JsonObject& root) {
    root["it"] = state.insideTemperature;
    root["ih"] = state.insideHumidity;
    root["ot"] = state.outsideTemperature;
    root["oh"] = state.outsideHumidity;
    root["mp"] = state.moisturePercent;
    root["isf"] = state.insideSensorFault;
    root["osf"] = state.outsideSensorFault;
    root["msf"] = state.moistureSensorFault;
    root["ntp"] = state.ntpSynced;
    root["wifi"] = state.wifiConnected;
  }
#endif
};

class GrowboxService : public StatefulService<GrowboxState> {
 public:
  GrowboxService(AsyncWebServer* server,
                 SecurityManager* securityManager,
                 AsyncMqttClient* mqttClient,
                 GrowboxSettingsService* settingsService,
                 GrowboxAutomationService* automationService);
  void begin();
  void loop();

#if FT_ENABLED(FT_BLE)
  void setBleServer(BLEServer* bleServer);
  void configureBle();
#endif

 private:
  HttpEndpoint<GrowboxState> _httpEndpoint;
  WebSocketTxRx<GrowboxState> _webSocket;
  MqttPubSub<GrowboxState> _mqttPubSubLights;
  MqttPubSub<GrowboxState> _mqttPubSubFanIn;
  MqttPubSub<GrowboxState> _mqttPubSubFanOut;
  MqttPubSub<GrowboxAutomationSettings> _mqttPubSubAutomation;
  MqttPub<GrowboxState> _mqttPubFullState;
  AsyncMqttClient* _mqttClient;
  GrowboxSettingsService* _settingsService;
  GrowboxAutomationService* _automationService;

  SHT2x _sht20;
  DHT* _dht;
  uint8_t _currentDhtPin;

  unsigned long _lastPeriodicUpdate;
  unsigned long _moistureBelowThresholdSince;

#if FT_ENABLED(FT_BLE)
  BLEServer* _bleServer;
  BlePubSub<GrowboxState>* _bleControls;
  BlePub<GrowboxState>* _bleSensors;
  BlePubSub<GrowboxAutomationSettings>* _bleAutomation;
#endif

  void onConfigUpdated();
  void applyRelayStates();
  void periodicUpdate();
  void reinitPins();
  void readSensors(const GrowboxSettings& settings);
  void evaluateAutomation(const GrowboxSettings& settings, const GrowboxAutomationSettings& automation);
  bool isTimeBetween(const String& onTime, const String& offTime);
  void configureMqtt();

  // Inline helper: write a relay pin respecting active-high/low setting
  static void writeRelay(uint8_t pin, bool on, bool activeHigh) {
    digitalWrite(pin, (on == activeHigh) ? HIGH : LOW);
  }
};

#endif
