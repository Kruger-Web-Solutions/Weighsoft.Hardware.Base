#include "UartModeService.h"
#include <examples/serialwriter/SerialWriterService.h>
#include <examples/diagnostics/DiagnosticsService.h>

UartModeService::UartModeService(AsyncWebServer* server,
                                 FS* fs,
                                 SecurityManager* securityManager)
    : _httpEndpoint(UartModeState::read,
                    UartModeState::update,
                    this,
                    server,
                    UART_MODE_ENDPOINT_PATH,
                    securityManager,
                    AuthenticationPredicates::IS_AUTHENTICATED),
      _fsPersistence(UartModeState::read,
                     UartModeState::update,
                     this,
                     fs,
                     UART_MODE_CONFIG_FILE),
      _webSocket(UartModeState::read,
                 UartModeState::update,
                 this,
                 server,
                 UART_MODE_SOCKET_PATH,
                 securityManager,
                 AuthenticationPredicates::IS_AUTHENTICATED) {
  _serialWriterService = nullptr;
  _diagnosticsService = nullptr;

  addUpdateHandler([this](const String& originId) {
    if (originId != "init") {
      onModeChanged();
    }
  }, false);
}

void UartModeService::begin() {
  _fsPersistence.readFromFS();

  // Validate loaded mode
  if (_state.mode > (uint8_t)UartModeType::DIAGNOSTICS) {
    Serial.println(F("[UartMode] WARNING: Invalid mode in config, defaulting to WRITER"));
    _state.mode = (uint8_t)UartModeType::WRITER;
    _fsPersistence.writeToFS();
  }

  const char* modeName = isWriterMode() ? "WRITER" : "DIAGNOSTICS";
  Serial.printf("[UartMode] Loaded mode: %s\n", modeName);
}

void UartModeService::setSerialWriterService(SerialWriterService* serialWriterService) {
  _serialWriterService = serialWriterService;
}

void UartModeService::setDiagnosticsService(DiagnosticsService* diagnosticsService) {
  _diagnosticsService = diagnosticsService;
}

void UartModeService::onModeChanged() {
  Serial.println(F("[UartMode] Mode change requested - applying new mode"));
  applyMode();
  _fsPersistence.writeToFS();
}

void UartModeService::applyMode() {
  if (!_serialWriterService || !_diagnosticsService) {
    Serial.println(F("[UartMode] WARNING: Services not registered yet"));
    return;
  }

  if (isWriterMode()) {
    Serial.println(F("[UartMode] Switching to WRITER mode"));
    _diagnosticsService->stopAllTests();
    _serialWriterService->resumeSerial();
  } else {
    Serial.println(F("[UartMode] Switching to DIAGNOSTICS mode"));
    _serialWriterService->suspendSerial();
  }
}
