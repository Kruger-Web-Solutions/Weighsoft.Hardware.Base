#ifndef DiagnosticsService_h
#define DiagnosticsService_h

#include <HttpEndpoint.h>
#include <WebSocketTxRx.h>
#include <SettingValue.h>
#include <examples/diagnostics/DiagnosticsState.h>

#define DIAGNOSTICS_ENDPOINT_PATH "/rest/diagnostics"
#define DIAGNOSTICS_SOCKET_PATH "/ws/diagnostics"

// Use same GPIO pins as Serial1
// ESP32-S3: GPIO18=U1RXD (RX), GPIO17=U1TXD (TX) - must match hardware UART1 defaults
#define DIAG_SERIAL_PORT Serial1
#define DIAG_RX_PIN 18
#define DIAG_TX_PIN 17

class DiagnosticsService : public StatefulService<DiagnosticsState> {
 public:
  DiagnosticsService(AsyncWebServer* server, SecurityManager* securityManager);
  void begin();
  void loop();

  // Stop all active tests (called by UartModeService)
  void stopAllTests();

 private:
  HttpEndpoint<DiagnosticsState> _httpEndpoint;
  WebSocketTxRx<DiagnosticsState> _webSocket;

  String _rxBuffer;
  bool _serialStarted;
  unsigned long _lastTestTime;

  // Test-specific state
  unsigned long _loopbackLastSend;
  unsigned long _baudTestStartTime;
  unsigned long _signalTestStartTime;
  float* _latencyBuffer;  // For jitter calculation
  uint16_t _latencyBufferSize;
  unsigned long _lastWsBroadcast;  // For throttling WebSocket updates

  // Test methods
  void runLoopbackTest();
  void runBaudScan();
  void runSignalQualityTest();

  // Helper methods
  void startSerial(uint32_t baud);
  void stopSerial();
  String readSerialLine();
  void calculateSignalQuality();
};

#endif
