#pragma once

// Pure helpers for linking a W-Soft box to the Weighsoft Indicator Hub from the web page.
// No Arduino, no mbedtls: host-tested in test/test_hub_pairing.
//
// The pairing code is what the Hub shows once under Devices > New pairing code
// (Weighsoft.Indicator.Hub server/src/registry): base64url of compact JSON
// {"v":1,"hub_url":"...","organisation_id":"...","device_id":"...","key_id":"...","secret":"<base64url, 32 bytes>"}.
// Newer Hubs add "hub_urls":["http://...", ...] with every address the Hub listens on. This box
// keeps only the LAN ones: it cannot join a tailnet (100.64/10) and loopback is itself.

#include <stddef.h>
#include <stdint.h>
#include <time.h>

static const size_t HUB_CODE_MAX = 600;  // same cap as the USB line reader in hub.cpp
static const size_t HUB_URL_MAX = 160;
static const uint8_t HUB_URLS_MAX = 4;  // LAN addresses kept from "hub_urls"; extra ones are ignored
static const size_t HUB_KEY_ID_MAX = 80;
static const size_t HUB_DEVICE_ID_MAX = 80;
static const size_t HUB_SECRET_LEN = 32;

enum HubCodeResult {
  HUB_CODE_OK = 0,
  HUB_CODE_EMPTY,        // nothing pasted
  HUB_CODE_TOO_LONG,     // longer than HUB_CODE_MAX
  HUB_CODE_NOT_BASE64,   // not base64url text
  HUB_CODE_NOT_JSON,     // decoded, but not the Hub's JSON
  HUB_CODE_NO_URL,       // hub_url missing or not http(s)://
  HUB_CODE_NO_KEY,       // key_id missing
  HUB_CODE_BAD_SECRET,   // secret missing or not 32 bytes
};

struct HubPairing {
  char hub_url[HUB_URL_MAX + 1];  // trailing slashes removed; the first LAN address when the code lists several
  // Addresses to try, in order. Always at least one: an old single-address code, or a code with
  // no LAN address at all, gives a list of just hub_url (and hub_url_hint warns about it).
  char hub_urls[HUB_URLS_MAX][HUB_URL_MAX + 1];
  uint8_t hub_url_count;
  char key_id[HUB_KEY_ID_MAX + 1];
  char device_id[HUB_DEVICE_ID_MAX + 1];  // may be empty
  uint8_t secret[HUB_SECRET_LEN];         // raw pairing secret; the HMAC key is sha256(secret)
};

// Trims spaces and line breaks around the code, then decodes and checks it.
// On success fills *out; on failure *out is cleared. out may be nullptr to only check the code.
// Never logs anything.
HubCodeResult hub_code_parse(const char *code, HubPairing *out);

// Short English text for the web page.
const char *hub_code_result_text(HubCodeResult result);

// Base64url (or plain base64) decode, padding optional. Returns false on a bad character
// or when the output does not fit. *length gets the decoded size.
bool hub_base64url_decode(const char *text, size_t text_len, uint8_t *out, size_t capacity, size_t *length);

enum HubUrlHint {
  HUB_URL_FINE = 0,
  HUB_URL_LOCALHOST,  // localhost / 127.x: points at the ESP itself
  HUB_URL_TAILSCALE,  // 100.64.0.0/10: the ESP is not on Tailscale
};

// Warns when the address in the code can't work from a box on a plain LAN.
HubUrlHint hub_url_hint(const char *hub_url);
const char *hub_url_hint_text(HubUrlHint hint);

// Parses an HTTP Date header (RFC 7231 IMF-fixdate, e.g. "Sat, 31 Oct 2026 08:05:56 GMT")
// into Unix seconds. Returns false if it is not that format. Used to set the clock from the
// Hub when the network has no internet time.
bool hub_parse_http_date(const char *text, time_t *out);

// Which Hub address to use. Moves to the next address only when the Hub gave no answer at all
// (HTTP status <= 0: refused, timed out, no route). Any HTTP status, even 401 or 503, means the
// address works, so the box stays on it. Wraps around to the first address.
struct HubFailover {
  uint8_t count;   // addresses in the list (at least 1)
  uint8_t active;  // index of the address in use
};

void hub_failover_begin(HubFailover *failover, uint8_t count);
void hub_failover_result(HubFailover *failover, bool reached);
