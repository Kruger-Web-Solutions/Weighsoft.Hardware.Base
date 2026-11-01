#include <Features.h>
#if FT_ENABLED(FT_WSOFT)

#include <examples/wsoft/WSoftHubService.h>

#include <RestartService.h>
#include <hub.h>

WSoftHubService::WSoftHubService(AsyncWebServer* server, SecurityManager* securityManager) :
    _pairHandler(WSOFT_HUB_ENDPOINT_PATH,
                 securityManager->wrapCallback(
                     std::bind(&WSoftHubService::pair, this, std::placeholders::_1, std::placeholders::_2),
                     AuthenticationPredicates::IS_ADMIN)) {
  server->on(WSOFT_HUB_ENDPOINT_PATH,
             HTTP_GET,
             securityManager->wrapRequest(std::bind(&WSoftHubService::status, this, std::placeholders::_1),
                                          AuthenticationPredicates::IS_ADMIN));
  server->on(WSOFT_HUB_ENDPOINT_PATH,
             HTTP_DELETE,
             securityManager->wrapRequest(std::bind(&WSoftHubService::unpair, this, std::placeholders::_1),
                                          AuthenticationPredicates::IS_ADMIN));
  _pairHandler.setMethod(HTTP_POST);
  _pairHandler.setMaxContentLength(WSOFT_HUB_MAX_BODY);
  server->addHandler(&_pairHandler);
}

void WSoftHubService::status(AsyncWebServerRequest* request) {
  AsyncJsonResponse* response = new AsyncJsonResponse(false, 768);
  JsonObject root = response->getRoot();
  bool configured = hub_configured();
  root["configured"] = configured;
  root["state"] = hub_state_text(hub_state());
  root["online"] = hub_state() == HUB_ONLINE;
  if (configured) {
    // Not secret: the key id travels in every request header. The secret never leaves the box.
    const char* in_use = hub_address();
    root["hub_url"] = in_use;  // the address in use now
    JsonArray all = root.createNestedArray("hub_urls");
    for (uint8_t i = 0; i < hub_address_count(); i++) all.add(hub_address_at(i));
    root["key_id"] = hub_key_id();
    HubUrlHint hint = hub_url_hint(in_use);
    if (hint != HUB_URL_FINE) root["warning"] = hub_url_hint_text(hint);
  }
  root["last_status"] = hub_last_status();
  root["clock_valid"] = hub_clock_valid();
  root["clock_from_hub"] = hub_clock_from_hub();
  response->setLength();
  request->send(response);
}

void WSoftHubService::pair(AsyncWebServerRequest* request, JsonVariant& json) {
  const char* code = json.is<JsonObject>() ? json["code"].as<const char*>() : nullptr;
  String saved_hub_url;
  HubCodeResult result = hub_save_code(code, &saved_hub_url);

  AsyncJsonResponse* response = new AsyncJsonResponse(false, 768);
  JsonObject root = response->getRoot();
  root["ok"] = result == HUB_CODE_OK;
  root["message"] = hub_code_result_text(result);
  if (result == HUB_CODE_OK) {
    root["hub_url"] = saved_hub_url;
    HubUrlHint hint = hub_url_hint(saved_hub_url.c_str());
    if (hint != HUB_URL_FINE) root["warning"] = hub_url_hint_text(hint);
    root["restarting"] = true;
    response->setCode(200);
    request->onDisconnect(RestartService::restartNow);
  } else {
    response->setCode(400);
  }
  response->setLength();
  request->send(response);
}

void WSoftHubService::unpair(AsyncWebServerRequest* request) {
  hub_forget_code();
  request->onDisconnect(RestartService::restartNow);
  request->send(200, "application/json", "{\"ok\":true,\"restarting\":true}");
}

#endif  // FT_WSOFT
