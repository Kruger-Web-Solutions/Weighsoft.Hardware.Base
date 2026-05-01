#ifndef SerialWriterService_h
#define SerialWriterService_h

#include <deque>

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <MqttPubSub.h>
#include <WebSocketTxRx.h>
#include <SettingValue.h>
#include <examples/serialwriter/SerialWriterState.h>

#define SERIALW_ENDPOINT_PATH    "/rest/serialWriter"
#define SERIALW_SOCKET_PATH      "/ws/serialWriter"
#define SERIALW_CONFIG_FILE      "/config/serialWriterConfig.json"

#define SERIALW_PORT             Serial1
#define SERIALW_RX_PIN           18
#define SERIALW_TX_PIN           17

// Source identifiers for transmitted lines (also surfaced in WS broadcast events)
enum class TxSource : uint8_t {
  NONE   = 0,
  MANUAL = 1,
  REST   = 2,
  WS     = 3,
  MQTT   = 4,
  READER = 5
};

class SerialWriterService : public StatefulService<SerialWriterState> {
 public:
  SerialWriterService(AsyncWebServer* server,
                      FS* fs,
                      SecurityManager* securityManager,
                      AsyncMqttClient* mqttClient);
  void begin();
  void loop();

  // Coordination with UartModeService — only the active mode owns Serial1.
  void suspendWriter();
  void resumeWriter();
  bool isSuspended() const { return _suspended; }

  // Single funnel for ALL outbound paths. Returns number of bytes written (0 on failure).
  size_t transmit(const String& data, TxSource source);

 private:
  HttpEndpoint<SerialWriterState>   _httpEndpoint;
  FSPersistence<SerialWriterState>  _fsPersistence;
  MqttPubSub<SerialWriterState>     _mqttPubSub;
  WebSocketTxRx<SerialWriterState>  _webSocket;
  AsyncMqttClient*                  _mqttClient;

  // Dedicated inbound WebSocket for receiving text frames to transmit over serial.
  // Mounted at /ws/serialWriter/inbound — separate from the state WS at /ws/serialWriter.
  AsyncWebSocket _inboundWs;

  AsyncWebServer*  _server;
  SecurityManager* _securityManager;

  bool _serialStarted = false;
  bool _suspended     = true;  // start suspended; UartModeService activates us only when in Writer mode

  // Inbound REST endpoint for { "data": "..." }
  void registerSendEndpoint();

  // Inbound WS handler for plain text or JSON {"data":"..."} frames
  void registerWsSink();

  // Inbound MQTT handler
  void onMqttMessage(const String& topic, const String& payload);
  void configureMqttSubscription();

  // Helpers
  void applySerialConfig();
  uint32_t serialMode() const;       // databits/parity/stopbits → ESP32 SERIAL_* constant
  String   lineEndingChars() const;  // returns "" / "\r" / "\n" / "\r\n"
  void     onConfigUpdated();
  void     broadcastTxEvent(const String& data, TxSource source);
};

#endif
