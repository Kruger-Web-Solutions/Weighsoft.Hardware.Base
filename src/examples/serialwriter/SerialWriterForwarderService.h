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
  String        _readerAccessToken;
  unsigned long _lastPollMs = 0;
  String        _serialRestUrl;
  uint8_t       _readerPollFailures = 0;
  unsigned long _lastReaderOkMs = 0;

  void onConfigChanged();
  void connectReader();
  void disconnectWs();
  void onWiFiGotIP();
  uint8_t _activeConnectionMethod = SERIALW_FWD_METHOD_WS;
  bool _wifiWasConnected = false;

  void onWsEvent(WStype_t type, uint8_t* payload, size_t length);
  void scheduleRetry();
  void announce();
  void pollReaderRest();
  void markReaderOk();
  void handleReaderPollFailure(const String& message);
  String fetchReaderAccessToken(const String& host, uint16_t port);
};

#endif
