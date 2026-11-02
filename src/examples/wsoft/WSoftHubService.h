#ifndef WSoftHubService_h
#define WSoftHubService_h

#include <Features.h>
#if FT_ENABLED(FT_WSOFT)

// "Link to Hub" for the W-Soft web page (Hub tab). Admins only.
//
//   GET    /rest/wsoftHub   state, Hub address, key id, last Hub answer, clock. Never the secret.
//   POST   /rest/wsoftHub   {"code":"<pairing code from the Hub>"}: checked, stored, then the box restarts.
//   DELETE /rest/wsoftHub   forget the pairing code, then the box restarts.
//
// Same storage and heartbeat code as the USB "HUB PAIR" / "HUB SAVE" / "HUB CLEAR" lines
// (lib/wsoft_esp/src/hub.cpp). Approval is NOT here: it stays USB only.

#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <SecurityManager.h>

#define WSOFT_HUB_ENDPOINT_PATH "/rest/wsoftHub"
#define WSOFT_HUB_MAX_BODY 1024

class WSoftHubService {
 public:
  WSoftHubService(AsyncWebServer* server, SecurityManager* securityManager);

 private:
  AsyncCallbackJsonWebHandler _pairHandler;

  void status(AsyncWebServerRequest* request);
  void pair(AsyncWebServerRequest* request, JsonVariant& json);
  void unpair(AsyncWebServerRequest* request);
};

#endif  // FT_WSOFT
#endif
