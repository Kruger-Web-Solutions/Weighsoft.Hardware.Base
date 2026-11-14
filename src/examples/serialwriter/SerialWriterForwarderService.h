#ifndef SerialWriterForwarderService_h
#define SerialWriterForwarderService_h

#include <WebSocketsClient.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <SettingValue.h>
#include <examples/serialwriter/SerialWriterForwarderState.h>

class SerialWriterService;

#define SERIALW_FWD_ENDPOINT_PATH "/rest/serialWriterSource"
#define SERIALW_FWD_CONFIG_FILE   "/config/serialWriterSource.json"

class SerialWriterForwarderService : public StatefulService<SerialWriterForwarderState> {
 public:
  SerialWriterForwarderService(AsyncWebServer* server,
                               FS* fs,
                               SecurityManager* securityManager,
                               SerialWriterService* writerService);
  void begin();
  void loop();

  // Called by UartModeService when entering / leaving Writer mode.
  void start();
  void stop();

  // The newest weight received (the number only), the millis() it arrived and its DJB unit
  // code (1 kg, 2 t, 3 g; left alone when the source sent no unit). False before the first
  // weight. Safe from another task (the W-Soft screen task in Writer mode).
  bool latestWeight(char* weight, size_t size, uint32_t* receivedMs, int* unit);
  // The weigh board's product code ("" = none). False when the source sends no product.
  bool latestProduct(String* code);

 private:
  HttpEndpoint<SerialWriterForwarderState>  _httpEndpoint;
  FSPersistence<SerialWriterForwarderState> _fsPersistence;

  SerialWriterService* _writerService;
  WebSocketsClient     _wsClient;

  bool          _running = false;
  unsigned long _nextRetryMs = 0;
  uint16_t      _backoffMs   = 1000;  // 1s, 2s, 4s, ..., capped at 30s

  unsigned long _lastAnnounceMs = 0;
  String        _announceUrl;   // e.g. "http://192.168.2.50/rest/writers/announce"
  unsigned long _lastPollMs = 0;
  String        _serialRestUrl;
  uint8_t       _readerPollFailures = 0;
  unsigned long _lastReaderOkMs = 0;
  // A W-Soft weigh board's /ws/wsoftFeed pushes every 0.5 s, so a quiet socket is a dead one:
  // _lastWsDataMs is the last socket message (REST backup polls do not count), _wsoftFeed
  // says the source is such a feed.
  unsigned long _lastWsDataMs = 0;
  bool          _wsoftFeed = false;

  void onConfigChanged();
  void connectReader();
  void disconnectWs();
  void onWiFiGotIP();
  uint32_t _lastTimestamp = 0;  // the source's timestamp of the last line handled (0 = none)
  uint8_t _activeConnectionMethod = SERIALW_FWD_METHOD_WS;
  bool _wifiWasConnected = false;

  void onWsEvent(WStype_t type, uint8_t* payload, size_t length);
  void scheduleRetry();
  void announce();
  void pollReaderRest();
  void markReaderOk();
  void handleReaderPollFailure(const String& message);
  void onReaderData(JsonObject data);
};

#endif
