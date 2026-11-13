#ifndef WSoftProductsService_h
#define WSoftProductsService_h

#include <Features.h>
#if FT_ENABLED(FT_WSOFT)

// Products on the board's web page (W-Soft > Products). Rules in lib/wsoft_core (wsoft_products).
//   GET  /rest/wsoftProducts  (logged in)  {"current":"CODE"|"","board_max":20,"products":[
//          {"code","name","low_g","target_g","high_g","from":"hub"|"board","hidden":bool}]}
//   POST /rest/wsoftProducts  (admin)      one of
//          {"action":"put","old_code":""|"CODE","product":{"code","name","low_g","target_g","high_g"}}
//          {"action":"delete","code":"CODE"}
//          {"action":"use","code":"CODE"|""}
//        answers like GET, or 400/409 with {"error":"CODE","message":"plain words"}.
// Hub products are read-only here; board products stay on the board (not sent to the Hub as
// products, but every weighing carries its code and limits).

#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <SecurityManager.h>

#define WSOFT_PRODUCTS_ENDPOINT_PATH "/rest/wsoftProducts"

class WSoftProductsService {
 public:
  WSoftProductsService(AsyncWebServer* server, SecurityManager* securityManager);

 private:
  void read(AsyncWebServerRequest* request);
  void update(AsyncWebServerRequest* request, JsonVariant& json);
};

#endif  // FT_WSOFT
#endif
