#ifndef GrowboxAutomationService_h
#define GrowboxAutomationService_h

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <WebSocketTxRx.h>

#define GROWBOX_AUTOMATION_FILE "/config/growboxAutomation.json"
#define GROWBOX_AUTOMATION_ENDPOINT_PATH "/rest/growboxAutomation"
#define GROWBOX_AUTOMATION_SOCKET_PATH "/ws/growboxAutomation"

// Safe conservative defaults
#define DEFAULT_LIGHT_SCHEDULE_ENABLED false
#define DEFAULT_LIGHT_ON_TIME "06:00"
#define DEFAULT_LIGHT_OFF_TIME "22:00"
#define DEFAULT_FAN_CONTROL_ENABLED false
#define DEFAULT_FAN_TEMPERATURE_THRESHOLD 28.0f
#define DEFAULT_FAN_HYSTERESIS 2.0f
#define DEFAULT_MOISTURE_ALARM_ENABLED false
#define DEFAULT_MOISTURE_ALARM_THRESHOLD 20
#define DEFAULT_MOISTURE_ALARM_DWELL_MS 30000

#define GROWBOX_AUTOMATION_BUFFER_SIZE 512

class GrowboxAutomationSettings {
 public:
  // Light schedule
  bool lightScheduleEnabled;
  String lightOnTime;   // "HH:MM" in local time (uses NTP timezone)
  String lightOffTime;  // "HH:MM"

  // Fan automation
  bool fanControlEnabled;
  float fanTemperatureThreshold;  // °C to turn fans on
  float fanHysteresis;            // °C band below threshold to turn fans off

  // Moisture alarm
  bool moistureAlarmEnabled;
  int moistureAlarmThreshold;   // % — alarm fires when below this
  uint32_t moistureAlarmDwellMs;  // ms the reading must be below threshold before alarm fires

  static void read(GrowboxAutomationSettings& settings, JsonObject& root) {
    root["light_schedule_enabled"] = settings.lightScheduleEnabled;
    root["light_on_time"] = settings.lightOnTime;
    root["light_off_time"] = settings.lightOffTime;
    root["fan_control_enabled"] = settings.fanControlEnabled;
    root["fan_temperature_threshold"] = settings.fanTemperatureThreshold;
    root["fan_hysteresis"] = settings.fanHysteresis;
    root["moisture_alarm_enabled"] = settings.moistureAlarmEnabled;
    root["moisture_alarm_threshold"] = settings.moistureAlarmThreshold;
    root["moisture_alarm_dwell_ms"] = settings.moistureAlarmDwellMs;
  }

  static StateUpdateResult update(JsonObject& root, GrowboxAutomationSettings& settings) {
    settings.lightScheduleEnabled = root["light_schedule_enabled"] | DEFAULT_LIGHT_SCHEDULE_ENABLED;
    settings.lightOnTime = root["light_on_time"] | DEFAULT_LIGHT_ON_TIME;
    settings.lightOffTime = root["light_off_time"] | DEFAULT_LIGHT_OFF_TIME;
    settings.fanControlEnabled = root["fan_control_enabled"] | DEFAULT_FAN_CONTROL_ENABLED;
    settings.fanTemperatureThreshold = root["fan_temperature_threshold"] | DEFAULT_FAN_TEMPERATURE_THRESHOLD;
    settings.fanHysteresis = root["fan_hysteresis"] | DEFAULT_FAN_HYSTERESIS;
    settings.moistureAlarmEnabled = root["moisture_alarm_enabled"] | DEFAULT_MOISTURE_ALARM_ENABLED;
    settings.moistureAlarmThreshold = root["moisture_alarm_threshold"] | DEFAULT_MOISTURE_ALARM_THRESHOLD;
    settings.moistureAlarmDwellMs = root["moisture_alarm_dwell_ms"] | (uint32_t)DEFAULT_MOISTURE_ALARM_DWELL_MS;
    return StateUpdateResult::CHANGED;
  }
};

class GrowboxAutomationService : public StatefulService<GrowboxAutomationSettings> {
 public:
  GrowboxAutomationService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager);
  void begin();

 private:
  HttpEndpoint<GrowboxAutomationSettings> _httpEndpoint;
  FSPersistence<GrowboxAutomationSettings> _fsPersistence;
  WebSocketTxRx<GrowboxAutomationSettings> _webSocket;
};

#endif
