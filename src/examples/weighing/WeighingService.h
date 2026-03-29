#ifndef WeighingService_h
#define WeighingService_h

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <MqttPubSub.h>
#include <WebSocketTxRx.h>
#include <SettingValue.h>
#include <ESPAsyncWebServer.h>

#include <examples/weighing/WeighingState.h>
#include <examples/weighing/ADS1231.h>
#include <examples/weighing/AuditTrail.h>

#if FT_ENABLED(FT_BLE)
#include <BlePubSub.h>
#include <BLEServer.h>
#include <BLEService.h>
#include <BLECharacteristic.h>
#endif

#define WEIGHING_ENDPOINT_PATH  "/rest/weighing"
#define WEIGHING_SOCKET_PATH    "/ws/weighing"
#define WEIGHING_AUDIT_PATH     "/rest/weighingAudit"
#define WEIGHING_CONFIG_FILE    "/config/weighing.json"

// Stability detection: rolling buffer of N readings
#define WEIGHING_STABILITY_SAMPLES 8

class WeighingService : public StatefulService<WeighingState> {
 public:
  WeighingService(AsyncWebServer*   server,
                  FS*               fs,
                  SecurityManager*  securityManager,
                  AsyncMqttClient*  mqttClient
#if FT_ENABLED(FT_BLE)
                  , BLEServer* bleServer
#endif
                  );

  void begin();
  void loop();

#if FT_ENABLED(FT_BLE)
  void setBleServer(BLEServer* bleServer) { _bleServer = bleServer; }
  void configureBle();
#endif

 private:
  HttpEndpoint<WeighingState>    _httpEndpoint;
  FSPersistence<WeighingState>   _fsPersistence;
  MqttPubSub<WeighingState>      _mqttPubSub;
  WebSocketTxRx<WeighingState>   _webSocket;
  AsyncMqttClient*               _mqttClient;
  AsyncWebServer*                _server;
  SecurityManager*               _securityManager;

  ADS1231    _adc;
  AuditTrail _auditTrail;

  // MQTT inline config
  String _mqttBasePath;
  String _mqttName;
  String _mqttUniqueId;

#if FT_ENABLED(FT_BLE)
  BlePubSub<WeighingState> _blePubSub;
  BLEServer*               _bleServer;
  BLEService*              _bleService;
  BLECharacteristic*       _bleCharacteristic;
  static constexpr const char* BLE_SERVICE_UUID = "56780000-e8f2-537e-4f6c-d104768a5678";
  static constexpr const char* BLE_CHAR_UUID    = "56780001-e8f2-537e-4f6c-d104768a5678";
#endif

  // Internal state for loop()
  unsigned long _lastAztTime;
  float         _stabilityBuf[WEIGHING_STABILITY_SAMPLES];
  uint8_t       _stabilityIdx;
  bool          _stabilityFull;
  bool          _powerOnZeroDone;

  // MQTT helpers
  void configureMqtt();

  // Called when a channel triggers an update (config change or command)
  void onCommandReceived(const String& originId);

  // Weighing operations (called from onCommandReceived)
  void executeTare();
  void executePresetTare(float value);
  void executeClearTare();
  void executeZero();
  void executeCalibration(float knownWeight);
  void executePowerOnZero();
  void executeSeal();
  void executeUnseal(const String& pin);

  // Automatic Zero Tracking
  void performAzt(float grossWeight);

  // Stability detection
  bool computeStability(float weight);

  // Audit helpers
  void logAndCount(const String& event,
                   const String& param,
                   const String& oldVal,
                   const String& newVal);

  // Persist config to flash
  void saveConfig();
};

#endif
