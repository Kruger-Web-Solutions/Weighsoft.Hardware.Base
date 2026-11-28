#pragma once

#include <Arduino.h>
#ifdef WSOFT_ON_BASE
#include <wsoft_packers.h>
#endif

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

#ifdef WSOFT_ON_BASE
enum HubPackerAuthState {
  HUB_PACKER_AUTH_IDLE = 0,
  HUB_PACKER_AUTH_PENDING,
  HUB_PACKER_AUTH_DONE,
  HUB_PACKER_AUTH_OFFLINE,
};

uint32_t hub_packer_auth_start(const char *code, const char *pin, const char *action, bool fire_and_forget = false);
HubPackerAuthState hub_packer_auth_poll(uint32_t job_id, WsoftPackerAuthReply *out, bool cancel_if_pending = false);
void hub_packer_auth_cancel(uint32_t job_id);
bool hub_packer_auth(const char *code, const char *pin, const char *action, WsoftPackerAuthReply *out);
void hub_trigger_bag_events_push();
#endif
