#include <Features.h>
#if FT_ENABLED(FT_WSOFT)

#include <examples/wsoft/WSoftStatusService.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>

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
         a.trade_queue == b.trade_queue && a.hub_online == b.hub_online &&
         strncmp(a.hub_text, b.hub_text, sizeof a.hub_text) == 0;
}

bool sameSave(const SaveStatus& a, const SaveStatus& b) {
  return a.check == b.check && a.auto_on == b.auto_on && a.auto_stable_s == b.auto_stable_s &&
         a.last.seq == b.last.seq && a.queue == b.queue && a.hub_status == b.hub_status;
}

// "12.3 kg"
void weightText(int32_t value, int decimals, int unit, char* out, size_t size) {
  format_weight(value, decimals, out, size);
  const char* text = weight_unit_text(unit);
  if (text[0] != '\0') {
    strlcat(out, " ", size);
    strlcat(out, text, size);
  }
}

}  // namespace

void WSoftStatus::read(WSoftStatus& status, JsonObject& root) {
  JsonObjectSink sink(root);
  wsoft_view_write(status.snapshot, sink);

  const SaveStatus& save = status.save;
  root["save_can"] = save.can_save;
  root["save_reason"] = save_check_text(save.check);
  root["save_auto"] = save.auto_on;
  root["save_auto_s"] = save.auto_stable_s;
  root["save_last_seq"] = save.last.seq;
  if (save.last.valid) {
    char text[24];
    weightText(save.last.value, save.last.decimals, save.last.unit, text, sizeof text);
    root["save_last_text"] = String(text);
  } else {
    root["save_last_text"] = (const char*)nullptr;
  }
  root["save_queue"] = save.queue;
  root["save_hub_status"] = save.hub_status;
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
                  AuthenticationPredicates::IS_AUTHENTICATED,
                  WSOFT_STATUS_BUFFER_SIZE),
    _webSocket(WSoftStatus::read,
               this,
               server,
               WSOFT_STATUS_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_AUTHENTICATED,
               WSOFT_STATUS_BUFFER_SIZE),
#if FT_ENABLED(FT_MQTT)
    // Retained, so a subscriber that joins later still sees the current state.
    _mqttPub(WSoftStatus::read, this, mqttClient, "", true, WSOFT_STATUS_BUFFER_SIZE),
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
  // Any logged-in user may save, like pressing SAVE on the screen.
  server->on(WSOFT_SAVE_ENDPOINT_PATH,
             HTTP_POST,
             securityManager->wrapRequest(std::bind(&WSoftStatusService::handleSave, this, std::placeholders::_1),
                                          AuthenticationPredicates::IS_AUTHENTICATED));
  server->on(WSOFT_SAVE_ENDPOINT_PATH,
             HTTP_GET,
             securityManager->wrapRequest(std::bind(&WSoftStatusService::handleSaveList, this, std::placeholders::_1),
                                          AuthenticationPredicates::IS_AUTHENTICATED));
#if FT_ENABLED(FT_MQTT)
  _mqttClient->onConnect(std::bind(&WSoftStatusService::configureMqtt, this));
  // Topic or MQTT on/off changed on the settings page.
  _settingsService->addUpdateHandler([this](const String&) { configureMqtt(); }, false);
#else
  (void)mqttClient;
#endif
}

void WSoftStatusService::handleDjb(AsyncWebServerRequest* request, JsonVariant& json) {
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

void WSoftStatusService::handleSave(AsyncWebServerRequest* request) {
  SaveCheck check = SAVE_NO_SIGNAL;
  uint32_t seq = 0;
  if (!save_request(WSOFT_SAVE_WAIT_MS, &check, &seq)) {
    request->send(503, "application/json", "{\"ok\":false,\"reason\":\"busy\"}");
    return;
  }
  AsyncJsonResponse* response = new AsyncJsonResponse(false, 256);
  JsonObject root = response->getRoot().to<JsonObject>();
  bool saved = check == SAVE_OK && seq != 0;
  root["ok"] = saved;
  if (saved) {
    root["seq"] = seq;
    SaveStatus status;
    save_status(&status);
    if (status.last.valid && status.last.seq == seq) {
      char text[24];
      weightText(status.last.value, status.last.decimals, status.last.unit, text, sizeof text);
      root["weight_text"] = String(text);
    }
  } else {
    root["reason"] = save_check_text(check);
  }
  response->setCode(saved ? 200 : 409);
  response->setLength();
  request->send(response);
}

void WSoftStatusService::handleSaveList(AsyncWebServerRequest* request) {
  SavedWeighing recent[SAVE_RECENT_MAX];
  uint32_t queued_from = 0;
  size_t count = save_recent(recent, SAVE_RECENT_MAX, &queued_from);
  SaveStatus status;
  save_status(&status);

  AsyncJsonResponse* response = new AsyncJsonResponse(false, 2560);
  JsonObject root = response->getRoot().to<JsonObject>();
  root["queue"] = status.queue;
  root["hub_status"] = status.hub_status;
  JsonArray list = root.createNestedArray("recent");
  for (size_t i = 0; i < count; i++) {
    const SavedWeighing& w = recent[i];
    JsonObject item = list.createNestedObject();
    item["seq"] = w.seq;
    char text[24];
    weightText(w.value, w.decimals, w.unit, text, sizeof text);
    item["weight_text"] = String(text);
    char when[32];
    if (save_time_text(w.at, when, sizeof when)) {
      item["at"] = String(when);
    } else {
      item["at"] = (const char*)nullptr;
    }
    item["auto_saved"] = w.auto_saved;
    item["trade"] = w.trade;
    item["delivered"] = queued_from == 0 || w.seq < queued_from;
  }
  response->setLength();
  request->send(response);
}

void WSoftStatusService::begin() {
  Serial.println(F("[WSoft] Starting W-Soft screen (RS485 Serial1 RX15/TX16)..."));
  wsoft_screen_setup();
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
  SaveStatus latestSave;
  save_status(&latestSave);
  update(
      [&](WSoftStatus& status) {
        bool changed = !sameView(status.snapshot, latest) || !sameSave(status.save, latestSave);
        status.snapshot = latest;
        status.save = latestSave;
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
