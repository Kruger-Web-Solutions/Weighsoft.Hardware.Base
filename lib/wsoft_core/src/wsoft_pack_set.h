#pragma once

// Pack-Size Auto-Save (up to 8 target pack weights in whole grams, +/- tolerance_g).
// Pure C++ for host tests (test/test_wsoft_pack_set).
//
// OFF by default (enabled = false). When enabled and no Hi-Lo product is selected,
// Auto save only saves settled net weights within tolerance_g of one of targets_g.

#include <stddef.h>
#include <stdint.h>

#include "wsoft_hilo.h"

static const size_t WSOFT_PACK_SET_MAX = 8;
static const int32_t WSOFT_PACK_SET_GRAMS_MAX = WSOFT_HILO_GRAMS_MAX;

struct WsoftPackSet {
  int32_t targets_g[WSOFT_PACK_SET_MAX] = {};
  uint8_t count = 0;
  int32_t tolerance_g = 0;
  bool enabled = false;
};

struct WsoftPackSetMatch {
  bool matched = false;
  int32_t matched_target_g = 0;
  int32_t low_limit_g = 0;
  int32_t high_limit_g = 0;
};

void wsoft_pack_set_defaults(WsoftPackSet *cfg);

// True when count <= WSOFT_PACK_SET_MAX, tolerance_g in [0, WSOFT_PACK_SET_GRAMS_MAX],
// and every active target in targets_g[0..count) is in [1, WSOFT_PACK_SET_GRAMS_MAX].
bool wsoft_pack_set_valid(const WsoftPackSet &cfg);

// Field-by-field equality (ignores unused slots in targets_g[count..WSOFT_PACK_SET_MAX)).
bool wsoft_pack_set_same(const WsoftPackSet &a, const WsoftPackSet &b);

// Finds the nearest valid positive target in targets_g[0..count) and checks
// abs(net_g - target_g) <= tolerance_g. When cfg.enabled, net_g > 0, and a valid
// target exists, *out receives matched_target_g, low_limit_g and high_limit_g even
// when matched is false. Returns true iff matched.
bool wsoft_pack_set_match(const WsoftPackSet &cfg, int32_t net_g, WsoftPackSetMatch *out);

// Converts a net weight in whole grams to a WsoftHiloState:
// PASS when matched, LOW/HIGH outside [nearest - tolerance_g, nearest + tolerance_g],
// NONE when disabled, empty (net_g <= 0), unsettled, or no valid targets exist.
WsoftHiloState wsoft_pack_set_hilo_state(const WsoftPackSet &cfg, int32_t net_g, bool settled = true);

// Reading-unit wrapper (1 = kg, 2 = t, 3 = g; decimals 0..9). Rounds sub-gram readings
// half away from zero and delegates to wsoft_pack_set_match / wsoft_pack_set_hilo_state.
WsoftHiloState wsoft_pack_set_check(const WsoftPackSet &cfg, int32_t net_value, int decimals, int unit,
                                    bool settled = true, WsoftPackSetMatch *out = nullptr);
