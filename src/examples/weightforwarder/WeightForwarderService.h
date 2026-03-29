#ifndef WeightForwarderService_h
#define WeightForwarderService_h

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <WebSocketTxRx.h>
#include <examples/weightforwarder/WeightForwarderState.h>
#include <examples/serial/SerialService.h>

#ifdef ESP32
#include <HTTPClient.h>
#include <WebSocketsClient.h>
#endif

#if FT_ENABLED(FT_BLE)
#include <BLEDevice.h>
#include <BLEClient.h>
#endif

#define WEIGHT_FORWARDER_ENDPOINT_PATH "/rest/weightForwarder"
#define WEIGHT_FORWARDER_SOCKET_PATH "/ws/weightForwarder"
#define WEIGHT_FORWARDER_CONFIG_FILE "/config/weightForwarderConfig.json"

class WeightForwarderService : public StatefulService<WeightForwarderState> {
 public:
  WeightForwarderService(AsyncWebServer* server,
                         FS* fs,
                         SecurityManager* securityManager,
                         AsyncMqttClient* mqttClient,
                         SerialService* serialService);
  void begin();
  void loop();

 private:
  HttpEndpoint<WeightForwarderState> _httpEndpoint;
  FSPersistence<WeightForwarderState> _fsPersistence;
  WebSocketTxRx<WeightForwarderState> _webSocket;
  AsyncMqttClient* _mqttClient;
  SerialService* _serialService;

#ifdef ESP32
  WebSocketsClient* _wsClient;
  unsigned long _lastWsReconnectAttempt;
#endif

#if FT_ENABLED(FT_BLE)
  BLEClient* _bleClient;
  BLERemoteCharacteristic* _bleRemoteChar;
  unsigned long _lastBleReconnectAttempt;
#endif

  unsigned long _lastForwardTime;
  static constexpr unsigned long MIN_FORWARD_INTERVAL = 500;  // 500ms = 2/sec max (prevents loop starvation)

  // HTTP auth per target URL (one JWT token cached per endpoint); not persisted
  String _httpAuthTokens[MAX_TARGET_URLS];
  bool _httpAuthTokensValid[MAX_TARGET_URLS];
  void clearAuthTokens();
  String getHttpBaseUrl(const String& targetUrl) const;
  bool fetchHttpAuthToken(const String& baseUrl, int idx);
  bool forwardViaHttpSingle(const String& url, int idx, const String& lastLine, const String& weight);

  void onConfigUpdated();
  void onSerialWeightUpdate(const String& originId);
  void forwardWeight(const String& lastLine, const String& weight);
  void formatJson(DynamicJsonDocument& doc, const String& lastLine, const String& weight, OutputFormat fmt);

  // Protocol-specific forwarding
  void forwardViaHttp(const String& lastLine, const String& weight);
  void forwardViaSerial(const String& lastLine, const String& weight);
  void forwardViaWs(const String& lastLine, const String& weight);
  void forwardViaMqtt(const String& lastLine, const String& weight);
  void forwardViaBle(const String& lastLine, const String& weight);

  // Protocol management
  void switchProtocol(ForwardProtocol newProtocol);
  void cleanupProtocol(ForwardProtocol protocol);
  void initWebSocketClient();
  void initBleClient();

#ifdef ESP32
  void checkWsConnection();
#endif

#if FT_ENABLED(FT_BLE)
  void checkBleConnection();
#endif
};

#endif
