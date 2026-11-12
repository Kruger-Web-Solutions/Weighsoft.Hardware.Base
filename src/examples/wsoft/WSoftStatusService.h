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
// Approval and Auto save are switched in WSoftSwitchesService (and on the screen, the Hub
// and the USB line), all through switches.h.
// GET/POST /rest/wsoftHub (admin) shows the Hub link and connects or disconnects it with a
// Hub pairing key; the board restarts after a change. The key is never sent back.
// GET /rest/wsoftSave (logged in) shows the weighings waiting for the Hub; POST saves the
// load on the scale now, like the SAVE button on the board's screen.
// GET /rest/wsoftFeed and the /ws/wsoftFeed socket (open, read-only, like the Serial reader's
// /rest/serial and /ws/serial): the weight for a Writer board (wsoft_feed_write), pushed every
// WSOFT_FEED_MS while a Writer listens.
// POST /rest/djb (admin) reads and writes the weighing card's registers. The screen task owns
// Serial1 and runs that one transaction; this service only queues it. Refused with 409 NO_CARD
// on RS232; every write that goes through adds 1 to the scale setup audit counter.

#include <HttpEndpoint.h>
#include <WebSocketTxRx.h>
#if FT_ENABLED(FT_MQTT)
#include <MqttPubSub.h>
#endif

#include <wsoft_view.h>

#include <examples/wsoft/WSoftSettingsService.h>

class AsyncMqttClient;

#define WSOFT_STATUS_ENDPOINT_PATH "/rest/wsoftStatus"
#define WSOFT_DJB_ENDPOINT_PATH "/rest/djb"
#define WSOFT_HUB_ENDPOINT_PATH "/rest/wsoftHub"
#define WSOFT_SAVE_ENDPOINT_PATH "/rest/wsoftSave"
#define WSOFT_STATUS_SOCKET_PATH "/ws/wsoftStatus"
#define WSOFT_STATUS_WS_CLEANUP_MS 5000UL
#define WSOFT_FEED_ENDPOINT_PATH "/rest/wsoftFeed"
#define WSOFT_FEED_SOCKET_PATH "/ws/wsoftFeed"
#define WSOFT_FEED_MS 500UL

#define WSOFT_SCREEN_TASK_STACK 8192
#define WSOFT_SCREEN_TASK_CORE 1
#define WSOFT_SCREEN_TASK_PRIORITY 1
#define WSOFT_SCREEN_TASK_DELAY_MS 5

class WSoftStatus {
 public:
  WSoftSnapshot snapshot;

  static void read(WSoftStatus& status, JsonObject& root);
};

class WSoftStatusService : public StatefulService<WSoftStatus> {
 public:
  WSoftStatusService(AsyncWebServer* server,
                     SecurityManager* securityManager,
                     AsyncMqttClient* mqttClient,
                     WSoftSettingsService* settingsService);

  // Runs the W-Soft screen setup (RS485 on Serial1, or the RS232 scale through the Serial
  // reader when the settings say so; panel, approval, Hub)
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
  AsyncWebSocket _feedSocket;
  unsigned long _lastFeedMs = 0;

  void configureMqtt();
  static String feedJson();
  void publishDiscovery(const String& statusTopic);
  void handleDjb(AsyncWebServerRequest* request, JsonVariant& json);
  void readHub(AsyncWebServerRequest* request);
  void readSave(AsyncWebServerRequest* request);
  void save(AsyncWebServerRequest* request);
  void updateHub(AsyncWebServerRequest* request, JsonVariant& json);
  static void screenTask(void* param);
};

#endif  // FT_WSOFT
#endif
