#ifndef SettingValue_h
#define SettingValue_h

#include <Arduino.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif

namespace SettingValue {
String format(String value);
/** STA MAC as 12 lowercase hex chars (used in MQTT paths and device_id). */
String getUniqueId();
};

#endif  // end SettingValue
