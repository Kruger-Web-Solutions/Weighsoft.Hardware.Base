#ifndef DiagnosticsService_h
#define DiagnosticsService_h

#include <HttpEndpoint.h>
#include <WebSocketTxRx.h>
#include <SettingValue.h>
#include <examples/diagnostics/DiagnosticsState.h>

// Forward declaration to avoid circular dependency
class SerialService;

#define DIAGNOSTICS_ENDPOINT_PATH "/rest/diagnostics"
#define DIAGNOSTICS_SOCKET_PATH "/ws/diagnostics"

// Use same GPIO pins as Serial1
// ESP32-S3: GPIO18=U1RXD (RX), GPIO17=U1TXD (TX) - must match hardware UART1 defaults
// Override via build flags: -DDIAG_RX_PIN=xx -DDIAG_TX_PIN=xx
// esp32dev (display board): DIAG pins overridden to avoid conflict with TFT SCK/MISO on GPIO17/18
#define DIAG_SERIAL_PORT Serial1
#ifndef DIAG_RX_PIN
#define DIAG_RX_PIN 18
#endif
#ifndef DIAG_TX_PIN
#define DIAG_TX_PIN 17
#endif

class DiagnosticsService : public StatefulService<DiagnosticsState> {
 public:
  DiagnosticsService(AsyncWebServer* server, SecurityManager* securityManager);
  void begin();
  void loop();  // Must be called in main loop()
  
  // Allow SerialService to register itself for coordination
  void setSerialService(SerialService* serialService);
  
  // Stop all active tests (called by UartModeService)
  void stopAllTests();

 private:
  HttpEndpoint<DiagnosticsState> _httpEndpoint;
  WebSocketTxRx<DiagnosticsState> _webSocket;
  
  SerialService* _serialService;  // Reference to SerialService for coordination
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
  
  // Coordination with SerialService
  bool requestSerialControl();  // Ask SerialService to stop
  void releaseSerialControl();  // Tell SerialService it can resume
};

#endif
