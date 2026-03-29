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
    if (originId != "weight_data") {
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
