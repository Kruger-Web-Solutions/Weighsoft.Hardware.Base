#ifndef RemoteWeightService_h
#define RemoteWeightService_h

#include <HttpEndpoint.h>
#include <WebSocketTxRx.h>
#include <examples/remoteweight/RemoteWeightState.h>

#define REMOTE_WEIGHT_ENDPOINT_PATH "/rest/remoteWeight"
#define REMOTE_WEIGHT_SOCKET_PATH "/ws/remoteWeight"

class RemoteWeightService : public StatefulService<RemoteWeightState> {
 public:
  RemoteWeightService(AsyncWebServer* server, SecurityManager* securityManager);
  void begin();

 private:
  HttpEndpoint<RemoteWeightState> _httpEndpoint;
  WebSocketTxRx<RemoteWeightState> _webSocket;
};

#endif
