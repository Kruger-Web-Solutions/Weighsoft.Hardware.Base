#include "UartModeService.h"
#include <examples/serial/SerialService.h>
#include <examples/diagnostics/DiagnosticsService.h>
#include <examples/serialwriter/SerialWriterService.h>
#include <examples/serialwriter/SerialWriterForwarderService.h>

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
  // Register update handler for mode changes
  addUpdateHandler([this](const String& originId) {
    if (originId != "init") {
      onModeChanged();
    }
  }, false);
}

void UartModeService::begin() {
  // Load persisted mode from filesystem.
  // If the file doesn't exist or is invalid, modeStr will stay "" (NEW state).
  _fsPersistence.readFromFS();

  Serial.printf("[UartMode] Loaded mode: \"%s\"\n", _state.modeStr.c_str());

  // Mode will be applied in main.cpp after all services are initialized
}

void UartModeService::setSerialService(SerialService* serialService) {
  _serialService = serialService;
}

void UartModeService::setWriterService(SerialWriterService* writerService) {
  _writerService = writerService;
}

void UartModeService::setForwarderService(SerialWriterForwarderService* forwarderService) {
  _forwarderService = forwarderService;
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
  // Suspend ALL services first so Serial1 is always released before a new owner claims it.
  if (_serialService) {
    _serialService->suspendSerial();
  }
  if (_writerService) {
    _writerService->suspendWriter();
  }
  if (_forwarderService) {
    _forwarderService->stop();
  }
  if (_diagnosticsService) {
    _diagnosticsService->stopAllTests();
  }

  // Resume the active mode's owner.
  if (_state.isWriter()) {
    Serial.println(F("[UartMode] Switching to WRITER mode"));
    if (_writerService) {
      _writerService->resumeWriter();
    }
    if (_forwarderService) {
      _forwarderService->start();
    }
  } else if (_state.isDiagnostics()) {
    Serial.println(F("[UartMode] Switching to DIAGNOSTICS mode"));
    // DiagnosticsService manages its own Serial1 start when tests are triggered —
    // no explicit resume call needed here (preserved from existing pattern).
  } else {
    // READER mode and NEW (unconfigured) state both default to SerialService.
    Serial.printf("[UartMode] Switching to READER mode (modeStr=\"%s\")\n", _state.modeStr.c_str());
    if (_serialService) {
      _serialService->resumeSerial();
    }
  }
}
