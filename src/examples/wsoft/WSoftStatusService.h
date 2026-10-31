#ifndef WSoftStatusService_h
#define WSoftStatusService_h

#include <Features.h>
#if FT_ENABLED(FT_WSOFT)

// Live W-Soft state for the web page, WebSocket and MQTT (read-only, logged-in users).
// The W-Soft screen loop runs in its own FreeRTOS task (core 1, 8 KB stack, 5 ms
// pause between passes) and publishes a snapshot after every DJB poll. This service
// copies that snapshot in loop() and pushes it out when it changes, at most once per
// publish interval (WSoftSettingsService).
//
// Approval is switched only with a USB cable (APPROVAL ON/OFF).
// POST /rest/djb (admin) reads and writes DJB registers. The screen task owns Serial1
// and runs that one transaction; this service only queues it.
//
// Saving a weighing (lib/wsoft_esp/src/save_queue.h), any logged-in user:
//   POST /rest/wsoftSave  same as the SAVE button on the screen. 200 {ok,seq,weight_text} when
//                         saved, 409 {ok:false,reason} when not (moving, empty, saved, ...),
//                         503 when the screen task did not answer (e.g. firmware upload).
//   GET  /rest/wsoftSave  the last 10 saved weighings, newest first, and the queue size.
// The live status (WebSocket/MQTT) carries save_can, save_reason, save_last_seq, save_queue, ...

#include <HttpEndpoint.h>
#include <WebSocketTxRx.h>
#if FT_ENABLED(FT_MQTT)
#include <MqttPubSub.h>
#endif

#include <save_queue.h>
#include <wsoft_view.h>

#include <examples/wsoft/WSoftSettingsService.h>

class AsyncMqttClient;

#define WSOFT_STATUS_ENDPOINT_PATH "/rest/wsoftStatus"
#define WSOFT_DJB_ENDPOINT_PATH "/rest/djb"
#define WSOFT_SAVE_ENDPOINT_PATH "/rest/wsoftSave"
#define WSOFT_SAVE_WAIT_MS 1500
// The view plus the save fields; the framework default (1024) leaves little room.
#define WSOFT_STATUS_BUFFER_SIZE 1536
#define WSOFT_STATUS_SOCKET_PATH "/ws/wsoftStatus"
#define WSOFT_STATUS_WS_CLEANUP_MS 5000UL

#define WSOFT_SCREEN_TASK_STACK 8192
#define WSOFT_SCREEN_TASK_CORE 1
#define WSOFT_SCREEN_TASK_PRIORITY 1
#define WSOFT_SCREEN_TASK_DELAY_MS 5

class WSoftStatus {
 public:
  WSoftSnapshot snapshot;
  SaveStatus save;

  static void read(WSoftStatus& status, JsonObject& root);
};

class WSoftStatusService : public StatefulService<WSoftStatus> {
 public:
  WSoftStatusService(AsyncWebServer* server,
                     SecurityManager* securityManager,
                     AsyncMqttClient* mqttClient,
                     WSoftSettingsService* settingsService);

  // Runs the W-Soft screen setup (Serial1 RS485 on RX15/TX16, panel, approval, Hub)
  // and then starts the screen task.
  void begin();
  void loop();

 private:
  HttpGetEndpoint<WSoftStatus> _httpEndpoint;
  WebSocketTx<WSoftStatus> _webSocket;
#if FT_ENABLED(FT_MQTT)
  MqttPub<WSoftStatus> _mqttPub;
  AsyncMqttClient* _mqttClient;
  // Last topics published to, so they can be cleared when MQTT is switched off or moved.
  String _statusTopic;
  String _discoveryTopic;
#endif
  WSoftSettingsService* _settingsService;
  unsigned long _lastPublishMs = 0;
  unsigned long _lastCleanupMs = 0;

  void configureMqtt();
  void publishDiscovery(const String& statusTopic);
  void handleDjb(AsyncWebServerRequest* request, JsonVariant& json);
  void handleSave(AsyncWebServerRequest* request);
  void handleSaveList(AsyncWebServerRequest* request);
  static void screenTask(void* param);
};

#endif  // FT_WSOFT
#endif
