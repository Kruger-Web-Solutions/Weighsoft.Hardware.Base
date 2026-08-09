#include <examples/liveweight/LiveWeightDiscovery.h>
#include <examples/liveweight/LiveWeightService.h>

void LiveWeightDiscovery::begin() {
  _lastAnnounceMs = 0;
  _lastIp = "";
  _mdnsReady = false;
  _lastSendOk = false;
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
    // Announce before mDNS so a flaky MDNS path cannot block presence
    announce();
    _lastAnnounceMs = millis();
    ensureMdns();
    return;
  }

  const unsigned long now = millis();
  if (now - _lastAnnounceMs >= LIVE_WEIGHT_DISCOVERY_INTERVAL_MS) {
    _lastAnnounceMs = now;
    announce();
  }

  ensureMdns();
}

void LiveWeightDiscovery::ensureUdp() {
  if (_udpReady) {
    return;
  }
  // ESP8266 rejects port 0 — bind a fixed local port for outbound announces
  if (_udp.begin(LIVE_WEIGHT_DISCOVERY_LOCAL_PORT)) {
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

bool LiveWeightDiscovery::announceTo(const IPAddress& dest) {
  if (WiFi.status() != WL_CONNECTED) {
    _lastSendOk = false;
    return false;
  }
  ensureUdp();
  if (!_udpReady) {
    _lastSendOk = false;
    return false;
  }

  char payload[192];
  const size_t len = buildPayload(payload, sizeof(payload));
  if (len == 0 || len >= sizeof(payload)) {
    _lastSendOk = false;
    return false;
  }

  bool ok = false;
  if (_udp.beginPacket(dest, LIVE_WEIGHT_DISCOVERY_UDP_PORT)) {
    const size_t written = _udp.write(reinterpret_cast<const uint8_t*>(payload), len);
    ok = _udp.endPacket() != 0 && written == len;
  }
  _lastSendOk = ok;
  return ok;
}

void LiveWeightDiscovery::announce() {
  IPAddress limited = IPAddress(255, 255, 255, 255);
  if (WiFi.subnetMask()) {
    const uint32_t ip = (uint32_t)WiFi.localIP();
    const uint32_t mask = (uint32_t)WiFi.subnetMask();
    limited = IPAddress(ip | ~mask);
  }

  // Prefer global broadcast first (some STA stacks drop directed subnet bcast)
  bool ok = announceTo(IPAddress(255, 255, 255, 255));
  if (limited != IPAddress(255, 255, 255, 255)) {
    ok = announceTo(limited) || ok;
  }
  _lastSendOk = ok;
}
