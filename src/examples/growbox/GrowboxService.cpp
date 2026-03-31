#include <examples/growbox/GrowboxService.h>

#ifdef ESP32
#include <WiFi.h>
#include <time.h>
#endif

// Minimum epoch for NTP validity check (1 Jan 2020)
#define NTP_VALID_EPOCH 1577836800UL

// How often to update diagnostics and read sensors (ms)
#define GROWBOX_POLL_DEFAULT_MS 5000

// DHT22 minimum read interval — the sensor needs ~2s between reads
#define DHT_MIN_INTERVAL_MS 2500

GrowboxService::GrowboxService(AsyncWebServer* server,
                               SecurityManager* securityManager,
                               AsyncMqttClient* mqttClient,
                               GrowboxSettingsService* settingsService,
                               GrowboxAutomationService* automationService) :
    _httpEndpoint(GrowboxState::read,
                  GrowboxState::update,
                  this,
                  server,
                  GROWBOX_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED,
                  GROWBOX_STATE_BUFFER_SIZE),
    _webSocket(GrowboxState::read,
               GrowboxState::update,
               this,
               server,
               GROWBOX_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_AUTHENTICATED,
               GROWBOX_STATE_BUFFER_SIZE),
    _mqttPubSubLights(GrowboxState::haReadLights, GrowboxState::haUpdateLights, this, mqttClient),
    _mqttPubSubFanIn(GrowboxState::haReadFanIn, GrowboxState::haUpdateFanIn, this, mqttClient),
    _mqttPubSubFanOut(GrowboxState::haReadFanOut, GrowboxState::haUpdateFanOut, this, mqttClient),
    _mqttPubSubAutomation(GrowboxAutomationSettings::read,
                          GrowboxAutomationSettings::update,
                          automationService,
                          mqttClient),
    _mqttPubFullState(GrowboxState::read, this, mqttClient),
    _mqttClient(mqttClient),
    _settingsService(settingsService),
    _automationService(automationService),
    _dht(nullptr),
    _currentDhtPin(0),
    _lastPeriodicUpdate(0),
    _moistureBelowThresholdSince(0)
#if FT_ENABLED(FT_BLE)
    ,
    _bleServer(nullptr),
    _bleControls(nullptr),
    _bleSensors(nullptr),
    _bleAutomation(nullptr)
#endif
{
  // Re-init GPIO pins and sensors whenever hardware settings change
  _settingsService->addUpdateHandler(
      [this](const String& originId) { reinitPins(); }, false);

  // Apply relay states whenever live state changes from any channel
  addUpdateHandler([this](const String& originId) { onConfigUpdated(); }, false);

  // Configure MQTT topics and HA discovery on each MQTT connect
  _mqttClient->onConnect(std::bind(&GrowboxService::configureMqtt, this));
}

void GrowboxService::begin() {
  // Load pin config and initialise hardware before anything else
  reinitPins();

  // Start with everything off and modes set to manual
  update(
      [](GrowboxState& state) {
        state.lightsOn = false;
        state.lightAOn = false;
        state.lightBOn = false;
        state.fanInOn = false;
        state.fanOutOn = false;
        state.lightsMode = "manual";
        state.fanInMode = "manual";
        state.fanOutMode = "manual";
        state.insideTemperature = 0.0f;
        state.insideHumidity = 0.0f;
        state.outsideTemperature = 0.0f;
        state.outsideHumidity = 0.0f;
        state.moistureRaw = 0;
        state.moisturePercent = 0.0f;
        state.moistureCalibrated = false;
        state.moistureAlarmActive = false;
        state.moistureAlarmAcknowledged = false;
        state.insideSensorFault = false;
        state.outsideSensorFault = false;
        state.moistureSensorFault = false;
        state.lightsReason = "manual";
        state.fanInReason = "manual";
        state.fanOutReason = "manual";
        state.moistureAlarmReason = "";
        state.scheduleActive = false;
        state.manualOverrideActive = false;
        state.uptimeMs = 0;
        state.bootCount = 0;
        state.ntpSynced = false;
        state.wifiConnected = false;
        return StateUpdateResult::CHANGED;
      },
      "init");

  // Ensure relays are physically off after boot regardless of any stored state
  applyRelayStates();

  Serial.println("[Growbox] Service started");
}

