#ifndef LiveWeightDiscovery_h
#define LiveWeightDiscovery_h

#include <Arduino.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#endif

// UDP announce for Option 2 discovery (sender auto-adopt). Lean for ESP8266.
#define LIVE_WEIGHT_DISCOVERY_UDP_PORT 4210
#define LIVE_WEIGHT_DISCOVERY_INTERVAL_MS 5000
#define LIVE_WEIGHT_DISCOVERY_SVC "weighsoft-lw"

/**
 * Board-side LAN announce: UDP broadcast + optional mDNS service.
 * Not a StatefulService — no persisted config; identity comes from WiFi.
 */
class LiveWeightDiscovery {
 public:
  void begin();
  void loop();

 private:
  WiFiUDP _udp;
  bool _udpReady = false;
  bool _mdnsReady = false;
  unsigned long _lastAnnounceMs = 0;
  String _lastIp;

  void ensureUdp();
  void ensureMdns();
  void announce();
  size_t buildPayload(char* buf, size_t buflen);
};

#endif
