#include "MdnsService.h"
#include <ESPmDNS.h>
#include <SettingValue.h>

#define MDNS_SERVICE_NAME  "_weighsoft"
#define MDNS_SERVICE_PROTO "_tcp"
#define MDNS_SERVICE_PORT  80

MdnsService::MdnsService(UartModeService* uartModeService)
    : _uartModeService(uartModeService) {}

void MdnsService::begin() {
  if (_started) return;

  String hostname = String("weighsoft-") + currentId();
  if (!MDNS.begin(hostname.c_str())) {
    Serial.printf("[mDNS] begin(%s) failed\n", hostname.c_str());
    return;
  }
  MDNS.addService(MDNS_SERVICE_NAME, MDNS_SERVICE_PROTO, MDNS_SERVICE_PORT);
  applyServiceTxt();
  _started = true;
  Serial.printf("[mDNS] announced %s.local with service %s.%s\n",
                hostname.c_str(), MDNS_SERVICE_NAME, MDNS_SERVICE_PROTO);
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
  if (_uartModeService->isReader()) return "reader";
  if (_uartModeService->isWriter()) return "writer";
  if (_uartModeService->isDiagnostics()) return "diagnostics";
  return "new";
}

String MdnsService::currentName() const {
  return currentId();
}

String MdnsService::currentId() const {
  return SettingValue::getUniqueId();
}