void GrowboxService::loop() {
  GrowboxSettings settings;
  _settingsService->update(
      [&settings](GrowboxSettings& s) {
        settings = s;
        return StateUpdateResult::UNCHANGED;
      },
      "local");

  GrowboxAutomationSettings automation;
  _automationService->update(
      [&automation](GrowboxAutomationSettings& a) {
        automation = a;
        return StateUpdateResult::UNCHANGED;
      },
      "local");

  unsigned long now = millis();
  uint16_t interval = settings.pollIntervalMs > 0 ? settings.pollIntervalMs : GROWBOX_POLL_DEFAULT_MS;
  if (now - _lastPeriodicUpdate >= interval) {
    _lastPeriodicUpdate = now;
    periodicUpdate();
    readSensors(settings);
    evaluateAutomation(settings, automation);
  }
}

void GrowboxService::periodicUpdate() {
  update(
      [](GrowboxState& state) {
        state.uptimeMs = millis();
#ifdef ESP32
        state.wifiConnected = (WiFi.status() == WL_CONNECTED);
        time_t t;
        time(&t);
        state.ntpSynced = (t > NTP_VALID_EPOCH);
#endif
        return StateUpdateResult::CHANGED;
      },
      "diagnostic");
}

void GrowboxService::readSensors(const GrowboxSettings& settings) {
  // ── SHT20 inside sensor (I2C) ─────────────────────────────────────────────
  if (settings.insideSensorEnabled) {
    bool insideFault = false;
    float insideTemp = 0.0f, insideHum = 0.0f;
    if (_sht20.isConnected()) {
      if (_sht20.read()) {
        insideTemp = _sht20.getTemperature();
        insideHum = _sht20.getHumidity();
        // Sanity check: reject obviously invalid readings
        if (isnan(insideTemp) || isnan(insideHum) || insideTemp < -40.0f || insideTemp > 125.0f) {
          insideFault = true;
        }
      } else {
        insideFault = true;
      }
    } else {
      insideFault = true;
    }
    update(
        [insideFault, insideTemp, insideHum](GrowboxState& state) {
          state.insideSensorFault = insideFault;
          if (!insideFault) {
            state.insideTemperature = insideTemp;
            state.insideHumidity = insideHum;
          }
          return StateUpdateResult::CHANGED;
        },
        "sensor");
  }

  // ── DHT22 outside sensor (one-wire) ───────────────────────────────────────
  if (settings.outsideSensorEnabled && _dht != nullptr) {
    bool outsideFault = false;
    float outsideTemp = _dht->readTemperature();
    float outsideHum = _dht->readHumidity();
    if (isnan(outsideTemp) || isnan(outsideHum) || outsideTemp < -40.0f || outsideTemp > 80.0f) {
      outsideFault = true;
    }
    update(
        [outsideFault, outsideTemp, outsideHum](GrowboxState& state) {
          state.outsideSensorFault = outsideFault;
          if (!outsideFault) {
            state.outsideTemperature = outsideTemp;
            state.outsideHumidity = outsideHum;
          }
          return StateUpdateResult::CHANGED;
        },
        "sensor");
  }

  // ── Capacitive moisture sensor (ADC) ──────────────────────────────────────
  if (settings.moistureSensorEnabled) {
    int raw = analogRead(settings.moistureAdcPin);
    bool calibrated = (settings.moistureDryValue != settings.moistureWetValue) &&
                      (settings.moistureDryValue > 0) &&
                      (settings.moistureWetValue > 0);
    float moisturePercent = 0.0f;
    if (calibrated) {
      // Map raw ADC to 0–100 %, clamped at bounds
      float ratio = (float)(settings.moistureDryValue - raw) /
                    (float)(settings.moistureDryValue - settings.moistureWetValue);
      moisturePercent = constrain(ratio * 100.0f, 0.0f, 100.0f);
    }
    // Fault: raw reading is out of expected ADC range (0–4095 on ESP32)
    bool moistureFault = (raw < 0 || raw > 4095);
    update(
        [raw, moisturePercent, calibrated, moistureFault](GrowboxState& state) {
          state.moistureRaw = raw;
          state.moisturePercent = moisturePercent;
          state.moistureCalibrated = calibrated;
          state.moistureSensorFault = moistureFault;
          return StateUpdateResult::CHANGED;
        },
        "sensor");
  }
}

