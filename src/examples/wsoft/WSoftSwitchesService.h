#ifndef WSoftSwitchesService_h
#define WSoftSwitchesService_h

#include <Features.h>
#if FT_ENABLED(FT_WSOFT)

// The W-Soft switches, the screen PIN and the RS485 scale setup on the web
// (contract A in docs/superpowers/plans/2026-11-07-wsoft-switches-scale-setup.md):
//   GET  /rest/wsoftSwitches    (logged in)  Approval and Auto save state, the last 8 approval log lines
//   POST /rest/wsoftSwitches    (admin)      {"switch":"approval"|"auto_save","on":bool,"reason":"text"}
//   POST /rest/wsoftPin         (admin)      {"pin":"1234"}: sets the screen PIN (never sent back or logged)
//   GET  /rest/wsoftScaleSetup  (logged in)  the board's part of the setup, and the card's e as last read
//   POST /rest/wsoftScaleSetup  (admin)      {"board":{...},"reason":"text"}
// A switch goes through switches.h, the same path as the screen, the Hub and the USB line.
// Errors: 409 (or 400 for a bad shape) with {"error":"CODE","message":"plain words"}.

#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <SecurityManager.h>

#define WSOFT_SWITCHES_ENDPOINT_PATH "/rest/wsoftSwitches"
#define WSOFT_PIN_ENDPOINT_PATH "/rest/wsoftPin"
#define WSOFT_SCALE_SETUP_ENDPOINT_PATH "/rest/wsoftScaleSetup"

class WSoftSwitchesService {
 public:
  WSoftSwitchesService(AsyncWebServer* server, SecurityManager* securityManager);

 private:
  SecurityManager* _securityManager;

  String username(AsyncWebServerRequest* request);
  void readSwitches(AsyncWebServerRequest* request);
  void updateSwitch(AsyncWebServerRequest* request, JsonVariant& json);
  void updatePin(AsyncWebServerRequest* request, JsonVariant& json);
  void readScaleSetup(AsyncWebServerRequest* request);
  void updateScaleSetup(AsyncWebServerRequest* request, JsonVariant& json);
};

#endif  // FT_WSOFT
#endif
