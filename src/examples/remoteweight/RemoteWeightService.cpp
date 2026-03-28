#include <examples/remoteweight/RemoteWeightService.h>

RemoteWeightService::RemoteWeightService(AsyncWebServer* server, SecurityManager* securityManager) :
    _httpEndpoint(RemoteWeightState::read,
                  RemoteWeightState::update,
                  this,
                  server,
                  REMOTE_WEIGHT_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED),
    _webSocket(RemoteWeightState::read,
               RemoteWeightState::update,
               this,
               server,
               REMOTE_WEIGHT_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_AUTHENTICATED) {
}

void RemoteWeightService::begin() {
  _state.weight = "";
  _state.lastLine = "";
  _state.timestamp = 0;
  Serial.println(F("[RemoteWeight] Service ready — listening on /rest/remoteWeight"));
}
