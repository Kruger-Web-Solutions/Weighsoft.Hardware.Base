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

bool wsoft_screen_push_due(const WsoftScreenPush &push, uint32_t now_ms, uint32_t hash) {
  if (push.holding && (int32_t)(now_ms - push.hold_until_ms) < 0) return false;
  if (!push.ever) return push.holding || push.last_ms == 0 || now_ms - push.last_ms >= WSOFT_SCREEN_MIN_MS;
  uint32_t since = now_ms - push.last_ms;
  if (hash != push.last_hash && since >= WSOFT_SCREEN_MIN_MS) return true;
  return since >= WSOFT_SCREEN_KEEPALIVE_MS;
}

void wsoft_screen_push_result(WsoftScreenPush *push, uint32_t now_ms, uint32_t hash, int status) {
  push->last_ms = now_ms;
  push->holding = false;
  if (status == 202) {
    push->ever = true;
    push->last_hash = hash;
    return;
  }
  // No Control room on this Hub yet (an older Hub), or too many pushes: wait, then try again.
  if (status == 404 || status == 429) {
    push->holding = true;
    push->hold_until_ms = now_ms + (status == 404 ? WSOFT_SCREEN_OLD_HUB_MS : WSOFT_SCREEN_BUSY_HUB_MS);
  }
  // Anything else (no answer, refused, Hub error): the Hub still holds the old screen, so the
  // changed one is tried again after WSOFT_SCREEN_MIN_MS.
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
