#include <Features.h>
#if FT_ENABLED(FT_WSOFT)

#include <examples/wsoft/WSoftStatusService.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <RestartService.h>

#include <hub_web.h>
#include <scale.h>
#include <weighings.h>
#include <wsoft_djb.h>
#include <wsoft_screen.h>

namespace {

// Maps the pure view onto an ArduinoJson object.
class JsonObjectSink : public WSoftJsonSink {
 public:
  explicit JsonObjectSink(JsonObject& root) : _root(root) {}
  void put_null(const char* key) override {
    // ArduinoJson 6 stores a null string pointer as JSON null.
    _root[key] = (const char*)nullptr;
  }
  void put_bool(const char* key, bool value) override { _root[key] = value; }
  void put_int(const char* key, int64_t value) override {
    if (value >= LONG_MIN && value <= LONG_MAX) {
      _root[key] = (long)value;
    } else {
      _root[key] = (double)value;  // whole grams beyond 32 bits are still exact in a double
    }
  }
  void put_str(const char* key, const char* value) override {
    // ArduinoJson 6 keeps a const char* by pointer but copies a char*. Some values are
    // stack buffers or live in the state, which can change before the JSON is sent,
    // so force the copy.
    _root[key] = const_cast<char*>(value);
  }

 private:
  JsonObject& _root;
};

// Same view, ignoring the poll counter and timestamp that change on every pass.
bool sameView(const WSoftSnapshot& a, const WSoftSnapshot& b) {
  return a.valid == b.valid && a.display_ready == b.display_ready && a.linked == b.linked && a.fresh == b.fresh &&
         a.alarm == b.alarm && a.value == b.value && a.decimals == b.decimals && a.unit == b.unit &&
         a.approval_on == b.approval_on && a.stable == b.stable && a.trade_check == b.trade_check &&
         a.audit_counter == b.audit_counter && a.last_trade_seq == b.last_trade_seq &&
         a.trade_queue == b.trade_queue && a.hub_online == b.hub_online && a.save_waiting == b.save_waiting &&
         strncmp(a.hub_text, b.hub_text, sizeof a.hub_text) == 0;
}

}  // namespace

void WSoftStatus::read(WSoftStatus& status, JsonObject& root) {
  JsonObjectSink sink(root);
  wsoft_view_write(status.snapshot, sink);
}

WSoftStatusService::WSoftStatusService(AsyncWebServer* server,
                                       SecurityManager* securityManager,
                                       AsyncMqttClient* mqttClient,
                                       WSoftSettingsService* settingsService) :
    _httpEndpoint(WSoftStatus::read,
                  this,
                  server,
                  WSOFT_STATUS_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED),
    _webSocket(WSoftStatus::read,
               this,
               server,
               WSOFT_STATUS_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_AUTHENTICATED),
#if FT_ENABLED(FT_MQTT)
    // Retained, so a subscriber that joins later still sees the current state.
    _mqttPub(WSoftStatus::read, this, mqttClient, "", true),
    _mqttClient(mqttClient),
