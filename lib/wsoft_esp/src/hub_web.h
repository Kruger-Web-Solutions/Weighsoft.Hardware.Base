#pragma once

// Hardware.Base only (WSOFT_ON_BASE): lets an admin connect W-Soft to the Hub from the web
// interface (W-Soft > Hub), as well as with the USB "HUB PAIR" / "HUB SAVE" lines.
// Kept out of hub.h so that file stays identical to the Weighsoft.WSoft original.
//
// The pairing key holds the signing secret: it is written to NVS and never read back out.
// hub_web_info() returns only the Hub address and ids.

#include "hub.h"

#include <wsoft_hub_code.h>

struct HubWebInfo {
  bool paired;                   // a valid pairing key is saved
  HubState state;                // live state since boot (HUB_SETUP when not paired at boot)
  char hub_url[WSOFT_HUB_URL_MAX];   // the Hub's main address
  // Every address this board may use for the Hub, the main one first; the rest are backups,
  // tried in turn when the Hub gives no answer at all. active = the one in use now.
  char hub_urls[WSOFT_HUB_URLS_MAX][WSOFT_HUB_URL_MAX];
  uint8_t url_count;
  uint8_t active;
  char key_id[WSOFT_HUB_ID_MAX];
  char device_id[WSOFT_HUB_ID_MAX];
  int last_status;               // HTTP status of the last heartbeat, 0 when none was sent
  uint32_t last_age_s;           // seconds since that heartbeat
};

// Checks the key and saves it. The caller restarts the board so the Hub task picks it up.
WsoftHubCodeError hub_web_save(const char *code);
// Forgets the saved key. The caller restarts the board.
void hub_web_forget();
void hub_web_info(HubWebInfo *out);
