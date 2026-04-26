#ifndef SerialWriterForwarderService_h
#define SerialWriterForwarderService_h

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <WebSocketTxRx.h>
#include <ArduinoJson.h>
#include <examples/serialwriter/SerialWriterForwarderState.h>
#include <examples/serialwriter/SerialWriterService.h>

#ifdef ESP32
#include <HTTPClient.h>
#include <WebSocketsClient.h>
#endif

#define SERIAL_WRITER_FORWARDER_ENDPOINT_PATH "/rest/serialWriterForwarder"
#define SERIAL_WRITER_FORWARDER_SOCKET_PATH "/ws/serialWriterForwarder"
#define SERIAL_WRITER_FORWARDER_CONFIG_FILE "/config/serialWriterForwarderConfig.json"

class SerialWriterForwarderService : public StatefulService<SerialWriterForwarderState> {
 public:
  SerialWriterForwarderService(AsyncWebServer* server,
                               FS* fs,
                               SecurityManager* securityManager,
                               SerialWriterService* serialWriterService);

  void begin();
  void loop();

 private:
  HttpEndpoint<SerialWriterForwarderState> _httpEndpoint;
  FSPersistence<SerialWriterForwarderState> _fsPersistence;
  WebSocketTxRx<SerialWriterForwarderState> _webSocket;
  SerialWriterService* _serialWriterService;

#ifdef ESP32
  WebSocketsClient* _wsClient;
  unsigned long _lastWsReconnectAttempt;
  uint32_t _wsAttemptCount;
  bool _wsHadConnected;
  uint32_t _wsReconnectDelayMs;
  int _lastSignInHttpCode;
  uint8_t _consecutiveSignInTransportFailures;
#endif

  unsigned long _lastPollTime;

  String _httpAuthToken;
  bool _httpAuthTokenValid;

  String getHttpBaseUrl(const String& url) const;
  bool fetchHttpAuthToken(const String& baseUrl);

  void onConfigUpdated();
  void pollHttp();
  void deliverLine(const String& line, const char* sourcePath = nullptr);
  void setRuntimeError(const String& errorText, bool connected);

  void initWsClient();
  void checkWsConnection();

#ifdef ESP32
  static const char* wsTypeName(WStype_t type);
#endif
};

#endif
