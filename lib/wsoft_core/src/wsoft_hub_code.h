#pragma once

// Reads a Weighsoft Indicator Hub pairing code (server/src/registry/registry.ts in that repo):
// base64url of the JSON {v, hub_url, organisation_id, device_id, key_id, secret}, where
// secret is 32 random bytes in standard base64. Pure C++ so the web page and the USB
// command check a code the same way, in host tests too.
//
// The secret is the device's signing material: never log it, echo it or send it back.

#include <stddef.h>
#include <stdint.h>

static const size_t WSOFT_HUB_CODE_MAX = 600;  // same cap as the USB "HUB PAIR" line
static const size_t WSOFT_HUB_URL_MAX = 128;
static const size_t WSOFT_HUB_ID_MAX = 64;

struct WsoftHubCode {
  char hub_url[WSOFT_HUB_URL_MAX];      // no trailing '/'
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