void GrowboxService::onConfigUpdated() {
  applyRelayStates();
}

void GrowboxService::applyRelayStates() {
  // Read current settings to get pin assignments and polarity
  GrowboxSettings settings;
  _settingsService->update(
      [&settings](GrowboxSettings& s) {
        settings = s;
        return StateUpdateResult::UNCHANGED;
      },
      "local");

  // Read current relay states via update-with-UNCHANGED (thread-safe read)
  bool lightA = false, lightB = false, fanIn = false, fanOut = false;
  update(
      [&](GrowboxState& state) {
        lightA = state.lightAOn;
        lightB = state.lightBOn;
        fanIn = state.fanInOn;
        fanOut = state.fanOutOn;
        return StateUpdateResult::UNCHANGED;
      },
      "local");

  writeRelay(settings.lightRelayAPin, lightA, settings.relayActiveHigh);
  writeRelay(settings.lightRelayBPin, lightB, settings.relayActiveHigh);
  writeRelay(settings.fanInPin, fanIn, settings.relayActiveHigh);
  writeRelay(settings.fanOutPin, fanOut, settings.relayActiveHigh);
}

void GrowboxService::reinitPins() {
  // Read pin configuration from settings
  GrowboxSettings settings;
  _settingsService->update(
      [&settings](GrowboxSettings& s) {
        settings = s;
        return StateUpdateResult::UNCHANGED;
      },
      "local");

  // Configure relay output pins and drive them to off immediately
  const uint8_t offLevel = settings.relayActiveHigh ? LOW : HIGH;
  auto initRelay = [offLevel](uint8_t pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, offLevel);
  };

  initRelay(settings.lightRelayAPin);
  initRelay(settings.lightRelayBPin);
  initRelay(settings.fanInPin);
  initRelay(settings.fanOutPin);

  // (Re-)initialise I2C for SHT20 — configure bus pins first, then init sensor
  // SHT2x uses the global Wire instance (default constructor arg); Wire.begin()
  // must be called with the correct pins before SHT2x::begin() communicates.
  Wire.begin(settings.i2cSdaPin, settings.i2cSclPin);
  _sht20.begin();

  // (Re-)initialise DHT22 if pin changed or first call
  if (settings.dhtPin != _currentDhtPin || _dht == nullptr) {
    if (_dht != nullptr) {
      delete _dht;
    }
    _dht = new DHT(settings.dhtPin, DHT22);
    _dht->begin();
    _currentDhtPin = settings.dhtPin;
  }

  Serial.printf("[Growbox] Pins init: LA=%d LB=%d FI=%d FO=%d SDA=%d SCL=%d DHT=%d ADC=%d activeHigh=%d\n",
                settings.lightRelayAPin,
                settings.lightRelayBPin,
                settings.fanInPin,
                settings.fanOutPin,
                settings.i2cSdaPin,
                settings.i2cSclPin,
                settings.dhtPin,
                settings.moistureAdcPin,
                settings.relayActiveHigh);
}

// ── Automation evaluation ────────────────────────────────────────────────────

bool GrowboxService::isTimeBetween(const String& onTime, const String& offTime) {
#ifdef ESP32
  time_t now;
  time(&now);
  struct tm* localNow = localtime(&now);
  if (localNow == nullptr) return false;

  int nowMinutes = localNow->tm_hour * 60 + localNow->tm_min;
  int onMinutes = onTime.substring(0, 2).toInt() * 60 + onTime.substring(3, 5).toInt();
  int offMinutes = offTime.substring(0, 2).toInt() * 60 + offTime.substring(3, 5).toInt();

  if (onMinutes <= offMinutes) {
    // Same-day schedule (e.g. 06:00 – 22:00)
    return (nowMinutes >= onMinutes && nowMinutes < offMinutes);
  } else {
    // Overnight schedule (e.g. 22:00 – 06:00)
    return (nowMinutes >= onMinutes || nowMinutes < offMinutes);
  }
#else
  return false;
#endif
}

