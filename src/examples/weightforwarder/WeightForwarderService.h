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
// Audit endpoint deliberately uses a sibling path (NOT a sub-path) so it
// won't collide with HttpEndpoint's prefix matching for /rest/weightForwarder.
#define WEIGHT_FORWARDER_AUDIT_PATH "/rest/weightForwarderAudit"
#define WEIGHT_FORWARDER_CONFIG_FILE "/config/weightForwarderConfig.json"

// Heap-pressure guard. If free heap drops below this, drop the forward
// attempt rather than allocating an HTTPClient + JSON doc + WS broadcast
// buffer. Empirically the device starts failing ~8-10 KB free; 6 KB gives
// the guard headroom to reject before OOM without rejecting normal traffic.
#define WEIGHT_FORWARDER_MIN_FREE_HEAP 6000

// Audit ring-buffer capacity. 20 entries is enough to spot a leak / drop
// burst without burning static RAM.
#define WEIGHT_FORWARDER_AUDIT_CAPACITY 20

enum class WeightForwarderAuditReason : uint8_t {
  PERIODIC = 0,           // periodic 30 s heap snapshot
  FORWARD_OK = 1,         // at least one target accepted the forward
  FORWARD_DROPPED = 2,    // forward skipped due to heap pressure
  TARGET_FAILED = 3,      // a single target POST failed
  WS_RECONNECT = 4,       // outbound WS client reconnected
  BOOT = 5,               // service initialised
};

struct WeightForwarderAuditEntry {
  uint32_t millis_at;
  uint32_t free_heap;
  uint32_t min_heap;
  uint32_t max_alloc;
  uint16_t fwd_total;
  uint16_t fwd_dropped;
  uint16_t fwd_failed;
  uint8_t  reason;
  uint8_t  ws_clients;
};

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
  AsyncWebServer* _server;
  SecurityManager* _securityManager;
  HttpEndpoint<WeightForwarderState> _httpEndpoint;
  FSPersistence<WeightForwarderState> _fsPersistence;
  WebSocketTxRx<WeightForwarderState> _webSocket;
  AsyncMqttClient* _mqttClient;
  SerialService* _serialService;

  // Audit ring buffer + counters (mirrors RemoteWeightService).
  WeightForwarderAuditEntry _audit[WEIGHT_FORWARDER_AUDIT_CAPACITY];
  uint8_t _auditHead = 0;
  uint8_t _auditCount = 0;
  uint16_t _fwdTotal = 0;
  uint16_t _fwdDropped = 0;
  uint16_t _fwdFailed = 0;
  uint32_t _minHeapSeen = UINT32_MAX;
  unsigned long _lastPeriodicMillis = 0;

  void recordAudit(WeightForwarderAuditReason reason);
  void registerForwardOk();
  void registerForwardDropped();
  void registerTargetFailed();
  void registerAuditEndpoint();
  static void readAuditAndStats(WeightForwarderService* self, JsonObject& root);

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
  static constexpr unsigned long MIN_FORWARD_INTERVAL = 500;
  static constexpr unsigned long WEIGHT_DEBOUNCE_MS = 300;
  static constexpr int HTTP_CONNECT_TIMEOUT_MS = 800;
  // Bumped from 1500 -> 3000 ms so we stop reporting false-negative failures.
  // Empirically the Remote receives the POST and processes it fine, but a
  // congested WiFi can take >1.5s to return the 200 OK. The previous timeout
  // counted those as "failed" even though the data was already on the Remote
  // (Forwarder fwd_total=103 vs Remote posts_total=150 over the same window).
  static constexpr int HTTP_RESPONSE_TIMEOUT_MS = 3000;
  static constexpr int TARGET_FAIL_BACKOFF_MS = 30000;
  static constexpr int TARGET_FAIL_THRESHOLD = 3;

  // Debounce: store latest weight, only forward after it stabilises
  String _pendingWeight;
  String _pendingLine;
  unsigned long _weightLastChangedAt;
  String _lastForwardedWeight;

  // HTTP auth per target URL
  String _httpAuthTokens[MAX_TARGET_URLS];
  bool _httpAuthTokensValid[MAX_TARGET_URLS];
  // Per-target failure tracking to skip offline targets
  int _targetFailCount[MAX_TARGET_URLS];
  unsigned long _targetNextRetry[MAX_TARGET_URLS];
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
