#include "MdnsBrowser.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <SettingValue.h>
#include <WiFi.h>

// Tunables — keep small and self-explanatory.
static const unsigned long SCAN_INTERVAL_MS = 30UL * 1000UL;   // every 30 s
static const unsigned long ENTRY_TTL_MS     = 90UL * 1000UL;   // expire after 3 missed scans
static const uint32_t      QUERY_TIMEOUT_MS = 1500;            // mDNS query budget
static const char*         SERVICE_NAME     = "weighsoft";
static const char*         SERVICE_PROTO    = "tcp";

MdnsBrowser::MdnsBrowser(AsyncWebServer* server, SecurityManager* securityManager)
    : _server(server), _securityManager(securityManager) {}

void MdnsBrowser::begin() {
  _enabled = true;
  registerEndpoint();
  Serial.printf("[MDNSb] MdnsBrowser started — scan_interval=%lus, entry_ttl=%lus\n",
                SCAN_INTERVAL_MS / 1000UL,
                ENTRY_TTL_MS / 1000UL);
}

void MdnsBrowser::loop() {
  if (!_enabled) return;
  unsigned long now = millis();
  // Throttle: skip until the interval elapses. The first call after begin()
  // gets through immediately so the cache populates quickly post-boot.
  if (_lastScanMs != 0 && (now - _lastScanMs) < SCAN_INTERVAL_MS) {
    pruneExpired();
    return;
  }
  // Wait until WiFi is actually up — querying with no STA is a no-op that
  // wastes a 1.5 s timeout window each round.
  if (WiFi.status() != WL_CONNECTED && WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
    return;
  }
  _lastScanMs = now;
  scan();
  pruneExpired();
}

void MdnsBrowser::scan() {
  // ESPmDNS::queryService is blocking. Cap the budget at QUERY_TIMEOUT_MS.
  const String selfId = SettingValue::getUniqueId();
  const unsigned long now = millis();

  int n = MDNS.queryService(SERVICE_NAME, SERVICE_PROTO);
  if (n <= 0) {
    Serial.printf("[MDNSb] scan: no services found (n=%d)\n", n);
    return;
  }

  Serial.printf("[MDNSb] scan: %d services found\n", n);

  for (int i = 0; i < n; i++) {
    String hostname = MDNS.hostname(i);
    IPAddress ip   = MDNS.IP(i);
    uint16_t port  = MDNS.port(i);

    // TXT records — empty string if not present.
    String id   = MDNS.hasTxt(i, "id")   ? MDNS.txt(i, "id")   : String();
    String name = MDNS.hasTxt(i, "name") ? MDNS.txt(i, "name") : String();
    String mode = MDNS.hasTxt(i, "mode") ? MDNS.txt(i, "mode") : String();

    // Skip ourselves — we're discovering peers, not echoing back.
    if (id.length() > 0 && id == selfId) continue;
    // Use ID as the unique-key when present; fall back to ip:port for older
    // announcements that don't include the TXT record yet.
    String key = id.length() > 0 ? id : (ip.toString() + ":" + String(port));

    bool updated = false;
    for (auto& d : _devices) {
      String dKey = d.id.length() > 0 ? d.id : (d.ipAddress + ":" + String(d.port));
      if (dKey == key) {
        d.id          = id;
        d.name        = name;
        d.mode        = mode;
        d.hostname    = hostname;
        d.ipAddress   = ip.toString();
        d.port        = port;
        d.lastSeenMs  = now;
        updated = true;
        break;
      }
    }
    if (!updated) {
      DiscoveredDevice d;
      d.id          = id;
      d.name        = name;
      d.mode        = mode;
      d.hostname    = hostname;
      d.ipAddress   = ip.toString();
      d.port        = port;
      d.lastSeenMs  = now;
      _devices.push_back(d);
    }
  }
}

void MdnsBrowser::pruneExpired() {
  const unsigned long now = millis();
  for (auto it = _devices.begin(); it != _devices.end(); ) {
    if (now - it->lastSeenMs > ENTRY_TTL_MS) {
      it = _devices.erase(it);
    } else {
      ++it;
    }
  }
}

std::vector<MdnsBrowser::DiscoveredDevice> MdnsBrowser::getDiscovered() const {
  return _devices;
}

void MdnsBrowser::registerEndpoint() {
  _server->on(
      MDNS_BROWSE_ENDPOINT_PATH,
      HTTP_GET,
      _securityManager->wrapRequest(
          [this](AsyncWebServerRequest* request) {
            AsyncJsonResponse* response = new AsyncJsonResponse(false, 2048);
            JsonObject root = response->getRoot();
            JsonArray arr = root.createNestedArray("devices");
            const unsigned long now = millis();
            for (const auto& d : _devices) {
              if (now - d.lastSeenMs > ENTRY_TTL_MS) continue;
              JsonObject o = arr.createNestedObject();
              o["id"]               = d.id;
              o["name"]             = d.name;
              o["mode"]             = d.mode;
              o["hostname"]         = d.hostname;
              o["ip"]               = d.ipAddress;
              o["port"]             = d.port;
              o["last_seen_ms_ago"] = now - d.lastSeenMs;
            }
            response->setLength();
            request->send(response);
          },
          AuthenticationPredicates::IS_AUTHENTICATED));
}
