#ifndef SerialService_h
#define SerialService_h

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <MqttPubSub.h>
#include <WebSocketTxRx.h>
#include <SettingValue.h>
#include <regex.h>
#include <examples/serial/SerialState.h>

#if FT_ENABLED(FT_BLE)
#include <BlePubSub.h>
#include <BLEServer.h>
#include <BLEService.h>
#include <BLECharacteristic.h>
#endif

#define SERIAL_ENDPOINT_PATH "/rest/serial"
#define SERIAL_SOCKET_PATH "/ws/serial"
#define SERIAL_CONFIG_FILE "/config/serialConfig.json"

// ESP32 Serial1 pins
// Default: GPIO18=RX, GPIO17=TX (ESP32-S3 UART1 defaults)
// Override via build flags: -DSERIAL2_RX_PIN=16 -DSERIAL2_TX_PIN=17
// esp32dev (display board) uses GPIO16 for RX to free GPIO18 for SPI SCK
#define SERIAL_PORT Serial1
#ifndef SERIAL2_RX_PIN
#define SERIAL2_RX_PIN 18
#endif
#ifndef SERIAL2_TX_PIN
#define SERIAL2_TX_PIN 17
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

#if FT_ENABLED(FT_BLE)
  void setBleServer(BLEServer* bleServer) { _bleServer = bleServer; }
  void configureBle();
#endif

 private:
  HttpEndpoint<SerialState> _httpEndpoint;
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
  bool _usbEchoEnabled; // Mirror every UART1 line to Serial (USB-CDC) + Serial0 (UART0)
                        // independently of state-change broadcasts. Set by
                        // WeightForwarderService at config-load + on every
                        // /rest/weightForwarder POST.

  // Cached compiled regex to avoid recompiling on every serial line
  regex_t _compiledRegex;
  bool _regexCompiled;
  String _compiledPattern;
  void compileRegex(const String& pattern);
  void freeRegex();

  void configureMqtt();
  void readSerial();       // Called from loop()
  void applySerialConfig();  // Reconfigures Serial1 with current state
  String extractWeight(const String& line);  // Uses cached compiled regex
  uint32_t getSerialConfig();  // Converts databits/parity/stopbits to ESP32 config constant
  void onConfigUpdated();  // Called when config changes (e.g. from REST POST)

 public:
  // Coordination with DiagnosticsService
  void suspendSerial();    // Stop Serial1, allow DiagnosticsService to use it
  void resumeSerial();     // Restart Serial1 after DiagnosticsService releases it
  bool isSuspended() const { return _suspended; }

  // USB-CDC / UART0 echo — driven from WeightForwarderService config.
  // Echo fires inside SerialService::loop() on every line read so ScaleCOM
  // sees a continuous stream even when the weight value is unchanged.
  void setUsbEchoEnabled(bool enabled) { _usbEchoEnabled = enabled; }
  bool isUsbEchoEnabled() const { return _usbEchoEnabled; }
};

#endif
