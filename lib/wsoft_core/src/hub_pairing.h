#pragma once

// Pure helpers for linking a W-Soft box to the Weighsoft Indicator Hub from the web page.
// No Arduino, no mbedtls: host-tested in test/test_hub_pairing.
//
// The pairing code is what the Hub shows once under Devices > New pairing code
// (Weighsoft.Indicator.Hub server/src/registry): base64url of compact JSON
// {"v":1,"hub_url":"...","organisation_id":"...","device_id":"...","key_id":"...","secret":"<base64url, 32 bytes>"}.

#include <stddef.h>
#include <stdint.h>
#include <time.h>

static const size_t HUB_CODE_MAX = 600;  // same cap as the USB line reader in hub.cpp
static const size_t HUB_URL_MAX = 160;
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
  char hub_url[HUB_URL_MAX + 1];  // trailing slashes removed
  char key_id[HUB_KEY_ID_MAX + 1];
  char device_id[HUB_DEVICE_ID_MAX + 1];  // may be empty
  uint8_t secret[HUB_SECRET_LEN];         // raw pairing secret; the HMAC key is sha256(secret)
};

// Trims spaces and line breaks around the code, then decodes and checks it.
// On success fills *out. Never logs anything.
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