#endif
    _settingsService(settingsService) {
  AsyncCallbackJsonWebHandler* djbHandler = new AsyncCallbackJsonWebHandler(
      WSOFT_DJB_ENDPOINT_PATH,
      securityManager->wrapCallback(std::bind(&WSoftStatusService::handleDjb, this, std::placeholders::_1, std::placeholders::_2),
                                    AuthenticationPredicates::IS_ADMIN),
      256);
  djbHandler->setMethod(HTTP_POST);
  server->addHandler(djbHandler);
  server->on(WSOFT_HUB_ENDPOINT_PATH,
             HTTP_GET,
             securityManager->wrapRequest(std::bind(&WSoftStatusService::readHub, this, std::placeholders::_1),
                                          AuthenticationPredicates::IS_ADMIN));
  AsyncCallbackJsonWebHandler* hubHandler = new AsyncCallbackJsonWebHandler(
      WSOFT_HUB_ENDPOINT_PATH,
      securityManager->wrapCallback(std::bind(&WSoftStatusService::updateHub, this, std::placeholders::_1, std::placeholders::_2),
                                    AuthenticationPredicates::IS_ADMIN),
      1024);
  hubHandler->setMethod(HTTP_POST);
  server->addHandler(hubHandler);
  server->on(WSOFT_SAVE_ENDPOINT_PATH,
             HTTP_GET,
             securityManager->wrapRequest(std::bind(&WSoftStatusService::readSave, this, std::placeholders::_1),
                                          AuthenticationPredicates::IS_AUTHENTICATED));
  server->on(WSOFT_SAVE_ENDPOINT_PATH,
             HTTP_POST,
             securityManager->wrapRequest(std::bind(&WSoftStatusService::save, this, std::placeholders::_1),
                                          AuthenticationPredicates::IS_AUTHENTICATED));
#if FT_ENABLED(FT_MQTT)
  _mqttClient->onConnect(std::bind(&WSoftStatusService::configureMqtt, this));
  // Topic or MQTT on/off changed on the settings page.
  _settingsService->addUpdateHandler([this](const String&) { configureMqtt(); }, false);
#else
  (void)mqttClient;
#endif
}

namespace {

// Hub link as JSON. Only the address and ids: the pairing key itself never leaves the board.
void sendHub(AsyncWebServerRequest* request, int code, bool restarting, const char* error) {
  HubWebInfo info;
  hub_web_info(&info);
  AsyncJsonResponse* response = new AsyncJsonResponse(false, 512);
  JsonObject root = response->getRoot().to<JsonObject>();
  root["ok"] = error == nullptr;
  if (error != nullptr) root["error"] = error;
  root["paired"] = info.paired;
  root["state"] = hub_state_text(info.state);
  root["online"] = info.state == HUB_ONLINE;
  root["hub_url"] = const_cast<char*>(info.hub_url);
  root["key_id"] = const_cast<char*>(info.key_id);
  root["device_id"] = const_cast<char*>(info.device_id);
  root["last_status"] = info.last_status;
  root["last_age_s"] = info.last_age_s;
  root["restarting"] = restarting;
  response->setCode(code);
  response->setLength();
  request->send(response);
}

}  // namespace

namespace {

void putWeighing(JsonObject& root, const char* key, const TradeWeight& w) {
  if (!w.valid) {
    root[key] = (const char*)nullptr;
    return;
  }
  JsonObject out = root.createNestedObject(key);
  char weight[16];
  format_weight(w.value, w.decimals, weight, sizeof weight);
  out["seq"] = w.seq;
  out["weight"] = weight;  // ArduinoJson copies a char*
  out["unit"] = weight_unit_text(w.unit);
  out["at"] = (long)w.at;  // unix seconds
}

void sendSave(AsyncWebServerRequest* request, int code, WsoftSaveResult result, const TradeWeight* saved) {
  WeighingsInfo info;
  weighings_info(&info);
  AsyncJsonResponse* response = new AsyncJsonResponse(false, 512);
  JsonObject root = response->getRoot().to<JsonObject>();
  root["pending"] = (unsigned)info.pending;
  putWeighing(root, "last", info.last);
  if (saved != nullptr) {
    root["ok"] = result == WSOFT_SAVE_SAVED;
    root["result"] = wsoft_save_result_text(result);
    if (result == WSOFT_SAVE_SAVED) putWeighing(root, "saved", *saved);
  }
  response->setCode(code);
  response->setLength();
  request->send(response);
}

}  // namespace

void WSoftStatusService::readSave(AsyncWebServerRequest* request) {
  sendSave(request, 200, WSOFT_SAVE_NONE, nullptr);
}

// Asks the screen task to save the load on the scale and waits for its answer (one pass,
// at most 250 ms; 1.5 s cap). 200 when saved; 409 with the reason when not.
void WSoftStatusService::save(AsyncWebServerRequest* request) {
  TradeWeight saved;
  WsoftSaveResult result = weighings_save(1500, &saved);
  sendSave(request, result == WSOFT_SAVE_SAVED ? 200 : 409, result, &saved);
}

