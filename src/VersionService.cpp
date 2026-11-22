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
  // No update handlers needed - version is static
}

void VersionService::begin() {
  // Initialize version info from version.h
  _state.version = VERSION_STRING;
  _state.apiVersion = API_VERSION;
  _state.buildDate = BUILD_DATE;
  _state.buildTime = BUILD_TIME;
  _state.projectName = PROJECT_NAME;
  _state.projectUrl = PROJECT_URL;

  Serial.printf("[Version] Service initialized: %s\n", VERSION_STRING);
}
