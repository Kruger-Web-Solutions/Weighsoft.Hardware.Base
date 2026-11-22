#ifndef UartModeService_h
#define UartModeService_h

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <WebSocketTxRx.h>
#include <Features.h>
#include "UartMode.h"

// Forward declarations
class SerialService;
class SerialWriterService;
class SerialWriterForwarderService;
class DiagnosticsService;

#define UART_MODE_ENDPOINT_PATH "/rest/uartMode"
#define UART_MODE_SOCKET_PATH "/ws/uartMode"
#if FT_ENABLED(FT_WSOFT)
// W-Soft: "reader" or no file = Weigh (the default), "writer" = Writer. Its own file, so a
// mode saved by older firmware on this board never turns a weigh board into a Writer.
#define UART_MODE_CONFIG_FILE "/config/wsoftRole.json"
#else
#define UART_MODE_CONFIG_FILE "/config/uartMode.json"
#endif

class UartModeService : public StatefulService<UartModeState> {
 public:
  UartModeService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager);
  void begin();

  void setSerialService(SerialService* serialService);
  void setWriterService(SerialWriterService* writerService);
  void setForwarderService(SerialWriterForwarderService* forwarderService);
  void setDiagnosticsService(DiagnosticsService* diagnosticsService);

  void applyMode();

  bool isReader() const { return _state.isReader(); }
  bool isWriter() const { return _state.isWriter(); }
  bool isConfigured() const { return _state.isConfigured(); }

 private:
  HttpEndpoint<UartModeState> _httpEndpoint;
  FSPersistence<UartModeState> _fsPersistence;
  WebSocketTxRx<UartModeState> _webSocket;

  SerialService* _serialService = nullptr;
  SerialWriterService* _writerService = nullptr;
  SerialWriterForwarderService* _forwarderService = nullptr;
  DiagnosticsService* _diagnosticsService = nullptr;

  void onModeChanged();
};

#endif
