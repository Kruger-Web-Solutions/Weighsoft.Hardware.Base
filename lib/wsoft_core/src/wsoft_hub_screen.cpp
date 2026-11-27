#include "wsoft_hub_screen.h"

#include <stdio.h>

uint32_t wsoft_screen_hash(const char *json) {
  uint32_t h = 2166136261u;
  if (!json) return h;
  for (const unsigned char *p = (const unsigned char *)json; *p; p++) {
    h ^= *p;
    h *= 16777619u;
  }
  return h;
}

// Unsigned elapsed time: right across a millis() wrap, however long the board was not asking.
bool wsoft_screen_push_soon(const WsoftScreenPush &push, uint32_t now_ms) {
  if (!push.tried) return true;
  uint32_t since = now_ms - push.last_ms;
  if (since < push.hold_ms) return false;
  return since >= WSOFT_SCREEN_MIN_MS;
}

bool wsoft_screen_push_due(const WsoftScreenPush &push, uint32_t now_ms, uint32_t hash) {
  if (!wsoft_screen_push_soon(push, now_ms)) return false;
  if (!push.ever || push.hold_ms > 0) return true;  // nothing on the Hub yet, or a wait just ended
  uint32_t since = now_ms - push.last_ms;
  return hash != push.last_hash || since >= WSOFT_SCREEN_KEEPALIVE_MS;
}

void wsoft_screen_push_result(WsoftScreenPush *push, uint32_t now_ms, uint32_t hash, int status) {
  push->tried = true;
  push->last_ms = now_ms;
  push->hold_ms = 0;
  if (status == 202) {
    push->ever = true;
    push->last_hash = hash;
    return;
  }
  // No Control room on this Hub yet (an older Hub): ask again much later.
  if (status == 404) {
    push->hold_ms = WSOFT_SCREEN_OLD_HUB_MS;
    return;
  }
  // Refused (too many, too big, not allowed): retrying every 3 s would not help.
  if (status >= 400 && status < 500) push->hold_ms = WSOFT_SCREEN_REFUSED_MS;
  // No answer or a Hub error (5xx): the Hub still holds the old screen, so the changed one is
  // tried again after WSOFT_SCREEN_MIN_MS.
}

bool wsoft_screen_push_body(const char *sent_at, const char *model_json, char *out, size_t size) {
  if (!out || size == 0) return false;
  out[0] = 0;
  if (!sent_at || !*sent_at || !model_json || !*model_json) return false;
  int n = snprintf(out, size, "{\"sent_at\":\"%s\",\"screen\":%s}", sent_at, model_json);
  if (n < 0 || (size_t)n >= size) {
    out[0] = 0;
    return false;
  }
  return true;
}
