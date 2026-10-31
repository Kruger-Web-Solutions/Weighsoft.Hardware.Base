#pragma once

#include <Arduino.h>
#include <hub_pairing.h>

// Sends signed heartbeats to the Weighsoft Indicator Hub (docs/adr/0003 in that repo).
// The pairing code is kept in flash (NVS). It is set from the W-Soft web page (Hub tab,
// admins only, src/examples/wsoft/WSoftHubService) or over USB ("HUB PAIR" + "HUB SAVE").
// Approval stays USB only (approval.cpp).

enum HubState { HUB_SETUP, HUB_NO_WIFI, HUB_NO_CLOCK, HUB_ONLINE, HUB_REJECTED, HUB_UNREACHABLE };

void hub_begin();
void hub_handle_serial(Stream &port);
void hub_ota_poll();
bool hub_ota_busy();
void hub_set_health(bool linked, const char *weight, bool alarm);
HubState hub_state();
const char *hub_state_text(HubState state);

// For the web page. The pairing secret and signing key are never exposed.
bool hub_configured();
// Address the box is sending to now. A code can list several LAN addresses of the Hub; the box
// moves to the next one when the Hub gives no answer at all.
const char *hub_address();
uint8_t hub_address_count();               // 1 for an old single-address code
const char *hub_address_at(uint8_t index);  // "" past the end
const String &hub_key_id();
int hub_last_status();  // last HTTP status from the Hub, 0 before the first try
bool hub_clock_valid();
bool hub_clock_from_hub();  // true when the time came from the Hub (no internet time)

// Checks and stores a pairing code. The caller restarts the box so it takes effect.
// On success *saved_hub_url (if given) gets the first address the box will use.
HubCodeResult hub_save_code(const char *code, String *saved_hub_url = nullptr);
// Forgets the pairing code (Wi-Fi and everything else stay). The caller restarts the box.
void hub_forget_code();
