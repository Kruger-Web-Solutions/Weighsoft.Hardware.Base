#include <ESP8266React.h>
#include <examples/liveweight/LiveWeightService.h>
#include <examples/relay/RelayBoardService.h>

#define SERIAL_BAUD_RATE 115200

#ifdef ESP8266
// Route the ADC to measure the 3.3V supply so the twin can report VCC
ADC_MODE(ADC_VCC);
#endif

// Use pointers to avoid early construction issues on ESP32
AsyncWebServer* server;
ESP8266React* esp8266React;
RelayBoardService* relayBoardService;
LiveWeightService* liveWeightService;

void setup() {
  // start serial and filesystem
  Serial.begin(SERIAL_BAUD_RATE);
  delay(500);

  Serial.println(F("\n\n=== Weighsoft Relay Board ESP Built-In ==="));
#ifdef ESP32
  Serial.print(F("ESP-IDF version: "));
  Serial.println(esp_get_idf_version());
#endif
  Serial.print(F("Free heap: "));
  Serial.println(ESP.getFreeHeap());

  Serial.println(F("[1/6] Creating web server..."));
  server = new AsyncWebServer(80);
  Serial.println(F("[1/6] Web server created OK"));

  Serial.println(F("[2/6] Initializing framework..."));
  esp8266React = new ESP8266React(server);
  Serial.println(F("[2/6] Framework created OK"));

  Serial.println(F("[3/6] Starting framework services..."));
  esp8266React->begin();
  Serial.println(F("[3/6] Framework initialized OK"));

  Serial.println(F("[4/6] Initializing relay board service..."));
  relayBoardService =
      new RelayBoardService(server, esp8266React->getSecurityManager(), esp8266React->getMqttClient());
  relayBoardService->begin();
  Serial.println(F("[4/6] Relay board service loaded OK"));

  Serial.println(F("[5/6] Initializing live weight service..."));
  liveWeightService = new LiveWeightService(
      server, esp8266React->getFS(), esp8266React->getSecurityManager(), esp8266React->getMqttClient());
  liveWeightService->setRelayBoardService(relayBoardService);
  liveWeightService->begin();
  relayBoardService->setDiEdgeCallback([](uint8_t di, bool active) {
    if (liveWeightService) {
      liveWeightService->onDiEdge(di, active);
    }
  });
  Serial.println(F("[5/6] Live weight service loaded OK"));

  Serial.println(F("[6/6] Starting web server..."));
  server->begin();
  Serial.println(F("[6/6] Web server started OK"));

  Serial.println(F("=== System Ready! ==="));
  Serial.print(F("Free heap after init: "));
  Serial.println(ESP.getFreeHeap());
}

void loop() {
  esp8266React->loop();
  relayBoardService->loop();
  liveWeightService->loop();
}
