#include "MdnsService.h"
#include <ESPmDNS.h>
#include <WiFi.h>
#include <SettingValue.h>

#define MDNS_SERVICE_NAME  "_weighsoft"
#define MDNS_SERVICE_PROTO "_tcp"
#define MDNS_SERVICE_PORT  80
#define MDNS_RETRY_INTERVAL_MS 5000UL

MdnsService::MdnsService(UartModeService* uartModeService)
    : _uartModeService(uartModeService) {}

void MdnsService::begin() {
  // Defer the actual MDNS.begin() call until the network is up. loop() handles it.
  _enabled = true;
}

void MdnsService::loop() {
  if (!_enabled || _started) return;
  unsigned long now = millis();
  if (now < _nextRetryMs) return;
  _nextRetryMs = now + MDNS_RETRY_INTERVAL_MS;

  // Wait until at least one network interface is up:
  //  - STA connected to a real WiFi network, OR
  //  - SoftAP is running (we'll have a valid AP IP)
  bool staReady = WiFi.status() == WL_CONNECTED;
  bool apReady = WiFi.softAPIP() != IPAddress(0, 0, 0, 0);
  if (!staReady && !apReady) return;

  // IMPORTANT: ArduinoOTA (started by the framework's OTASettingsService) already
  // calls MDNS.begin() internally with its own hostname. Calling MDNS.begin() a
  // second time here would tear down the existing responder and conflict with
  // the WiFi scan path. Instead, we just add our service to the existing
  // mDNS responder.
  MDNS.addService(MDNS_SERVICE_NAME, MDNS_SERVICE_PROTO, MDNS_SERVICE_PORT);
  applyServiceTxt();
  _started = true;
  Serial.printf("[mDNS] added service %s.%s on port %d (using existing responder)\n",
                MDNS_SERVICE_NAME, MDNS_SERVICE_PROTO, MDNS_SERVICE_PORT);
}

void MdnsService::refresh() {
  if (!_started) return;
  applyServiceTxt();
}

void MdnsService::applyServiceTxt() {
  String mode = currentMode();
  String name = currentName();
  String id = currentId();
  MDNS.addServiceTxt(MDNS_SERVICE_NAME, MDNS_SERVICE_PROTO, "mode", mode.c_str());
  MDNS.addServiceTxt(MDNS_SERVICE_NAME, MDNS_SERVICE_PROTO, "name", name.c_str());
  MDNS.addServiceTxt(MDNS_SERVICE_NAME, MDNS_SERVICE_PROTO, "id", id.c_str());
}

String MdnsService::currentMode() const {
  if (!_uartModeService) return "new";
  if (_uartModeService->isWriter()) return "writer";
  if (_uartModeService->isReader()) return "reader";
  return "new";
}

String MdnsService::currentName() const {
  return currentId();
}

String MdnsService::currentId() const {
  return SettingValue::getUniqueId();
}
