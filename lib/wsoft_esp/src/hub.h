#pragma once

#include <Arduino.h>

// Sends signed heartbeats to the Weighsoft Indicator Hub (docs/adr/0003 in that repo).
// Wi-Fi details and the pairing code are provisioned over USB and kept in flash (NVS).

enum HubState { HUB_SETUP, HUB_NO_WIFI, HUB_NO_CLOCK, HUB_ONLINE, HUB_REJECTED, HUB_UNREACHABLE };

void hub_begin();
void hub_handle_serial(Stream &port);
void hub_ota_poll();
bool hub_ota_busy();
void hub_set_health(bool linked, const char *weight, bool alarm);
HubState hub_state();
const char *hub_state_text(HubState state);
