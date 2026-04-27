#include <examples/remoteweight/RemoteWeightService.h>

RemoteWeightService::RemoteWeightService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager) :
    _httpEndpoint(RemoteWeightState::read,
                  RemoteWeightState::update,
                  this,
                  server,
                  REMOTE_WEIGHT_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED),
    _fsPersistence(RemoteWeightState::readConfig,
                   RemoteWeightState::updateConfig,
                   this,
                   fs,
                   REMOTE_WEIGHT_CONFIG_FILE),
    _webSocket(RemoteWeightState::read,
               RemoteWeightState::update,
               this,
               server,
               REMOTE_WEIGHT_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_AUTHENTICATED) {
  _fsPersistence.disableUpdateHandler();

  addUpdateHandler([this](const String& originId) {
    // Detect "new scale line received" by tracking the timestamp. Every weight
    // POST from the Forwarder bumps `_state.timestamp` (even when the value is
    // identical), so a delta here means real fresh data. Config-only saves
    // (UI toggles enabled/displayEnabled/usbEchoEnabled) leave it untouched.
    // This lets us (a) echo on every genuine reading — including a steady
    // scale heartbeat — and (b) persist only on config changes to avoid
    // flash thrash.
    bool isWeightUpdate = (_state.timestamp != _lastSeenTimestamp);
    _lastSeenTimestamp = _state.timestamp;

    // Echo received scale line to a host PC if enabled. Mirrors to both the
    // USB-CDC interface (Serial, when ESP is plugged directly into a PC) and
    // UART0 (Serial0, available via the dev board's built-in CH343 chip on
    // GPIO43/44).
    if (isWeightUpdate && _state.usbEchoEnabled && !_state.lastLine.isEmpty()) {
      Serial.println(_state.lastLine);
#if ARDUINO_USB_CDC_ON_BOOT
      Serial0.println(_state.lastLine);
#endif
    }

    if (!isWeightUpdate) {
      _fsPersistence.writeToFS();
    }
  }, false);
}

void RemoteWeightService::begin() {
  _fsPersistence.readFromFS();
  _state.weight = "";
  _state.lastLine = "";
  _state.timestamp = 0;
  Serial.printf("[RemoteWeight] Service ready — enabled=%d, displayEnabled=%d\n",
                _state.enabled, _state.displayEnabled);
}
