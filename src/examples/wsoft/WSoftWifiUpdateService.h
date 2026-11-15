#ifndef WSoftWifiUpdateService_h
#define WSoftWifiUpdateService_h

#include <Features.h>
#if FT_ENABLED(FT_WSOFT)

// The "Wi-Fi update" switch on the web (W-Soft > Settings, admin):
//   GET  /rest/wsoftWifiUpdate  (admin)  the switch, whether a password is set, why it is blocked
//   POST /rest/wsoftWifiUpdate  (admin)  {"on":true|false} and/or {"password":"text"}
// The password is checked and stored on the board and never sent back, echoed or logged.
// Rules: lib/wsoft_core/src/wsoft_wifi_update.h. The board side: lib/wsoft_esp/src/wifi_update.h.
// Errors: 409 (or 400 for a bad shape) with {"error":"CODE","message":"plain words"}.

#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <SecurityManager.h>

#define WSOFT_WIFI_UPDATE_ENDPOINT_PATH "/rest/wsoftWifiUpdate"

class WSoftWifiUpdateService {
 public:
  WSoftWifiUpdateService(AsyncWebServer* server, SecurityManager* securityManager);

 private:
  SecurityManager* _securityManager;

  String username(AsyncWebServerRequest* request);
  void read(AsyncWebServerRequest* request);
  void update(AsyncWebServerRequest* request, JsonVariant& json);
};

#endif  // FT_WSOFT
#endif
