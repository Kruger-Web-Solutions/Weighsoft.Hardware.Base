#ifndef UartModeService_h
#define UartModeService_h

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <WebSocketTxRx.h>
#include "UartMode.h"

// Forward declarations
class SerialService;
class DiagnosticsService;
class SerialWriterService;

#define UART_MODE_ENDPOINT_PATH "/rest/uartMode"
#define UART_MODE_SOCKET_PATH "/ws/uartMode"
#define UART_MODE_CONFIG_FILE "/config/uartMode.json"

class UartModeService : public StatefulService<UartModeState> {
 public:
  UartModeService(AsyncWebServer* server,
                  FS* fs,
                  SecurityManager* securityManager);
  void begin();
  
  // Register services for coordination
  void setSerialService(SerialService* serialService);
  void setDiagnosticsService(DiagnosticsService* diagnosticsService);
  void setSerialWriterService(SerialWriterService* serialWriterService);
  
  // Apply current mode (callable from main.cpp after service linking)
  void applyMode();
  
  // Check current mode
  bool isLiveMode() const { return _state.mode == (uint8_t)UartModeType::LIVE_MONITORING; }
  bool isWriterMode() const { return _state.mode == (uint8_t)UartModeType::WRITER; }
  bool isDiagnosticsMode() const { return _state.mode == (uint8_t)UartModeType::DIAGNOSTICS; }

 private:
  HttpEndpoint<UartModeState> _httpEndpoint;
  FSPersistence<UartModeState> _fsPersistence;
  WebSocketTxRx<UartModeState> _webSocket;
  
  SerialService* _serialService;
  DiagnosticsService* _diagnosticsService;
  SerialWriterService* _serialWriterService;
  
  void onModeChanged();
};

#endif
