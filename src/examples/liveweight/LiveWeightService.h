#ifndef LiveWeightService_h
#define LiveWeightService_h

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <MqttPubSub.h>
#include <WebSocketTxRx.h>
#include <SettingValue.h>
#include <examples/liveweight/LiveWeightState.h>
#include <examples/liveweight/LiveWeightDiscovery.h>

class RelayBoardService;

#define LIVE_WEIGHT_ENDPOINT_PATH "/rest/liveWeight"
#define LIVE_WEIGHT_SOCKET_PATH "/ws/liveWeight"
#define LIVE_WEIGHT_CONFIG_FILE "/config/liveWeight.json"
#define LIVE_WEIGHT_PRODUCTS_FILE "/config/products.json"
#define LIVE_WEIGHT_TX_FILE "/log/transactions.ndjson"
#define LIVE_WEIGHT_PRODUCTS_PATH "/rest/liveWeightProducts"
#define LIVE_WEIGHT_TX_PATH "/rest/liveWeightTransactions"
#define LIVE_WEIGHT_DISCOVERY_REST_PATH LIVE_WEIGHT_DISCOVERY_PATH
#define LIVE_WEIGHT_MAX_PRODUCTS 9
#define LIVE_WEIGHT_MAX_TX 40

// Serial ingest caps (ESP8266-safe): 128 B lines, ≤5 Hz state publish, drain-bounded
#define LIVE_WEIGHT_LINE_MAX 128
#define LIVE_WEIGHT_PUBLISH_MIN_MS 200
#define LIVE_WEIGHT_SERIAL_BYTES_PER_LOOP 64

struct LiveWeightProductEntry {
  String plu;
  String product;
  String unit;
};

class LiveWeightService : public StatefulService<LiveWeightState> {
 public:
  LiveWeightService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager, AsyncMqttClient* mqttClient);

  void begin();
  void loop();
  void setRelayBoardService(RelayBoardService* relayBoard);
  // DI rising edge from RelayBoardService (diIndex 1 or 2, active=true on press)
  void onDiEdge(uint8_t diIndex, bool active);

 private:
  HttpEndpoint<LiveWeightState> _httpEndpoint;
  FSPersistence<LiveWeightState> _fsPersistence;
  MqttPubSub<LiveWeightState> _mqttPubSub;
  WebSocketTxRx<LiveWeightState> _webSocket;
  AsyncMqttClient* _mqttClient;
  AsyncWebServer* _server;
  SecurityManager* _securityManager;
  FS* _fs;
  RelayBoardService* _relayBoard;
  LiveWeightDiscovery _discovery;
  LiveWeightProductEntry _products[LIVE_WEIGHT_MAX_PRODUCTS];
  uint8_t _productCount;

  String _mqttBasePath;
  String _lineBuffer;
  bool _serialConfigured;
  LiveWeightSource _appliedSource;
  uint32_t _appliedBaud;
  String _appliedRegex;
  bool _appliedRs485Enabled;
  uint8_t _appliedRs485Address;
  bool _appliedRangeEnabled;
  float _appliedRangeLow;
  float _appliedRangeHigh;
  uint8_t _appliedRelayLow;
  uint8_t _appliedRelayOk;
  uint8_t _appliedRelayHigh;
  String _appliedPlu;
  String _appliedProduct;
  uint32_t _appliedCount;
  String _appliedUnit;
  String _appliedDi1Action;
  String _appliedDi2Action;
  bool _appliedPrinterEnabled;
  String _appliedPrinterIp;
  uint16_t _appliedPrinterPort;
  uint8_t _lastDrivenZone;
  String _pendingAction;
  bool _printPending;
  unsigned long _lastSerialPublishMs;

  void configureMqtt();
  void onConfigUpdated();
  bool configChanged() const;
  bool sourceSettingsChanged() const;
  void syncAppliedConfig();
  void applySource();
  void readSerialLine();
  void invalidateRegexCache();
  bool ensureRegexCompiled(const String& pattern);
  String extractWeight(const String& line);
  String extractWeightSimple(const String& line);
  void evaluateBandAndDrive(const String& originId);
  uint8_t computeZone(float weight) const;
  void runAction(const String& action);
  void sendNetworkPrint();
  void registerCatalogEndpoints();
  void loadProducts();
  bool saveProducts();
  int findProductIndex(const String& plu) const;
  void appendTransaction(const char* reason);
};

#endif
