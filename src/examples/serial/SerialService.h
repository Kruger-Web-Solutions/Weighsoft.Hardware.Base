#ifndef SerialService_h
#define SerialService_h

#include <deque>

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <MqttPubSub.h>
#include <WebSocketTxRx.h>
#include <SettingValue.h>
#include <examples/serial/SerialState.h>

// Forward declaration — full definition included in SerialService.cpp
class KnownWritersService;

#if FT_ENABLED(FT_BLE)
#include <BlePubSub.h>
#include <BLEServer.h>
#include <BLEService.h>
#include <BLECharacteristic.h>
#endif

#define SERIAL_ENDPOINT_PATH "/rest/serial"
#define SERIAL_SOCKET_PATH "/ws/serial"
#define SERIAL_CONFIG_FILE "/config/serialConfig.json"

// ESP32 Serial1 pins — product standard: GPIO18 (RX), GPIO17 (TX) on all ESP32 targets in this repo
#define SERIAL_PORT Serial1
#define SERIAL2_RX_PIN 18
#define SERIAL2_TX_PIN 17

// Completed UART lines queued here; `loop()` publishes at most this many per second to REST/WS/MQTT.
#ifndef SERIAL_MAX_COMPLETE_LINES_PER_SEC
#define SERIAL_MAX_COMPLETE_LINES_PER_SEC 4
#endif
#ifndef SERIAL_MAX_QUEUED_COMPLETE_LINES
#define SERIAL_MAX_QUEUED_COMPLETE_LINES 32
#endif

class SerialService : public StatefulService<SerialState> {
 public:
  SerialService(AsyncWebServer* server,
                FS* fs,
                SecurityManager* securityManager,
                AsyncMqttClient* mqttClient
#if FT_ENABLED(FT_BLE)
                ,BLEServer* bleServer
#endif
                );
  void begin();
  void loop();  // Must be called in main loop() to read serial

  // Wires the KnownWriters tracking service so scale-line broadcasts update writer timestamps.
  // NOTE: WS connect/disconnect auto-registration requires a framework callback not currently
  // exposed by WebSocketTxRx — call onWriterConnected/onWriterDisconnected manually or extend
  // the framework in a future task (DONE_WITH_CONCERNS: Task 3, Plan 2).
  void setKnownWritersService(KnownWritersService* svc) { _knownWriters = svc; }

#if FT_ENABLED(FT_BLE)
  void setBleServer(BLEServer* bleServer) { _bleServer = bleServer; }
  void configureBle();
#endif

 private:
  KnownWritersService* _knownWriters = nullptr;
  // GET is open for device-to-device reads (Writer poll); POST stays authenticated.
  HttpGetEndpoint<SerialState>    _httpGetEndpoint;
  HttpPostEndpoint<SerialState>  _httpPostEndpoint;
  FSPersistence<SerialState> _fsPersistence;
  MqttPubSub<SerialState> _mqttPubSub;
  WebSocketTxRx<SerialState> _webSocket;
  AsyncMqttClient* _mqttClient;

  // Inline MQTT configuration - single-layer pattern
  String _mqttBasePath;
  String _mqttName;
  String _mqttUniqueId;

#if FT_ENABLED(FT_BLE)
  BlePubSub<SerialState> _blePubSub;
  BLEServer* _bleServer;
  BLEService* _bleService;
  BLECharacteristic* _bleCharacteristic;

  // Inline BLE UUIDs - single-layer pattern
  static constexpr const char* BLE_SERVICE_UUID = "12340000-e8f2-537e-4f6c-d104768a1234";
  static constexpr const char* BLE_CHAR_UUID = "12340001-e8f2-537e-4f6c-d104768a1234";
#endif

  String _lineBuffer;   // Accumulates serial data until newline
  bool _serialStarted;  // True after first begin(), so we can call end() before reconfig
  bool _suspended;      // True when DiagnosticsService is using Serial1

  std::deque<String> _rxCompleteLineQueue;
  unsigned long _lastRxLinePublishMs{0};

  void enqueueCompletedLine(const String& line);
  void drainRxLineQueue();

  void configureMqtt();
  void readSerial();       // Called from loop()
  void applySerialConfig();  // Reconfigures Serial1 with current state
  String extractWeight(const String& line);  // Extracts first capture group from regex pattern
  uint32_t getSerialConfig();  // Converts databits/parity/stopbits to ESP32 config constant
  void onConfigUpdated();  // Called when config changes (e.g. from REST POST)

 public:
  // Coordination with DiagnosticsService
  void suspendSerial();    // Stop Serial1, allow DiagnosticsService to use it
  void resumeSerial();     // Restart Serial1 after DiagnosticsService releases it
  bool isSuspended() const { return _suspended; }
};

#endif
