#ifndef JsonUtils_h
#define JsonUtils_h

#include <Arduino.h>
#include <IPUtils.h>
#include <ArduinoJson.h>

class JsonUtils {
 public:
  static void readIP(JsonObject& root, const String& key, IPAddress& ip, const String& def) {
    IPAddress defaultIp = {};
    if (!defaultIp.fromString(def)) {
      defaultIp = INADDR_NONE;
    }
    readIP(root, key, ip, defaultIp);
  }
  // Arduino-ESP32 3.x can build an IPAddress from text, so a string-literal default would
  // match both overloads above and below; this one picks the String overload.
  static void readIP(JsonObject& root, const String& key, IPAddress& ip, const char* def) {
    readIP(root, key, ip, String(def));
  }
  static void readIP(JsonObject& root, const String& key, IPAddress& ip, const IPAddress& defaultIp = INADDR_NONE) {
    if (!root[key].is<String>() || !ip.fromString(root[key].as<String>())) {
      ip = defaultIp;
    }
  }
  static void writeIP(JsonObject& root, const String& key, const IPAddress& ip) {
    if (IPUtils::isSet(ip)) {
      root[key] = ip.toString();
    }
  }
};

#endif  // end JsonUtils
