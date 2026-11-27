#pragma once

// Board side of the Hub Control room (Hub ADR 0010, contract hub-screen-v1). Pure C++ for host
// tests (test/test_wsoft_hub_screen).
//
// The board pushes the words on its screen (the same WsoftUiModel JSON that GET /rest/wsoftScreen
// returns) to POST /api/v1/ingest/screen, signed like every ingest request:
//   {"sent_at":"<ISO UTC>","screen":<model JSON>}
// When: the screen changed and at least WSOFT_SCREEN_MIN_MS since the last push, or
// WSOFT_SCREEN_KEEPALIVE_MS since the last push. Only while the Hub is online and no firmware
// upload runs (the hub task checks that). The Hub answers 202 and keeps only the latest screen.
// A 404 means the Hub has no Control room yet: the board waits WSOFT_SCREEN_OLD_HUB_MS. Any other
// refusal (4xx: too many, too big, not allowed) waits WSOFT_SCREEN_REFUSED_MS; no answer or a Hub
// error (5xx) tries the changed screen again after WSOFT_SCREEN_MIN_MS.
// The model carries only how many PIN digits were typed (pin_count), never a digit.

#include <stddef.h>
#include <stdint.h>

static const uint32_t WSOFT_SCREEN_MIN_MS = 3000;
static const uint32_t WSOFT_SCREEN_KEEPALIVE_MS = 30000;
static const uint32_t WSOFT_SCREEN_OLD_HUB_MS = 600000;
static const uint32_t WSOFT_SCREEN_REFUSED_MS = 60000;
// The model JSON (WSOFT_UI_JSON_MAX, 2560) plus the envelope; the Hub takes up to 4096.
static const size_t WSOFT_SCREEN_BODY_MAX = 2560 + 64;

struct WsoftScreenPush {
  bool tried = false;      // a push was tried at least once
  bool ever = false;       // a push was answered 202 at least once
  uint32_t last_ms = 0;    // when the last push was tried
  uint32_t last_hash = 0;  // the screen the Hub holds (last 202)
  uint32_t hold_ms = 0;    // the Hub asked us to wait this long after last_ms (0 = no wait)
};

// FNV-1a over the JSON text: tells a changed screen from the same one.
uint32_t wsoft_screen_hash(const char *json);

// False when no push can be due yet, whatever the screen: lets the board skip building the JSON.
bool wsoft_screen_push_soon(const WsoftScreenPush &push, uint32_t now_ms);

// True when the screen with this hash should be pushed now.
bool wsoft_screen_push_due(const WsoftScreenPush &push, uint32_t now_ms, uint32_t hash);

// Records the Hub's answer (HTTP status, or <= 0 for no answer) to the push of `hash`.
void wsoft_screen_push_result(WsoftScreenPush *push, uint32_t now_ms, uint32_t hash, int status);

// {"sent_at":"...","screen":<model_json>} into out. False when out is too small or an input is
// empty; out is then "".
bool wsoft_screen_push_body(const char *sent_at, const char *model_json, char *out, size_t size);