void GrowboxService::evaluateAutomation(const GrowboxSettings& settings,
                                        const GrowboxAutomationSettings& automation) {
  // Read current live state for decision-making
  float insideTemp = 0.0f;
  float moisturePercent = 0.0f;
  bool insideFault = false, moistureFault = false, moistureCalibrated = false;
  bool ntpSynced = false;

  update(
      [&](GrowboxState& state) {
        insideTemp = state.insideTemperature;
        insideFault = state.insideSensorFault;
        moisturePercent = state.moisturePercent;
        moistureFault = state.moistureSensorFault;
        moistureCalibrated = state.moistureCalibrated;
        ntpSynced = state.ntpSynced;
        return StateUpdateResult::UNCHANGED;
      },
      "local");

  // ── Light schedule ──────────────────────────────────────────────────────────
  bool lightsShould = false;
  String lightsMode = "manual";
  String lightsReason = "manual control";

  if (automation.lightScheduleEnabled && ntpSynced) {
    lightsShould = isTimeBetween(automation.lightOnTime, automation.lightOffTime);
    lightsMode = "schedule";
    lightsReason = lightsShould
                       ? ("scheduled on (" + automation.lightOnTime + "–" + automation.lightOffTime + ")")
                       : ("scheduled off until " + automation.lightOnTime);
  }

  // ── Fan automation ──────────────────────────────────────────────────────────
  bool fanOnShould = false;
  String fanMode = "manual";
  String fanReason = "manual control";

  if (automation.fanControlEnabled && !insideFault) {
    // Read current fan states to apply hysteresis
    bool fanCurrentlyOn = false;
    update(
        [&fanCurrentlyOn](GrowboxState& state) {
          fanCurrentlyOn = state.fanInOn || state.fanOutOn;
          return StateUpdateResult::UNCHANGED;
        },
        "local");

    float offThreshold = automation.fanTemperatureThreshold - automation.fanHysteresis;
    if (insideTemp >= automation.fanTemperatureThreshold) {
      fanOnShould = true;
    } else if (fanCurrentlyOn && insideTemp > offThreshold) {
      fanOnShould = true;  // Hysteresis: stay on until temp drops below off threshold
    }
    fanMode = "auto";
    fanReason = fanOnShould
                    ? ("temp " + String(insideTemp, 1) + "°C >= " + String(automation.fanTemperatureThreshold, 1) + "°C")
                    : ("temp " + String(insideTemp, 1) + "°C < " + String(offThreshold, 1) + "°C");
  } else if (automation.fanControlEnabled && insideFault) {
    fanMode = "auto";
    fanReason = "sensor fault — fans paused";
  }

  // ── Moisture alarm ──────────────────────────────────────────────────────────
  bool moistureAlarm = false;
  String moistureAlarmReason = "";

  if (automation.moistureAlarmEnabled && moistureCalibrated && !moistureFault) {
    if (moisturePercent < automation.moistureAlarmThreshold) {
      if (_moistureBelowThresholdSince == 0) {
        _moistureBelowThresholdSince = millis();
      }
      unsigned long dwellElapsed = millis() - _moistureBelowThresholdSince;
      if (dwellElapsed >= automation.moistureAlarmDwellMs) {
        moistureAlarm = true;
        moistureAlarmReason = String("moisture ") + String(moisturePercent, 0) + "% below " +
                              String(automation.moistureAlarmThreshold) + "% threshold";
      }
    } else {
      _moistureBelowThresholdSince = 0;  // Reset dwell timer when above threshold
    }
  } else {
    _moistureBelowThresholdSince = 0;
  }

  // ── Apply computed states ───────────────────────────────────────────────────
  update(
      [&](GrowboxState& state) {
        bool changed = false;

        if (automation.lightScheduleEnabled && ntpSynced) {
          if (state.lightsOn != lightsShould) {
            state.lightsOn = lightsShould;
            state.lightAOn = lightsShould;
            state.lightBOn = lightsShould;
            changed = true;
          }
          state.lightsMode = lightsMode;
          state.lightsReason = lightsReason;
        }

        if (automation.fanControlEnabled && !insideFault) {
          if (state.fanInOn != fanOnShould) {
            state.fanInOn = fanOnShould;
            changed = true;
          }
          if (state.fanOutOn != fanOnShould) {
            state.fanOutOn = fanOnShould;
            changed = true;
          }
          state.fanInMode = fanMode;
          state.fanOutMode = fanMode;
          state.fanInReason = fanReason;
          state.fanOutReason = fanReason;
        }

        if (automation.moistureAlarmEnabled) {
          if (state.moistureAlarmActive != moistureAlarm) {
            state.moistureAlarmActive = moistureAlarm;
            // Clear acknowledgement when alarm newly fires
            if (moistureAlarm) state.moistureAlarmAcknowledged = false;
            changed = true;
          }
          state.moistureAlarmReason = moistureAlarmReason;
        }

        state.scheduleActive = automation.lightScheduleEnabled;
        return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
      },
      "automation");
}