void WSoftStatusService::readHub(AsyncWebServerRequest* request) {
  sendHub(request, 200, false, nullptr);
}

// {"action":"connect","code":"<pairing key>"} or {"action":"disconnect"}.
void WSoftStatusService::updateHub(AsyncWebServerRequest* request, JsonVariant& json) {
  const char* action = json["action"] | "";
  if (strcmp(action, "connect") == 0) {
    WsoftHubCodeError error = hub_web_save(json["code"] | "");
    if (error != WSOFT_HUB_CODE_OK) {
      sendHub(request, 400, false, wsoft_hub_code_error_text(error));
      return;
    }
  } else if (strcmp(action, "disconnect") == 0) {
    hub_web_forget();
  } else {
    sendHub(request, 400, false, "Unknown action.");
    return;
  }
  // The Hub task reads the key once at boot, so restart once the reply has gone out.
  request->onDisconnect(RestartService::restartNow);
  sendHub(request, 200, true, nullptr);
}

void WSoftStatusService::handleDjb(AsyncWebServerRequest* request, JsonVariant& json) {
  if (!scale_rs485()) {
    request->send(409,
                  "application/json",
                  "{\"error\":\"NO_CARD\",\"message\":\"No weighing card: the scale is on RS232.\"}");
    return;
  }
  if (!json.is<JsonObject>()) {
    request->send(400, "application/json", "{\"ok\":false,\"error\":\"json\"}");
    return;
  }
  JsonObject body = json.as<JsonObject>();
  const char* op = body["op"] | "";
  const char* field = body["field"] | "";
  bool has_channel = body.containsKey("channel");
  int channel = body["channel"] | 0;
  bool has_value = body.containsKey("value");
  long raw_value = 0;
  if (has_value) {
    if (!body["value"].is<long>() && !body["value"].is<int>()) {
      request->send(400, "application/json", "{\"ok\":false,\"error\":\"bad_value\"}");
      return;
    }
    raw_value = body["value"].as<long>();
  }

  WsoftDjbPlan plan;
  if (!wsoft_djb_plan(op, field, channel, has_channel, (int32_t)raw_value, has_value, &plan)) {
    char message[80];
    snprintf(message, sizeof message, "{\"ok\":false,\"error\":\"%s\"}", plan.error);
    request->send(400, "application/json", message);
    return;
  }

  WsoftDjbResult result;
  bool finished = wsoft_screen_djb(&plan, &result, 2000);
  int code = 200;
  if (!finished) {
    code = strcmp(result.error, "busy") == 0 ? 409 : 504;
  } else if (!result.ok && !result.sent) {
    code = 409;
  }

  AsyncJsonResponse* response = new AsyncJsonResponse(false, 768);
  JsonObject root = response->getRoot().to<JsonObject>();
  root["ok"] = finished && result.ok;
  root["sent"] = result.sent;
  if (result.error[0]) root["error"] = result.error;
  if (finished) {
    root["slave"] = result.slave;
    root["doc"] = result.doc;
    root["pdu"] = result.pdu;
    root["command"] = result.command;
    root["value"] = (long)result.value;
    root["request_hex"] = result.request_hex;
    root["reply_hex"] = result.reply_hex;
    if (result.exception) root["exception"] = result.exception;
    if (result.alarm_valid) {
      char hex[9];
      snprintf(hex, sizeof hex, "%08lX", (unsigned long)result.alarm_raw);
      root["alarm_raw"] = (long)result.value;
      root["alarm_raw_hex"] = hex;
      JsonObject bits = root.createNestedObject("alarm_bits");
      bits["parameter_changed"] = result.alarm.parameter_changed;
      bits["software_match"] = result.alarm.software_match;
      bits["adc_failure"] = result.alarm.adc_failure;
      bits["over_range"] = result.alarm.over_range;
      bits["eeprom"] = result.alarm.eeprom;
    }
  }
  response->setCode(code);
  response->setLength();
  request->send(response);
}

