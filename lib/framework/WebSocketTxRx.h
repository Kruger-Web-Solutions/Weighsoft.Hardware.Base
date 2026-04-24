#ifndef WebSocketTxRx_h
#define WebSocketTxRx_h

#include <StatefulService.h>
#include <ESPAsyncWebServer.h>
#include <SecurityManager.h>

#define WEB_SOCKET_CLIENT_ID_MSG_SIZE 128

#define WEB_SOCKET_ORIGIN "websocket"
#define WEB_SOCKET_ORIGIN_CLIENT_ID_PREFIX "websocket:"

template <class T>
class WebSocketConnector {
 protected:
  StatefulService<T>* _statefulService;
  AsyncWebServer* _server;
  AsyncWebSocket _webSocket;
  size_t _bufferSize;

  WebSocketConnector(StatefulService<T>* statefulService,
                     AsyncWebServer* server,
                     const char* webSocketPath,
                     SecurityManager* securityManager,
                     AuthenticationPredicate authenticationPredicate,
                     size_t bufferSize) :
      _statefulService(statefulService), _server(server), _webSocket(webSocketPath), _bufferSize(bufferSize) {
    Serial.printf("[WS] Registering secured WebSocket at: %s\n", webSocketPath);
    _webSocket.setFilter(securityManager->filterRequest(authenticationPredicate));
    _webSocket.onEvent(std::bind(&WebSocketConnector::onWSEvent,
                                 this,
                                 std::placeholders::_1,
                                 std::placeholders::_2,
                                 std::placeholders::_3,
                                 std::placeholders::_4,
                                 std::placeholders::_5,
                                 std::placeholders::_6));
    _server->addHandler(&_webSocket);
    _server->on(webSocketPath, HTTP_GET, std::bind(&WebSocketConnector::forbidden, this, std::placeholders::_1));
  }

  WebSocketConnector(StatefulService<T>* statefulService,
                     AsyncWebServer* server,
                     const char* webSocketPath,
                     size_t bufferSize) :
      _statefulService(statefulService), _server(server), _webSocket(webSocketPath), _bufferSize(bufferSize) {
    _webSocket.onEvent(std::bind(&WebSocketConnector::onWSEvent,
                                 this,
                                 std::placeholders::_1,
                                 std::placeholders::_2,
                                 std::placeholders::_3,
                                 std::placeholders::_4,
                                 std::placeholders::_5,
                                 std::placeholders::_6));
    _server->addHandler(&_webSocket);
  }

  virtual void onWSEvent(AsyncWebSocket* server,
                         AsyncWebSocketClient* client,
                         AwsEventType type,
                         void* arg,
                         uint8_t* data,
                         size_t len) = 0;

  String clientId(AsyncWebSocketClient* client) {
    return WEB_SOCKET_ORIGIN_CLIENT_ID_PREFIX + String(client->id());
  }

 private:
  void forbidden(AsyncWebServerRequest* request) {
    Serial.printf("[WS] Forbidden request to: %s\n", request->url().c_str());
    request->send(403);
  }
};

template <class T>
class WebSocketTx : virtual public WebSocketConnector<T> {
 public:
  WebSocketTx(JsonStateReader<T> stateReader,
              StatefulService<T>* statefulService,
              AsyncWebServer* server,
              const char* webSocketPath,
              SecurityManager* securityManager,
              AuthenticationPredicate authenticationPredicate = AuthenticationPredicates::IS_ADMIN,
              size_t bufferSize = DEFAULT_BUFFER_SIZE) :
      WebSocketConnector<T>(statefulService,
                            server,
                            webSocketPath,
                            securityManager,
                            authenticationPredicate,
                            bufferSize),
      _stateReader(stateReader) {
    WebSocketConnector<T>::_statefulService->addUpdateHandler(
        [&](const String& originId) { transmitData(nullptr, originId); }, false);
  }

  WebSocketTx(JsonStateReader<T> stateReader,
              StatefulService<T>* statefulService,
              AsyncWebServer* server,
              const char* webSocketPath,
              size_t bufferSize = DEFAULT_BUFFER_SIZE) :
      WebSocketConnector<T>(statefulService, server, webSocketPath, bufferSize), _stateReader(stateReader) {
    WebSocketConnector<T>::_statefulService->addUpdateHandler(
        [&](const String& originId) { transmitData(nullptr, originId); }, false);
  }

 protected:
  virtual void onWSEvent(AsyncWebSocket* server,
                         AsyncWebSocketClient* client,
                         AwsEventType type,
                         void* arg,
                         uint8_t* data,
                         size_t len) {
    if (type == WS_EVT_CONNECT) {
      Serial.printf("[WS] Client connected: %u\n", client->id());
      // when a client connects, we transmit it's id and the current payload
      transmitId(client);
      transmitData(client, WEB_SOCKET_ORIGIN);
    } else if (type == WS_EVT_DISCONNECT) {
      Serial.printf("[WS] Client disconnected: %u\n", client->id());
    } else if (type == WS_EVT_ERROR) {
      Serial.printf("[WS] Client error: %u\n", client->id());
    }
  }

 private:
  JsonStateReader<T> _stateReader;

  void transmitId(AsyncWebSocketClient* client) {
    DynamicJsonDocument jsonDocument = DynamicJsonDocument(WEB_SOCKET_CLIENT_ID_MSG_SIZE);
    JsonObject root = jsonDocument.to<JsonObject>();
    root["type"] = "id";
    root["id"] = WebSocketConnector<T>::clientId(client);
    size_t len = measureJson(jsonDocument);
    // Allocate len+1 so serializeJson can write the null terminator safely,
    // but only send len bytes (the actual JSON, without the null)
    AsyncWebSocketMessageBuffer* buffer = WebSocketConnector<T>::_webSocket.makeBuffer(len + 1);
    if (buffer) {
      serializeJson(jsonDocument, (char*)buffer->get(), len + 1);
      client->text((char*)buffer->get(), len);
      delete buffer;
    }
  }