// ── MQTT configuration and Home Assistant discovery ──────────────────────────

void GrowboxService::configureMqtt() {
  if (!_mqttClient->connected()) {
    return;
  }

  // Build HA-style base paths using SettingValue unique_id placeholder
  String uid = SettingValue::format("#{unique_id}");
  String deviceName = SettingValue::format("Growbox-#{unique_id}");

  // Growbox-specific topic base: growbox/<uid>
  String base = "growbox/" + uid;

  // ── Relay control topics (HA switch/light compatible) ─────────────────────
  String lightsStateTopic = base + "/lights/state";
  String lightsCommandTopic = base + "/lights/set";
  _mqttPubSubLights.configureTopics(lightsStateTopic, lightsCommandTopic);

  String fanInStateTopic = base + "/fan_in/state";
  String fanInCommandTopic = base + "/fan_in/set";
  _mqttPubSubFanIn.configureTopics(fanInStateTopic, fanInCommandTopic);

  String fanOutStateTopic = base + "/fan_out/state";
  String fanOutCommandTopic = base + "/fan_out/set";
  _mqttPubSubFanOut.configureTopics(fanOutStateTopic, fanOutCommandTopic);

  // ── Automation settings topics ────────────────────────────────────────────
  String autoStateTopic = base + "/automation/state";
  String autoCommandTopic = base + "/automation/set";
  _mqttPubSubAutomation.configureTopics(autoStateTopic, autoCommandTopic);

  // Full state topic (for custom dashboards and diagnostics) — auto-published on state change
  String fullStateTopic = base + "/state";
  _mqttPubFullState.setPubTopic(fullStateTopic);

  // ── Home Assistant auto-discovery ─────────────────────────────────────────
  // Device descriptor shared by all entities
  DynamicJsonDocument deviceDoc(200);
  JsonObject device = deviceDoc.to<JsonObject>();
  device["identifiers"][0] = uid;
  device["name"] = deviceName;
  device["model"] = "Growbox ESP32-S3";
  device["manufacturer"] = "Weighsoft";

  auto publishDiscovery = [&](const String& component,
                               const String& objectId,
                               const String& entityName,
                               const JsonObject& extra) {
    String configTopic = "homeassistant/" + component + "/" + uid + "-" + objectId + "/config";
    DynamicJsonDocument doc(512);
    JsonObject config = doc.to<JsonObject>();
    config["name"] = entityName;
    config["unique_id"] = uid + "-" + objectId;
    config["device"] = device;
    for (JsonPair kv : extra) {
      config[kv.key()] = kv.value();
    }
    String payload;
    serializeJson(doc, payload);
    _mqttClient->publish(configTopic.c_str(), 0, true, payload.c_str());
  };

  DynamicJsonDocument extDoc(256);
  JsonObject ext = extDoc.to<JsonObject>();

  // Lights (HA light entity)
  ext.clear();
  ext["stat_t"] = lightsStateTopic;
  ext["cmd_t"] = lightsCommandTopic;
  ext["payload_on"] = "ON";
  ext["payload_off"] = "OFF";
  publishDiscovery("light", "lights", deviceName + " Lights", ext);

  // Fan In (HA switch)
  ext.clear();
  ext["stat_t"] = fanInStateTopic;
  ext["cmd_t"] = fanInCommandTopic;
  publishDiscovery("switch", "fan-in", deviceName + " Fan In", ext);

  // Fan Out (HA switch)
  ext.clear();
  ext["stat_t"] = fanOutStateTopic;
  ext["cmd_t"] = fanOutCommandTopic;
  publishDiscovery("switch", "fan-out", deviceName + " Fan Out", ext);

  // Sensors — all point to the full state topic and use value_template
  ext.clear();
  ext["stat_t"] = fullStateTopic;
  ext["val_tpl"] = "{{ value_json.inside_temperature | round(1) }}";
  ext["unit_of_meas"] = "°C";
  ext["dev_cla"] = "temperature";
  ext["state_class"] = "measurement";
  publishDiscovery("sensor", "inside-temp", deviceName + " Inside Temperature", ext);

  ext.clear();
  ext["stat_t"] = fullStateTopic;
  ext["val_tpl"] = "{{ value_json.inside_humidity | round(1) }}";
  ext["unit_of_meas"] = "%";
  ext["dev_cla"] = "humidity";
  ext["state_class"] = "measurement";
  publishDiscovery("sensor", "inside-hum", deviceName + " Inside Humidity", ext);

  ext.clear();
  ext["stat_t"] = fullStateTopic;
  ext["val_tpl"] = "{{ value_json.outside_temperature | round(1) }}";
  ext["unit_of_meas"] = "°C";
  ext["dev_cla"] = "temperature";
  ext["state_class"] = "measurement";
  publishDiscovery("sensor", "outside-temp", deviceName + " Outside Temperature", ext);

  ext.clear();
  ext["stat_t"] = fullStateTopic;
  ext["val_tpl"] = "{{ value_json.outside_humidity | round(1) }}";
  ext["unit_of_meas"] = "%";
  ext["dev_cla"] = "humidity";
  ext["state_class"] = "measurement";
  publishDiscovery("sensor", "outside-hum", deviceName + " Outside Humidity", ext);

  ext.clear();
  ext["stat_t"] = fullStateTopic;
  ext["val_tpl"] = "{{ value_json.moisture_percent | round(0) }}";
  ext["unit_of_meas"] = "%";
  ext["state_class"] = "measurement";
  ext["icon"] = "mdi:water-percent";
  publishDiscovery("sensor", "moisture", deviceName + " Soil Moisture", ext);

  // Moisture alarm (HA binary_sensor)
  ext.clear();
  ext["stat_t"] = fullStateTopic;
  ext["val_tpl"] = "{{ 'ON' if value_json.moisture_alarm_active else 'OFF' }}";
  ext["dev_cla"] = "moisture";
  publishDiscovery("binary_sensor", "moisture-alarm", deviceName + " Moisture Alarm", ext);

  // ext doc done — the _mqttPubFullState will publish the current full state automatically
  // now that its topic is configured

  Serial.println("[Growbox] MQTT configured and HA discovery published");
}

