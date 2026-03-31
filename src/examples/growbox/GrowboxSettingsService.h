#ifndef GrowboxSettingsService_h
#define GrowboxSettingsService_h

#include <HttpEndpoint.h>
#include <FSPersistence.h>

#define GROWBOX_SETTINGS_FILE "/config/growboxSettings.json"
#define GROWBOX_SETTINGS_ENDPOINT_PATH "/rest/growboxSettings"

// Default safe GPIO pins for ESP32-S3 DevKitC-1
// Relay outputs on high-numbered safe pins, I2C/DHT/ADC on lower safe GPIOs
#define DEFAULT_LIGHT_A_PIN 38
#define DEFAULT_LIGHT_B_PIN 39
#define DEFAULT_FAN_IN_PIN 40
#define DEFAULT_FAN_OUT_PIN 41
#define DEFAULT_I2C_SDA_PIN 8
#define DEFAULT_I2C_SCL_PIN 9
#define DEFAULT_DHT_PIN 42
#define DEFAULT_MOISTURE_ADC_PIN 7

// Moisture sensor calibration defaults
// Adjust via UI calibration workflow after physical installation
#define DEFAULT_MOISTURE_DRY_VALUE 3100
#define DEFAULT_MOISTURE_WET_VALUE 1100

#define DEFAULT_RELAY_ACTIVE_HIGH true
#define DEFAULT_POLL_INTERVAL_MS 5000

#define DEFAULT_INSIDE_SENSOR_ENABLED true
#define DEFAULT_OUTSIDE_SENSOR_ENABLED true
#define DEFAULT_MOISTURE_SENSOR_ENABLED true

#define GROWBOX_SETTINGS_BUFFER_SIZE 512

class GrowboxSettings {
 public:
  uint8_t lightRelayAPin;
  uint8_t lightRelayBPin;
  uint8_t fanInPin;
  uint8_t fanOutPin;
  uint8_t i2cSdaPin;
  uint8_t i2cSclPin;
  uint8_t dhtPin;
  uint8_t moistureAdcPin;
  int moistureDryValue;
  int moistureWetValue;
  bool relayActiveHigh;
  uint16_t pollIntervalMs;
  bool insideSensorEnabled;
  bool outsideSensorEnabled;
  bool moistureSensorEnabled;

  static void read(GrowboxSettings& settings, JsonObject& root) {
    root["light_relay_a_pin"] = settings.lightRelayAPin;
    root["light_relay_b_pin"] = settings.lightRelayBPin;
    root["fan_in_pin"] = settings.fanInPin;
    root["fan_out_pin"] = settings.fanOutPin;
    root["i2c_sda_pin"] = settings.i2cSdaPin;
    root["i2c_scl_pin"] = settings.i2cSclPin;
    root["dht_pin"] = settings.dhtPin;
    root["moisture_adc_pin"] = settings.moistureAdcPin;
    root["moisture_dry_value"] = settings.moistureDryValue;
    root["moisture_wet_value"] = settings.moistureWetValue;
    root["relay_active_high"] = settings.relayActiveHigh;
    root["poll_interval_ms"] = settings.pollIntervalMs;
    root["inside_sensor_enabled"] = settings.insideSensorEnabled;
    root["outside_sensor_enabled"] = settings.outsideSensorEnabled;
    root["moisture_sensor_enabled"] = settings.moistureSensorEnabled;
  }

  static StateUpdateResult update(JsonObject& root, GrowboxSettings& settings) {
    settings.lightRelayAPin = root["light_relay_a_pin"] | (uint8_t)DEFAULT_LIGHT_A_PIN;
    settings.lightRelayBPin = root["light_relay_b_pin"] | (uint8_t)DEFAULT_LIGHT_B_PIN;
    settings.fanInPin = root["fan_in_pin"] | (uint8_t)DEFAULT_FAN_IN_PIN;
    settings.fanOutPin = root["fan_out_pin"] | (uint8_t)DEFAULT_FAN_OUT_PIN;
    settings.i2cSdaPin = root["i2c_sda_pin"] | (uint8_t)DEFAULT_I2C_SDA_PIN;
    settings.i2cSclPin = root["i2c_scl_pin"] | (uint8_t)DEFAULT_I2C_SCL_PIN;
    settings.dhtPin = root["dht_pin"] | (uint8_t)DEFAULT_DHT_PIN;
    settings.moistureAdcPin = root["moisture_adc_pin"] | (uint8_t)DEFAULT_MOISTURE_ADC_PIN;
    settings.moistureDryValue = root["moisture_dry_value"] | DEFAULT_MOISTURE_DRY_VALUE;
    settings.moistureWetValue = root["moisture_wet_value"] | DEFAULT_MOISTURE_WET_VALUE;
    settings.relayActiveHigh = root["relay_active_high"] | DEFAULT_RELAY_ACTIVE_HIGH;
    settings.pollIntervalMs = root["poll_interval_ms"] | (uint16_t)DEFAULT_POLL_INTERVAL_MS;
    settings.insideSensorEnabled = root["inside_sensor_enabled"] | DEFAULT_INSIDE_SENSOR_ENABLED;
    settings.outsideSensorEnabled = root["outside_sensor_enabled"] | DEFAULT_OUTSIDE_SENSOR_ENABLED;
    settings.moistureSensorEnabled = root["moisture_sensor_enabled"] | DEFAULT_MOISTURE_SENSOR_ENABLED;
    return StateUpdateResult::CHANGED;
  }
};

class GrowboxSettingsService : public StatefulService<GrowboxSettings> {
 public:
  GrowboxSettingsService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager);
  void begin();

 private:
  HttpEndpoint<GrowboxSettings> _httpEndpoint;
  FSPersistence<GrowboxSettings> _fsPersistence;
};

#endif
