#include "relay.h"

static portMUX_TYPE relay_lock = portMUX_INITIALIZER_UNLOCKED;
static WsoftRelayHold relay_hold;  // screen task only
static WsoftRelayZone posted_zone = WSOFT_RELAY_NONE;
static uint32_t posted_ms = 0;
static bool posted_once = false;

void relay_post(const WsoftRelayInput &in, bool fresh) {
  WsoftRelayZone zone = wsoft_relay_step(&relay_hold, wsoft_relay_want(in), fresh);
  uint32_t now = millis();
  portENTER_CRITICAL(&relay_lock);
  posted_zone = zone;
  posted_ms = now;
  posted_once = true;
  portEXIT_CRITICAL(&relay_lock);
}

WsoftRelayZone relay_zone() {
  portENTER_CRITICAL(&relay_lock);
  WsoftRelayZone zone = posted_zone;
  uint32_t at = posted_ms;
  bool once = posted_once;
  portEXIT_CRITICAL(&relay_lock);
  if (!once || !wsoft_relay_fresh(at, millis())) return WSOFT_RELAY_NONE;
  return zone;
}