  /**
   * Broadcasts the payload to the destination, if provided. Otherwise broadcasts to all clients except the origin, if
   * specified.
   *
   * Original implementation sent clients their own IDs so they could ignore updates they initiated. This approach
   * simplifies the client and the server implementation but may not be sufficent for all use-cases.
   */
  void transmitData(AsyncWebSocketClient* client, const String& originId) {
    DynamicJsonDocument jsonDocument = DynamicJsonDocument(WebSocketConnector<T>::_bufferSize);
    JsonObject root = jsonDocument.to<JsonObject>();
    root["type"] = "payload";
    root["origin_id"] = originId;
    JsonObject payload = root.createNestedObject("payload");
    WebSocketConnector<T>::_statefulService->read(payload, _stateReader);

    size_t len = measureJson(jsonDocument);
    // Allocate len+1 so serializeJson can write the null terminator safely,
    // but only send len bytes (the actual JSON, without the null)
    AsyncWebSocketMessageBuffer* buffer = WebSocketConnector<T>::_webSocket.makeBuffer(len + 1);
    if (buffer) {
      serializeJson(jsonDocument, (char*)buffer->get(), len + 1);
      if (client) {
        client->text((char*)buffer->get(), len);
      } else {
        WebSocketConnector<T>::_webSocket.textAll((char*)buffer->get(), len);
      }
      delete buffer;  // char* overload copies only; makeBuffer() is not adopted
    }
  }
};

template <class T>
class WebSocketRx : virtual public WebSocketConnector<T> {
 public:
  WebSocketRx(JsonStateUpdater<T> stateUpdater,
              StatefulService<T>* statefulService,
              AsyncWebServer* server,
              const char* webSocketPath,
              SecurityManager* securityManager,
              AuthenticationPredicate authenticationPredicate = AuthenticationPredicates::IS_ADMIN,
              size_t bufferSize = DEFAULT_BUFFER_SIZE) :
      WebSocketConnector<T>(statefulService,
                            server,
                            webSocketPath,
                            securityManager,
                            authenticationPredicate,
                            bufferSize),
      _stateUpdater(stateUpdater) {
  }

  WebSocketRx(JsonStateUpdater<T> stateUpdater,
              StatefulService<T>* statefulService,
              AsyncWebServer* server,
              const char* webSocketPath,
              size_t bufferSize = DEFAULT_BUFFER_SIZE) :
      WebSocketConnector<T>(statefulService, server, webSocketPath, bufferSize), _stateUpdater(stateUpdater) {
  }

 protected:
  virtual void onWSEvent(AsyncWebSocket* server,
                         AsyncWebSocketClient* client,
                         AwsEventType type,
                         void* arg,
                         uint8_t* data,
                         size_t len) {
    if (type == WS_EVT_DATA) {
      AwsFrameInfo* info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len) {
        if (info->opcode == WS_TEXT) {
          DynamicJsonDocument jsonDocument = DynamicJsonDocument(WebSocketConnector<T>::_bufferSize);
          DeserializationError error = deserializeJson(jsonDocument, (char*)data);
          if (!error && jsonDocument.is<JsonObject>()) {
            JsonObject jsonObject = jsonDocument.as<JsonObject>();
            WebSocketConnector<T>::_statefulService->update(
                jsonObject, _stateUpdater, WebSocketConnector<T>::clientId(client));
          }
        }
      }
    }
  }

 private:
  JsonStateUpdater<T> _stateUpdater;
};

template <class T>
class WebSocketTxRx : public WebSocketTx<T>, public WebSocketRx<T> {
 public:
  WebSocketTxRx(JsonStateReader<T> stateReader,
                JsonStateUpdater<T> stateUpdater,
                StatefulService<T>* statefulService,
                AsyncWebServer* server,
                const char* webSocketPath,
                SecurityManager* securityManager,
                AuthenticationPredicate authenticationPredicate = AuthenticationPredicates::IS_ADMIN,
                size_t bufferSize = DEFAULT_BUFFER_SIZE) :
      WebSocketConnector<T>(statefulService,
                            server,
                            webSocketPath,
                            securityManager,
                            authenticationPredicate,
                            bufferSize),
      WebSocketTx<T>(stateReader,
                     statefulService,
                     server,
                     webSocketPath,
                     securityManager,
                     authenticationPredicate,
                     bufferSize),
      WebSocketRx<T>(stateUpdater,
                     statefulService,
                     server,
                     webSocketPath,
                     securityManager,
                     authenticationPredicate,
                     bufferSize) {
  }

  WebSocketTxRx(JsonStateReader<T> stateReader,
                JsonStateUpdater<T> stateUpdater,
                StatefulService<T>* statefulService,
                AsyncWebServer* server,
                const char* webSocketPath,
                size_t bufferSize = DEFAULT_BUFFER_SIZE) :
      WebSocketConnector<T>(statefulService, server, webSocketPath, bufferSize),
      WebSocketTx<T>(stateReader, statefulService, server, webSocketPath, bufferSize),
      WebSocketRx<T>(stateUpdater, statefulService, server, webSocketPath, bufferSize) {
  }

 protected:
  void onWSEvent(AsyncWebSocket* server,
                 AsyncWebSocketClient* client,
                 AwsEventType type,
                 void* arg,
                 uint8_t* data,
                 size_t len) {
    WebSocketRx<T>::onWSEvent(server, client, type, arg, data, len);
    WebSocketTx<T>::onWSEvent(server, client, type, arg, data, len);
  }
};

#endif
