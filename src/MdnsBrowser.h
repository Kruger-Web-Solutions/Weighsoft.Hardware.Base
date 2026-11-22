#ifndef MdnsBrowser_h
#define MdnsBrowser_h

#include <Arduino.h>
#include <vector>
#include <ESPAsyncWebServer.h>
#include <SecurityManager.h>

#define MDNS_BROWSE_ENDPOINT_PATH "/rest/discovered"

// Periodically queries the local network for `_weighsoft._tcp` services and
// caches the results. Devices announce themselves via MdnsService; this is
// the matching "browse" half so each device can discover its peers without
// the user having to type IPs.
//
// Lightweight on purpose:
//   - Scan every 30 s (mDNS query is somewhat heavy, this matches typical
//     announce TTLs).
//   - Cached entries expire after 90 s of no-show (3 missed scans).
//   - Self-entries (matching SettingValue::getUniqueId()) are filtered out.
class MdnsBrowser {
 public:
  struct DiscoveredDevice {
    String   id;            // Hardware unique ID (TXT record `id`)
    String   name;          // Friendly name (TXT record `name`, falls back to hostname)
    String   mode;          // "reader" | "writer" | "diagnostics" | "new"
    String   hostname;      // mDNS hostname (e.g. weighsoft-abc123.local)
    String   ipAddress;     // Dotted-quad IPv4
    uint16_t port;          // Service port
    unsigned long lastSeenMs;  // millis() at the last successful sighting
  };

  MdnsBrowser(AsyncWebServer* server, SecurityManager* securityManager);
  void begin();
  void loop();

  // Snapshot of the current cache (excludes expired entries).
  std::vector<DiscoveredDevice> getDiscovered() const;

 private:
  AsyncWebServer*  _server;
  SecurityManager* _securityManager;

  std::vector<DiscoveredDevice> _devices;
  unsigned long _lastScanMs = 0;
  bool          _enabled    = false;

  void scan();
  void registerEndpoint();
  void pruneExpired();
};

#endif
