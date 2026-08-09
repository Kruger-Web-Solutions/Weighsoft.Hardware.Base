#include <examples/liveweight/LiveWeightDiscovery.h>
#include <examples/liveweight/LiveWeightService.h>

void LiveWeightDiscovery::begin() {
  _lastAnnounceMs = 0;
  _lastIp = "";
  _mdnsReady = false;
  ensureUdp();
}

void LiveWeightDiscovery::loop() {
  if (WiFi.status() != WL_CONNECTED) {
    _mdnsReady = false;
    _lastIp = "";
    return;
  }

  const String ip = WiFi.localIP().toString();
  if (ip != _lastIp) {
    _lastIp = ip;
    _mdnsReady = false;
    ensureUdp();
    ensureMdns();
    // Announce immediately on join / IP change
    announce();
    _lastAnnounceMs = millis();
    return;
  }

  ensureMdns();

  const unsigned long now = millis();
  if (now - _lastAnnounceMs >= LIVE_WEIGHT_DISCOVERY_INTERVAL_MS) {
    _lastAnnounceMs = now;
    announce();
  }
}

void LiveWeightDiscovery::ensureUdp() {
  if (_udpReady) {
    return;
  }
  // ESP8266 rejects port 0 — bind a fixed local port for outbound announces
  if (_udp.begin(LIVE_WEIGHT_DISCOVERY_UDP_PORT + 1)) {
    _udpReady = true;
    Serial.println(F("[LiveWeightDiscovery] UDP announce ready"));
  } else {
    Serial.println(F("[LiveWeightDiscovery] UDP begin failed"));
  }
}

void LiveWeightDiscovery::ensureMdns() {
  if (_mdnsReady || WiFi.status() != WL_CONNECTED) {
    return;
  }

#ifdef ESP8266
  String hostStr = WiFi.hostname();
#elif defined(ESP32)
  String hostStr = WiFi.getHostname() ? String(WiFi.getHostname()) : String("weighsoft-lw");
#else
  String hostStr = "weighsoft-lw";
#endif
  if (hostStr.length() == 0) {
    hostStr = "weighsoft-lw";
  }

  // ArduinoOTA may already have started mDNS; begin is safe to call again
  if (!MDNS.begin(hostStr.c_str())) {
    return;
  }
  MDNS.addService(LIVE_WEIGHT_DISCOVERY_SVC, "tcp", 80);
  MDNS.addServiceTxt(LIVE_WEIGHT_DISCOVERY_SVC, "tcp", "path", LIVE_WEIGHT_ENDPOINT_PATH);
  MDNS.addServiceTxt(LIVE_WEIGHT_DISCOVERY_SVC, "tcp", "ws", LIVE_WEIGHT_SOCKET_PATH);
  _mdnsReady = true;
  Serial.printf_P(PSTR("[LiveWeightDiscovery] mDNS %s._%s._tcp.local\r\n"),
                  hostStr.c_str(),
                  LIVE_WEIGHT_DISCOVERY_SVC);
}

size_t LiveWeightDiscovery::buildPayload(char* buf, size_t buflen) {
#ifdef ESP8266
  String hostStr = WiFi.hostname();
  const uint32_t chipId = ESP.getChipId();
#elif defined(ESP32)
  String hostStr = WiFi.getHostname() ? String(WiFi.getHostname()) : String("weighsoft-lw");
  const uint32_t chipId = (uint32_t)ESP.getEfuseMac();
#else
  String hostStr = "weighsoft-lw";
  const uint32_t chipId = 0;
#endif
  if (hostStr.length() == 0) {
    hostStr = "weighsoft-lw";
  }
  const String ipStr = WiFi.localIP().toString();

  // Compact JSON — keep under ~160 B for ESP8266 heap
  return snprintf(buf,
                  buflen,
                  "{\"v\":1,\"svc\":\"%s\",\"id\":\"%08x\",\"host\":\"%s\",\"ip\":\"%s\","
                  "\"http\":80,\"rest\":\"%s\",\"ws\":\"%s\"}",
                  LIVE_WEIGHT_DISCOVERY_SVC,
                  chipId,
                  hostStr.c_str(),
                  ipStr.c_str(),
                  LIVE_WEIGHT_ENDPOINT_PATH,
                  LIVE_WEIGHT_SOCKET_PATH);
}

void LiveWeightDiscovery::announce() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  ensureUdp();
  if (!_udpReady) {
    return;
  }

  char payload[192];
  const size_t len = buildPayload(payload, sizeof(payload));
  if (len == 0 || len >= sizeof(payload)) {
    return;
  }

  IPAddress limited = IPAddress(255, 255, 255, 255);
  if (WiFi.subnetMask()) {
    const uint32_t ip = (uint32_t)WiFi.localIP();
    const uint32_t mask = (uint32_t)WiFi.subnetMask();
    limited = IPAddress(ip | ~mask);
  }

  // Send subnet broadcast + global broadcast (some LANs only deliver one)
  const IPAddress targets[] = {limited, IPAddress(255, 255, 255, 255)};
  for (const IPAddress& dest : targets) {
    if (_udp.beginPacket(dest, LIVE_WEIGHT_DISCOVERY_UDP_PORT)) {
      _udp.write(reinterpret_cast<const uint8_t*>(payload), len);
      _udp.endPacket();
    }
  }
}
