#ifndef RemoteWeightService_h
#define RemoteWeightService_h

#include <ESPFS.h>
#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <WebSocketTxRx.h>
#include <examples/remoteweight/RemoteWeightState.h>

#define REMOTE_WEIGHT_ENDPOINT_PATH "/rest/remoteWeight"
#define REMOTE_WEIGHT_SOCKET_PATH "/ws/remoteWeight"
#define REMOTE_WEIGHT_CONFIG_FILE "/config/remoteWeightConfig.json"

class RemoteWeightService : public StatefulService<RemoteWeightState> {
 public:
  RemoteWeightService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager);
  void begin();

  bool isEnabled() const { return _state.enabled; }
  bool isDisplayEnabled() const { return _state.displayEnabled; }

 private:
  HttpEndpoint<RemoteWeightState> _httpEndpoint;
  FSPersistence<RemoteWeightState> _fsPersistence;
  WebSocketTxRx<RemoteWeightState> _webSocket;

  // Tracks the previous `timestamp` so the update handler can distinguish
  // weight payloads (timestamp bumped on every weight POST, even when the
  // value is identical) from config-only saves (timestamp untouched).
  unsigned long _lastSeenTimestamp = 0;
};

#endif
