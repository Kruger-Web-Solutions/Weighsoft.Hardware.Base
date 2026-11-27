#include "wsoft_pack_set.h"

#include <limits.h>

void wsoft_pack_set_defaults(WsoftPackSet *cfg) {
  if (cfg == nullptr) return;
  for (size_t i = 0; i < WSOFT_PACK_SET_MAX; i++) cfg->targets_g[i] = 0;
  cfg->count = 0;
  cfg->tolerance_g = 0;
  cfg->enabled = false;
}

bool wsoft_pack_set_valid(const WsoftPackSet &cfg) {
  if (cfg.count > WSOFT_PACK_SET_MAX) return false;
  if (cfg.tolerance_g < 0 || cfg.tolerance_g > WSOFT_PACK_SET_GRAMS_MAX) return false;
  for (uint8_t i = 0; i < cfg.count; i++) {
    if (cfg.targets_g[i] <= 0 || cfg.targets_g[i] > WSOFT_PACK_SET_GRAMS_MAX) return false;
  }
  return true;
}

bool wsoft_pack_set_same(const WsoftPackSet &a, const WsoftPackSet &b) {
  if (a.enabled != b.enabled || a.count != b.count || a.tolerance_g != b.tolerance_g) return false;
  uint8_t n = a.count <= WSOFT_PACK_SET_MAX ? a.count : (uint8_t)WSOFT_PACK_SET_MAX;
  for (uint8_t i = 0; i < n; i++) {
    if (a.targets_g[i] != b.targets_g[i]) return false;
  }
  return true;
}

bool wsoft_pack_set_match(const WsoftPackSet &cfg, int32_t net_g, WsoftPackSetMatch *out) {
  if (out) *out = WsoftPackSetMatch();
  if (!cfg.enabled || cfg.tolerance_g < 0 || cfg.tolerance_g > WSOFT_PACK_SET_GRAMS_MAX || net_g <= 0) {
    return false;
  }

  size_t n = cfg.count <= WSOFT_PACK_SET_MAX ? cfg.count : WSOFT_PACK_SET_MAX;
  bool found = false;
  int32_t best_target = 0;
  int64_t best_dist = 0;

  for (size_t i = 0; i < n; i++) {
    int32_t t = cfg.targets_g[i];
    if (t <= 0 || t > WSOFT_PACK_SET_GRAMS_MAX) continue;
    int64_t dist = (int64_t)net_g - (int64_t)t;
    if (dist < 0) dist = -dist;
    if (!found || dist < best_dist) {
      found = true;
      best_target = t;
      best_dist = dist;
    }
  }

  if (!found) return false;

  WsoftPackSetMatch m;
  m.matched_target_g = best_target;
  m.low_limit_g = (int32_t)((int64_t)best_target - (int64_t)cfg.tolerance_g);
  m.high_limit_g = (int32_t)((int64_t)best_target + (int64_t)cfg.tolerance_g);
  m.matched = best_dist <= (int64_t)cfg.tolerance_g;
  if (out) *out = m;
  return m.matched;
}

WsoftHiloState wsoft_pack_set_hilo_state(const WsoftPackSet &cfg, int32_t net_g, bool settled) {
  if (!cfg.enabled || !settled || net_g <= 0) return WSOFT_HILO_NONE;
  WsoftPackSetMatch m;
  if (wsoft_pack_set_match(cfg, net_g, &m)) return WSOFT_HILO_PASS;
  if (m.matched_target_g <= 0) return WSOFT_HILO_NONE;
  return net_g < m.low_limit_g ? WSOFT_HILO_LOW : WSOFT_HILO_HIGH;
}

namespace {

bool reading_to_grams(int32_t value, int decimals, int unit, int32_t *grams_out) {
  if (decimals < 0 || decimals > 9 || value <= 0) return false;
  int gram_exponent;
  switch (unit) {
    case 1: gram_exponent = 3; break;  // kg
    case 2: gram_exponent = 6; break;  // t
    case 3: gram_exponent = 0; break;  // g
    default: return false;
  }
  int shift = gram_exponent - decimals;
  int64_t grams = 0;
  if (shift >= 0) {
    int64_t scale = 1;
    for (int i = 0; i < shift; i++) scale *= 10;
    if ((int64_t)value > (int64_t)INT32_MAX / scale) {
      *grams_out = INT32_MAX;
      return true;
    }
    grams = (int64_t)value * scale;
  } else {
    int64_t divisor = 1;
    for (int i = 0; i < -shift; i++) divisor *= 10;
    grams = ((int64_t)value + divisor / 2) / divisor;
  }
  if (grams <= 0) return false;
  *grams_out = grams > INT32_MAX ? INT32_MAX : (int32_t)grams;
  return true;
}

}  // namespace

WsoftHiloState wsoft_pack_set_check(const WsoftPackSet &cfg, int32_t net_value, int decimals, int unit, bool settled,
                                    WsoftPackSetMatch *out) {
  if (out) *out = WsoftPackSetMatch();
  if (!cfg.enabled || !settled) return WSOFT_HILO_NONE;
  int32_t net_g = 0;
  if (!reading_to_grams(net_value, decimals, unit, &net_g)) return WSOFT_HILO_NONE;
  WsoftPackSetMatch m;
  bool ok = wsoft_pack_set_match(cfg, net_g, &m);
  if (out) *out = m;
  if (ok) return WSOFT_HILO_PASS;
  if (m.matched_target_g <= 0) return WSOFT_HILO_NONE;
  return net_g < m.low_limit_g ? WSOFT_HILO_LOW : WSOFT_HILO_HIGH;
}
