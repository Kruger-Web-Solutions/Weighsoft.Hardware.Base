#include "UartModeService.h"
#include <examples/serial/SerialService.h>
#include <examples/diagnostics/DiagnosticsService.h>

UartModeService::UartModeService(AsyncWebServer* server,
                                 FS* fs,
                                 SecurityManager* securityManager) :
    _httpEndpoint(UartModeState::read,
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
               AuthenticationPredicates::IS_AUTHENTICATED)
{
  _serialService = nullptr;
  _diagnosticsService = nullptr;
  
  // Register update handler for mode changes
  addUpdateHandler([this](const String& originId) {
    if (originId != "init") {
      onModeChanged();
    }
  }, false);
}

void UartModeService::begin() {
  // Load persisted mode from filesystem
  // If file doesn't exist or is invalid, mode will already be initialized to LIVE_MONITORING by constructor
  _fsPersistence.readFromFS();
  
  // Validate loaded mode is within valid range (defense against corrupted config)
  if (_state.mode > (uint8_t)UartModeType::DIAGNOSTICS) {
    Serial.println(F("[UartMode] WARNING: Invalid mode in config, defaulting to LIVE_MONITORING"));
    _state.mode = (uint8_t)UartModeType::LIVE_MONITORING;
    _fsPersistence.writeToFS();  // Persist corrected value
  }
  
  const char* modeName = isLiveMode() ? "LIVE MONITORING" : "DIAGNOSTICS";
  Serial.printf("[UartMode] Loaded mode: %s\n", modeName);
  
  // Mode will be applied in main.cpp after all services are initialized
}

void UartModeService::setSerialService(SerialService* serialService) {
  _serialService = serialService;
}

void UartModeService::setDiagnosticsService(DiagnosticsService* diagnosticsService) {
  _diagnosticsService = diagnosticsService;
}

void UartModeService::onModeChanged() {
  Serial.println(F("[UartMode] Mode change requested - applying new mode"));
  applyMode();
  
  // Persist mode change
  _fsPersistence.writeToFS();
}

void UartModeService::applyMode() {
  if (!_serialService || !_diagnosticsService) {
    Serial.println(F("[UartMode] WARNING: Services not registered yet"));
    return;
  }
  
  if (isLiveMode()) {
    Serial.println(F("[UartMode] Switching to LIVE MONITORING mode"));
    // Stop diagnostics, start serial
    _diagnosticsService->stopAllTests();
    _serialService->resumeSerial();
  } else {
    Serial.println(F("[UartMode] Switching to DIAGNOSTICS mode"));
    // Stop serial, diagnostics will start Serial1 when needed
    _serialService->suspendSerial();
  }
}
