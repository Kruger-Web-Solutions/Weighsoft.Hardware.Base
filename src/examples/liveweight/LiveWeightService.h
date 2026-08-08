#ifndef LiveWeightService_h
#define LiveWeightService_h

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <MqttPubSub.h>
#include <WebSocketTxRx.h>
#include <SettingValue.h>
#include <examples/liveweight/LiveWeightState.h>

#define LIVE_WEIGHT_ENDPOINT_PATH "/rest/liveWeight"
#define LIVE_WEIGHT_SOCKET_PATH "/ws/liveWeight"
#define LIVE_WEIGHT_CONFIG_FILE "/config/liveWeight.json"

class LiveWeightService : public StatefulService<LiveWeightState> {
 public:
  LiveWeightService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager, AsyncMqttClient* mqttClient);

  void begin();
  void loop();

 private:
  HttpEndpoint<LiveWeightState> _httpEndpoint;
  FSPersistence<LiveWeightState> _fsPersistence;
  MqttPubSub<LiveWeightState> _mqttPubSub;
  WebSocketTxRx<LiveWeightState> _webSocket;
  AsyncMqttClient* _mqttClient;

  String _mqttBasePath;
  String _lineBuffer;
  bool _serialConfigured;
  LiveWeightSource _appliedSource;
  uint32_t _appliedBaud;
  String _appliedRegex;
  bool _appliedRs485Enabled;
  uint8_t _appliedRs485Address;

  void configureMqtt();
  void onConfigUpdated();
  bool configChanged() const;
  void applySource();
  void readSerialLine();
  String extractWeight(const String& line);
  String extractWeightSimple(const String& line);
};

#endif
