#ifndef LiveWeightService_h
#define LiveWeightService_h

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <MqttPubSub.h>
#include <WebSocketTxRx.h>
#include <SettingValue.h>
#include <examples/liveweight/LiveWeightState.h>

class RelayBoardService;

#define LIVE_WEIGHT_ENDPOINT_PATH "/rest/liveWeight"
#define LIVE_WEIGHT_SOCKET_PATH "/ws/liveWeight"
#define LIVE_WEIGHT_CONFIG_FILE "/config/liveWeight.json"

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
  RelayBoardService* _relayBoard;

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

  void configureMqtt();
  void onConfigUpdated();
  bool configChanged() const;
  bool sourceSettingsChanged() const;
  void syncAppliedConfig();
  void applySource();
  void readSerialLine();
  String extractWeight(const String& line);
  String extractWeightSimple(const String& line);
  void evaluateBandAndDrive(const String& originId);
  uint8_t computeZone(float weight) const;
  void runAction(const String& action);
  void sendNetworkPrint();
};

#endif