// ── BLE configuration (called after BLEServer is started) ────────────────────

#if FT_ENABLED(FT_BLE)

void GrowboxService::setBleServer(BLEServer* bleServer) {
  _bleServer = bleServer;
}

void GrowboxService::configureBle() {
  if (!_bleServer) {
    return;
  }

  BLEService* pService = _bleServer->createService(GROWBOX_BLE_SERVICE_UUID);

  // Controls characteristic: relay outputs + alarm (R/W/Notify)
  BLECharacteristic* controlsChar = pService->createCharacteristic(
      GROWBOX_BLE_CONTROLS_CHAR_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  _bleControls =
      new BlePubSub<GrowboxState>(GrowboxState::readBleControls, GrowboxState::updateBleControls, this, _bleServer);
  _bleControls->configureCharacteristic(controlsChar);

  // Sensors characteristic: temperature, humidity, moisture (R/Notify only)
  BLECharacteristic* sensorsChar =
      pService->createCharacteristic(GROWBOX_BLE_SENSORS_CHAR_UUID,
                                     BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  _bleSensors = new BlePub<GrowboxState>(GrowboxState::readBleSensors, this, _bleServer);
  _bleSensors->setCharacteristic(sensorsChar);

  // Automation settings characteristic: schedule + thresholds (R/W/Notify)
  BLECharacteristic* automationChar = pService->createCharacteristic(
      GROWBOX_BLE_AUTOMATION_CHAR_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  _bleAutomation = new BlePubSub<GrowboxAutomationSettings>(GrowboxAutomationSettings::read,
                                                            GrowboxAutomationSettings::update,
                                                            _automationService,
                                                            _bleServer);
  _bleAutomation->configureCharacteristic(automationChar);

  pService->start();
  Serial.println("[Growbox] BLE service and characteristics configured");
}

#endif  // FT_ENABLED(FT_BLE)
