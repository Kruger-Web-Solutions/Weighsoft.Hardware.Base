#ifndef SerialWriterService_h
#define SerialWriterService_h

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <MqttPubSub.h>
#include <WebSocketTxRx.h>
#include <SettingValue.h>
#include <examples/serialwriter/SerialWriterState.h>

#if FT_ENABLED(FT_BLE)
#include <BlePubSub.h>
#include <BLEServer.h>
#include <BLEService.h>
#include <BLECharacteristic.h>
#endif

#define SERIAL_WRITER_ENDPOINT_PATH "/rest/serialWriter"
#define SERIAL_WRITER_SOCKET_PATH "/ws/serialWriter"
#define SERIAL_WRITER_CONFIG_FILE "/config/serialWriterConfig.json"

// Bitmask for forwarder line delivery (see SerialWriterForwarderState output_targets)
#define SERIAL_WRITER_FORWARDER_SINK_USB (1u << 0)
#define SERIAL_WRITER_FORWARDER_SINK_SERIAL1 (1u << 1)

// Shares Serial1 with SerialService (same UART, opposite direction)
// ESP32-S3: GPIO18=U1RXD (RX), GPIO17=U1TXD (TX)
#define SERIAL_WRITER_PORT Serial1
#define SERIAL_WRITER_TX_PIN 17
#define SERIAL_WRITER_RX_PIN 18

class SerialWriterService : public StatefulService<SerialWriterState> {
 public:
  SerialWriterService(AsyncWebServer* server,
                      FS* fs,
                      SecurityManager* securityManager,
                      AsyncMqttClient* mqttClient
#if FT_ENABLED(FT_BLE)
                      ,
                      BLEServer* bleServer
#endif
  );

  void begin();
  void loop();

#if FT_ENABLED(FT_BLE)
  void setBleServer(BLEServer* bleServer) { _bleServer = bleServer; }
  void configureBle();
#endif

  // UART port coordination — called by UartModeService
  void suspendSerial();
  void resumeSerial();
  bool isSuspended() const { return _suspended; }

  /** Forwarder line delivery. @param sinkMask SERIAL_WRITER_FORWARDER_SINK_* bits. */
  void enqueueForwardedLine(const String& line, uint8_t sinkMask);

 private:
  HttpEndpoint<SerialWriterState> _httpEndpoint;
  FSPersistence<SerialWriterState> _fsPersistence;
  MqttPubSub<SerialWriterState> _mqttPubSub;
  WebSocketTxRx<SerialWriterState> _webSocket;
  AsyncMqttClient* _mqttClient;

  String _mqttBasePath;

#if FT_ENABLED(FT_BLE)
  BlePubSub<SerialWriterState> _blePubSub;
  BLEServer* _bleServer;
  BLEService* _bleService;
  BLECharacteristic* _bleCharacteristic;

  static constexpr const char* BLE_SERVICE_UUID = "12350000-e8f2-537e-4f6c-d104768a1235";
  static constexpr const char* BLE_CHAR_UUID = "12350001-e8f2-537e-4f6c-d104768a1235";
#endif

  bool _serialStarted;
  bool _suspended;

  /** Coalesce forwarder-driven UI updates to avoid WiFi/WebSocket overload at high line rates. */
  uint32_t _pendingForwarderSentUiDelta = 0;
  String _pendingForwarderLastSentLine;
  unsigned long _lastForwarderSentUiFlushMs = 0;
  void flushPendingForwarderSentUi(bool force);

  void writePendingLine();
  void writeLineWithTerminatorToUsbSerial(const String& line) const;
  void writeLineWithTerminatorToSerial1(const String& line) const;
  void applySerialConfig();
  uint32_t getSerialConfig();
  void onConfigUpdated();
  void configureMqtt();
};

#endif
