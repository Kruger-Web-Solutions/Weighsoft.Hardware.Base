#ifndef DisplaySinkService_h
#define DisplaySinkService_h

#include <HttpEndpoint.h>
#include <WebSocketTxRx.h>
#include <SecurityManager.h>
#include <examples/displaysink/DisplaySinkState.h>

#define DISPLAY_SINK_GET_PATH "/rest/displaySink"
#define DISPLAY_SINK_SOCKET_PATH "/ws/display"

class DisplaySinkService : public StatefulService<DisplaySinkState> {
 public:
  DisplaySinkService(AsyncWebServer* server, SecurityManager* securityManager);
  void begin();

 private:
  HttpGetEndpoint<DisplaySinkState> _httpGet;
  WebSocketTxRx<DisplaySinkState> _webSocket;
};

#endif
