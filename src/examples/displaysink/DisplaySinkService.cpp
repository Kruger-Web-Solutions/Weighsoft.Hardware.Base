#include <examples/displaysink/DisplaySinkService.h>

DisplaySinkService::DisplaySinkService(AsyncWebServer* server, SecurityManager* securityManager) :
    _httpGet(DisplaySinkState::read,
            this,
            server,
            DISPLAY_SINK_GET_PATH,
            securityManager,
            AuthenticationPredicates::IS_AUTHENTICATED),
    _webSocket(DisplaySinkState::read,
               DisplaySinkState::update,
               this,
               server,
               DISPLAY_SINK_SOCKET_PATH,
               securityManager,
               AuthenticationPredicates::IS_AUTHENTICATED) {
  addUpdateHandler(
      [this](const String& originId) {
        if (!originId.startsWith("websocket:")) {
          return;
        }
        Serial.printf("[DisplaySink] RX from %s line1_len=%u line2_len=%u last_line_len=%u weight_len=%u\n",
                      originId.c_str(),
                      (unsigned)_state.line1.length(),
                      (unsigned)_state.line2.length(),
                      (unsigned)_state.last_line.length(),
                      (unsigned)_state.weight.length());
      },
      false);
}

void DisplaySinkService::begin() {
  _state.line1 = "";
  _state.line2 = "";
  _state.last_line = "";
  _state.weight = "";
  _state.source_timestamp = 0;
  _state.last_rx_ms = 0;
  Serial.println(F("[DisplaySink] Ready: GET /rest/displaySink, WS /ws/display (auth required)"));
}
