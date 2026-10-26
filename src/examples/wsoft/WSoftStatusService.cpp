#include <Features.h>
#if FT_ENABLED(FT_WSOFT)

#include <examples/wsoft/WSoftStatusService.h>

#include <limits.h>

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
#if FT_ENABLED(FT_MQTT)
  _mqttClient->onConnect(std::bind(&WSoftStatusService::configureMqtt, this));
  // Topic or MQTT on/off changed on the settings page.
  _settingsService->addUpdateHandler([this](const String&) { configureMqtt(); }, false);
#else
  (void)mqttClient;
#endif
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
  _mqttPub.setPubTopic(topic);
  if (_mqttClient->connected()) {
    Serial.printf("[WSoft] MQTT status topic: %s\n", topic.length() ? topic.c_str() : "(off)");
  }
#endif
}

#endif  // FT_WSOFT
