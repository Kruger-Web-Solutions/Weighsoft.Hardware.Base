#ifndef SerialWriterForwarderService_h
#define SerialWriterForwarderService_h

#include <WebSocketsClient.h>
#include <HTTPClient.h>

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

  void onConfigChanged();
  void connectWs();
  void disconnectWs();
  void onWsEvent(WStype_t type, uint8_t* payload, size_t length);
  void scheduleRetry();
  void announce();
  void pollReaderRest();
  String fetchReaderAccessToken(const String& host, uint16_t port);
};

#endif
