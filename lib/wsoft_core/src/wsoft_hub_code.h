#pragma once

// Reads a Weighsoft Indicator Hub pairing code (server/src/registry/registry.ts in that repo):
// base64url of the JSON {v, hub_url, [hub_urls], organisation_id, device_id, key_id, secret},
// where secret is 32 random bytes in standard base64. A Hub with 2+ addresses adds
// "hub_urls" (every address it listens on, e.g. Wi-Fi and cable); this board keeps the LAN ones
// as backup addresses of the SAME Hub (one key, one Hub). Ported from the 31 Oct 2026 Waveshare
// branch (claude/esp-hub-multi-address, b9e2ab1 / b2695c0), Rhyno 14 Nov 2026. Pure C++ so the web page and the USB
// command check a code the same way, in host tests too.
//
// The secret is the device's signing material: never log it, echo it or send it back.

#include <stddef.h>
#include <stdint.h>

static const size_t WSOFT_HUB_CODE_MAX = 600;  // same cap as the USB "HUB PAIR" line
static const size_t WSOFT_HUB_URL_MAX = 128;
static const size_t WSOFT_HUB_ID_MAX = 64;
// LAN addresses kept from "hub_urls"; extra ones are ignored.
static const uint8_t WSOFT_HUB_URLS_MAX = 4;

struct WsoftHubCode {
  char hub_url[WSOFT_HUB_URL_MAX];      // no trailing '/'; the first LAN address when the code lists several
  // Addresses to try, in order, hub_url first. Always at least one: an old single-address code, or
  // a code with no LAN address at all (only Tailscale or localhost), gives a list of just hub_url.
  char hub_urls[WSOFT_HUB_URLS_MAX][WSOFT_HUB_URL_MAX];
  uint8_t hub_url_count;
  char key_id[WSOFT_HUB_ID_MAX];
  char device_id[WSOFT_HUB_ID_MAX];     // "" when the code has none
  uint8_t secret[32];
};

// Why a code was refused, for the web page. Never mentions the secret's value.
enum WsoftHubCodeError {
  WSOFT_HUB_CODE_OK = 0,
  WSOFT_HUB_CODE_EMPTY,
  WSOFT_HUB_CODE_TOO_LONG,
  WSOFT_HUB_CODE_NOT_BASE64,
  WSOFT_HUB_CODE_NO_URL,       // hub_url missing, too long, or not http:// / https://
  WSOFT_HUB_CODE_NO_KEY_ID,
  WSOFT_HUB_CODE_BAD_SECRET,   // secret missing or not 32 bytes
};

// Leading and trailing spaces/newlines (from copy-paste) are ignored.
WsoftHubCodeError wsoft_hub_code_parse(const char *code, WsoftHubCode *out);

// Short, plain-words reason for the web page.
const char *wsoft_hub_code_error_text(WsoftHubCodeError error);

// A board on a plain LAN cannot reach a Tailscale address (100.64.0.0/10), and localhost /
// 127.x is the board itself. Those are skipped as backup addresses.
bool wsoft_hub_url_is_lan(const char *url);

// Which Hub address is in use. Moves to the next address only when the Hub gave no answer at
// all (HTTP status <= 0: refused, timed out, no route). Any HTTP status, even 401 or 503, means
// the address works, so the board stays on it. Wraps around to the first address.
struct WsoftHubFailover {
  uint8_t count = 1;   // addresses in the list (at least 1)
  uint8_t active = 0;  // index of the address in use
};
void wsoft_hub_failover_begin(WsoftHubFailover *failover, uint8_t count);
// Returns true when the address in use changed.
bool wsoft_hub_failover_result(WsoftHubFailover *failover, int http_status);
