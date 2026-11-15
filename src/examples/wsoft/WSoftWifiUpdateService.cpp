#include <Features.h>
#if FT_ENABLED(FT_WSOFT)

#include <examples/wsoft/WSoftWifiUpdateService.h>

#include <WiFi.h>

#include <wifi_update.h>
#include <wsoft_wifi_update.h>

#include "version.h"

namespace {

void sendError(AsyncWebServerRequest* request, int code, const char* error, const char* message) {
  AsyncJsonResponse* response = new AsyncJsonResponse(false, 256);
  JsonObject root = response->getRoot().to<JsonObject>();
  root["error"] = error;
  root["message"] = message;
  response->setCode(code);
  response->setLength();
  request->send(response);
}

// Never the password: only whether the board has one.
void sendInfo(AsyncWebServerRequest* request) {
  WifiUpdateInfo info;
  wifi_update_info(&info);
  AsyncJsonResponse* response = new AsyncJsonResponse(false, 512);
  JsonObject root = response->getRoot().to<JsonObject>();
  root["possible"] = info.two_copies;
  root["on"] = info.on;
  root["password_set"] = info.password_set;
  root["approval_on"] = info.approval_on;
  root["listening"] = info.listening;
  if (info.blocked == WSOFT_WIFI_UPDATE_OK) {
    root["blocked"] = (const char*)nullptr;
  } else {
    root["blocked"] = wsoft_wifi_update_error_code(info.blocked);
    root["blocked_message"] = wsoft_wifi_update_error_text(info.blocked);
  }
  root["port"] = WIFI_UPDATE_PORT;
  root["password_min"] = (unsigned)WSOFT_WIFI_UPDATE_PASSWORD_MIN;
  root["password_max"] = (unsigned)WSOFT_WIFI_UPDATE_PASSWORD_MAX;
  root["version"] = VERSION_STRING;
  response->setCode(200);
  response->setLength();
  request->send(response);
}

}  // namespace

WSoftWifiUpdateService::WSoftWifiUpdateService(AsyncWebServer* server, SecurityManager* securityManager) :
    _securityManager(securityManager) {
  wifi_update_begin(VERSION_STRING);
  server->on(WSOFT_WIFI_UPDATE_ENDPOINT_PATH,
             HTTP_GET,
             securityManager->wrapRequest(std::bind(&WSoftWifiUpdateService::read, this, std::placeholders::_1),
                                          AuthenticationPredicates::IS_ADMIN));
  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler(
      WSOFT_WIFI_UPDATE_ENDPOINT_PATH,
      securityManager->wrapCallback(
          std::bind(&WSoftWifiUpdateService::update, this, std::placeholders::_1, std::placeholders::_2),
          AuthenticationPredicates::IS_ADMIN),
      256);
  handler->setMethod(HTTP_POST);
  server->addHandler(handler);
}

// The signed-in name for the log. The admin check has already passed.
String WSoftWifiUpdateService::username(AsyncWebServerRequest* request) {
  Authentication auth = _securityManager->authenticateRequest(request);
  if (auth.authenticated && auth.user != nullptr && auth.user->username.length()) return auth.user->username;
  return "admin";
}

void WSoftWifiUpdateService::read(AsyncWebServerRequest* request) {
  sendInfo(request);
}

// {"on":true|false} and/or {"password":"text"}. A field that is not sent keeps its value.
void WSoftWifiUpdateService::update(AsyncWebServerRequest* request, JsonVariant& json) {
  if (!json.is<JsonObject>() || (json.containsKey("on") && !json["on"].is<bool>()) ||
      (json.containsKey("password") && !json["password"].is<const char*>())) {
    sendError(request, 400, "BAD_REQUEST", "Send on (true or false) and/or password (text).");
    return;
  }
  WsoftWifiUpdateRequest change;
  if (json.containsKey("on")) {
    change.has_on = true;
    change.on = json["on"].as<bool>();
  }
  if (json.containsKey("password")) change.password = json["password"].as<const char*>();
  String who = username(request);
  WsoftWifiUpdateError error = wifi_update_set(change, who.c_str());
  if (error != WSOFT_WIFI_UPDATE_OK) {
    bool shape = error == WSOFT_WIFI_UPDATE_PASSWORD_FORMAT || error == WSOFT_WIFI_UPDATE_PASSWORD_FACTORY ||
                 error == WSOFT_WIFI_UPDATE_NOTHING;
    sendError(request, shape ? 400 : 409, wsoft_wifi_update_error_code(error), wsoft_wifi_update_error_text(error));
    return;
  }
  sendInfo(request);
}

#endif  // FT_WSOFT