void WSoftStatusService::begin() {
  bool rs232 = _settingsService->source() == WSOFT_SOURCE_RS232;
  Serial.println(rs232 ? F("[WSoft] Starting W-Soft screen (RS232 scale via the Serial reader)...")
                       : F("[WSoft] Starting W-Soft screen (RS485 DJB on Serial1)..."));
  wsoft_screen_setup(rs232);
  xTaskCreatePinnedToCore(screenTask,
                          "wsoftScreen",
                          WSOFT_SCREEN_TASK_STACK,
                          nullptr,
                          WSOFT_SCREEN_TASK_PRIORITY,
                          nullptr,
                          WSOFT_SCREEN_TASK_CORE);
  _lastPublishMs = millis();
  _lastCleanupMs = millis();
  Serial.println(F("[WSoft] Screen task started"));
}

void WSoftStatusService::screenTask(void* param) {
  (void)param;
  for (;;) {
    wsoft_screen_loop();
    vTaskDelay(pdMS_TO_TICKS(WSOFT_SCREEN_TASK_DELAY_MS));
  }
}

void WSoftStatusService::loop() {
  unsigned long now = millis();

  if (now - _lastCleanupMs >= WSOFT_STATUS_WS_CLEANUP_MS) {
    _lastCleanupMs = now;
    _webSocket.cleanupClients();
  }

  if (now - _lastPublishMs < _settingsService->publishIntervalMs()) {
    return;
  }
  _lastPublishMs = now;

  WSoftSnapshot latest;
  wsoft_screen_snapshot(&latest);
  update(
      [&](WSoftStatus& status) {
        bool changed = !sameView(status.snapshot, latest);
        status.snapshot = latest;
        return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
      },
      "wsoft_screen");
}

void WSoftStatusService::configureMqtt() {
#if FT_ENABLED(FT_MQTT)
  String topic = _settingsService->mqttStatusTopic();
  String discovery = topic.length() ? "homeassistant/sensor/" + _settingsService->mqttUniqueId() + "/weight/config" : "";
  if (_mqttClient->connected()) {
    // An empty retained message removes the old state and the Home Assistant entity.
    if (_statusTopic.length() && _statusTopic != topic) {
      _mqttClient->publish(_statusTopic.c_str(), 0, true, "");
    }
    if (_discoveryTopic.length() && _discoveryTopic != discovery) {
      _mqttClient->publish(_discoveryTopic.c_str(), 0, true, "");
    }
    _statusTopic = topic;
    _discoveryTopic = discovery;
    if (topic.length()) publishDiscovery(topic);
    Serial.printf("[WSoft] MQTT status topic: %s\n", topic.length() ? topic.c_str() : "(off)");
  }
  _mqttPub.setPubTopic(topic);
#endif
}

#if FT_ENABLED(FT_MQTT)
// Home Assistant auto-discovery, like the LED example: one weight sensor in grams.
// weight_g is null when there is no valid, fresh weight, so the entity shows unknown.
void WSoftStatusService::publishDiscovery(const String& statusTopic) {
  DynamicJsonDocument doc(512);
  doc["name"] = _settingsService->mqttName();
  doc["unique_id"] = _settingsService->mqttUniqueId();
  doc["stat_t"] = statusTopic;
  doc["unit_of_meas"] = "g";
  doc["dev_cla"] = "weight";
  doc["stat_cla"] = "measurement";
  doc["val_tpl"] = "{{ value_json.weight_g }}";
  String payload;
  serializeJson(doc, payload);
  _mqttClient->publish(_discoveryTopic.c_str(), 0, true, payload.c_str());
}
#else
void WSoftStatusService::publishDiscovery(const String&) {}
#endif

#endif  // FT_WSOFT
