#include <examples/growbox/GrowboxSettingsService.h>

GrowboxSettingsService::GrowboxSettingsService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager) :
    _httpEndpoint(GrowboxSettings::read,
                  GrowboxSettings::update,
                  this,
                  server,
                  GROWBOX_SETTINGS_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED,
                  GROWBOX_SETTINGS_BUFFER_SIZE),
    _fsPersistence(GrowboxSettings::read,
                   GrowboxSettings::update,
                   this,
                   fs,
                   GROWBOX_SETTINGS_FILE,
                   GROWBOX_SETTINGS_BUFFER_SIZE) {
}

void GrowboxSettingsService::begin() {
  _fsPersistence.readFromFS();
}
