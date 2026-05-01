#ifndef KnownWritersService_h
#define KnownWritersService_h

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <SecurityManager.h>
#include <examples/serial/KnownWritersState.h>

#define KNOWN_WRITERS_ENDPOINT_PATH "/rest/writers"
#define KNOWN_WRITERS_CONFIG_FILE   "/config/knownWriters.json"

class KnownWritersService : public StatefulService<KnownWritersState> {
 public:
  KnownWritersService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager);
  void begin();

  void onWriterConnected(const String& id, const String& name, const String& ip);
  void onWriterDisconnected(const String& id);
  void recordBroadcastToOnlineWriters(const String& message);
  size_t connectedCount();

 private:
  HttpEndpoint<KnownWritersState>  _httpEndpoint;
  FSPersistence<KnownWritersState> _fsPersistence;
  AsyncWebServer*                  _server;
  SecurityManager*                 _securityManager;

  void registerForgetEndpoint();
  void persistAsync();
};

#endif
