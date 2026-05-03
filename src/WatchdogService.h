#ifndef WatchdogService_h
#define WatchdogService_h

#include <Arduino.h>
#include <FS.h>
#include <ESPAsyncWebServer.h>
#include <SecurityManager.h>

// Path of the JSON file that holds the most recent restart reason.
// Written immediately before esp_restart(); read on boot.
#define WATCHDOG_REASON_FILE  "/config/lastRestartReason.json"

// REST endpoint that exposes the live watchdog state plus the previous-restart
// info to the dashboard.
#define WATCHDOG_STATUS_PATH  "/rest/watchdogStatus"

// App-level supervisor that hard-restarts the chip when it has been "unhealthy"
// for longer than UNHEALTHY_LIMIT, with a 4-minute grace period from boot to
// prevent restart death-loops on permanently-broken networks.
//
// Health is intentionally cheap to compute: free heap above a threshold, and at
// least one network interface usable (STA connected, OR an AP client paired
// so the user can reach us at 192.168.4.1).
class WatchdogService {
 public:
  WatchdogService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager);
  void begin();
  void loop();

 private:
  AsyncWebServer*  _server;
  FS*              _fs;
  SecurityManager* _securityManager;

  unsigned long _bootMs        = 0;
  unsigned long _lastHealthyMs = 0;
  unsigned long _lastCheckMs   = 0;
  bool          _checkedOnce   = false;

  // Loaded once from the FS at begin(); reflects the *previous* uptime.
  String        _previousReason;
  unsigned long _previousUptimeMs = 0;
  uint32_t      _previousFreeHeap = 0;

  bool isHealthy() const;
  void recordReasonAndRestart(const char* reason);
  void readPreviousReason();
  void registerStatusEndpoint();
};

#endif
