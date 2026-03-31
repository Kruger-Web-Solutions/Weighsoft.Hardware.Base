#include <examples/growbox/GrowboxAutomationService.h>

GrowboxAutomationService::GrowboxAutomationService(AsyncWebServer* server,
                                                   FS* fs,
                                                   SecurityManager* securityManager) :
    _httpEndpoint(GrowboxAutomationSettings::read,
                  GrowboxAutomationSettings::update,
                  this,
                  server,
                  GROWBOX_AUTOMATION_ENDPOINT_PATH,
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED,
                  GROWBOX_AUTOMATION_BUFFER_SIZE),
    _fsPersistence(GrowboxAutomationSettings::read,
                   GrowboxAutomationSettings::update,
                   this,
                   fs,
                   GROWBOX_AUTOMATION_FILE,
                   GROWBOX_AUTOMATION_BUFFER_SIZE),
    _webSocket(GrowboxAutomationSettings::read,
               GrowboxAutomationSettings::update,
               this,
               server,
               GROWBOX_AUTOMATION_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_AUTHENTICATED,
               GROWBOX_AUTOMATION_BUFFER_SIZE) {
}

void GrowboxAutomationService::begin() {
  _fsPersistence.readFromFS();
}
