#include "VersionService.h"

VersionService::VersionService(AsyncWebServer* server, SecurityManager* securityManager) :
    _httpEndpoint(VersionInfo::read,
                  VersionInfo::update,
                  this,
                  server,
                  VERSION_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::NONE_REQUIRED)  // Public endpoint
{
  // No update handlers needed - version info is static
}

void VersionService::begin() {
  _state.version     = VERSION_STRING;
  _state.apiVersion  = API_VERSION;
  _state.buildDate   = BUILD_DATE;
  _state.buildTime   = BUILD_TIME;
  _state.projectName = PROJECT_NAME;
  _state.projectUrl  = PROJECT_URL;

  Serial.printf("[Version] %s build %s %s\n", VERSION_STRING, BUILD_DATE, BUILD_TIME);
}
