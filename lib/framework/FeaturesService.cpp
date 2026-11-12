#include <FeaturesService.h>

FeaturesService::FeaturesService(AsyncWebServer* server) {
  server->on(FEATURES_SERVICE_PATH,
             HTTP_GET,
             ArRequestHandlerFunction(std::bind(&FeaturesService::features, this, std::placeholders::_1)));
}

void FeaturesService::features(AsyncWebServerRequest* request) {
  AsyncJsonResponse* response = new AsyncJsonResponse(false, MAX_FEATURES_SIZE);
  JsonObject root = response->getRoot();
#if FT_ENABLED(FT_PROJECT)
  root["project"] = true;
#else
  root["project"] = false;
#endif
#if FT_ENABLED(FT_SECURITY)
  root["security"] = true;
#else
  root["security"] = false;
#endif
#if FT_ENABLED(FT_MQTT)
  root["mqtt"] = true;
#else
  root["mqtt"] = false;
#endif
#if FT_ENABLED(FT_NTP)
  root["ntp"] = true;
#else
  root["ntp"] = false;
#endif
#if FT_ENABLED(FT_OTA)
  root["ota"] = true;
#else
  root["ota"] = false;
#endif
#if FT_ENABLED(FT_UPLOAD_FIRMWARE)
  root["upload_firmware"] = true;
#else
  root["upload_firmware"] = false;
#endif
#if FT_ENABLED(FT_BLE)
  root["ble"] = true;
#else
  root["ble"] = false;
#endif
  // Which device this build is, so the web menu shows only pages this board serves.
  root["wsoft"] = FT_ENABLED(FT_WSOFT) != 0;
  root["wsoft_relay"] = FT_ENABLED(FT_WSOFT_RELAY) != 0;
#if defined(WSOFT_HAS_RS232) && WSOFT_HAS_RS232
  root["wsoft_rs232"] = true;
#else
  root["wsoft_rs232"] = false;
#endif
  response->setLength();
  request->send(response);
}
