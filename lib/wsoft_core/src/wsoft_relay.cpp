#include "wsoft_relay.h"

WsoftRelayZone wsoft_relay_want(const WsoftRelayInput &in) {
  if (!in.linked || in.alarm || in.blanked || !in.picked || in.value <= 0) return WSOFT_RELAY_NONE;
  switch (in.state) {
    case WSOFT_HILO_LOW: return WSOFT_RELAY_LOW;
    case WSOFT_HILO_PASS: return WSOFT_RELAY_OK;
    case WSOFT_HILO_HIGH: return WSOFT_RELAY_HIGH;
    default: return WSOFT_RELAY_NONE;
  }
}

WsoftRelayZone wsoft_relay_step(WsoftRelayHold *hold, WsoftRelayZone want, bool fresh) {
  if (want == WSOFT_RELAY_NONE) {
    hold->shown = WSOFT_RELAY_NONE;
    hold->candidate = WSOFT_RELAY_NONE;
    hold->count = 0;
    return hold->shown;
  }
  if (!fresh) return hold->shown;
  if (want == hold->shown) {
    hold->candidate = want;
    hold->count = 0;
    return hold->shown;
  }
  if (want != hold->candidate) {
    hold->candidate = want;
    hold->count = 0;
  }
  if (hold->count < WSOFT_RELAY_HOLD_POLLS) hold->count++;
  if (hold->count >= WSOFT_RELAY_HOLD_POLLS) {
    hold->shown = want;
    hold->count = 0;
  }
  return hold->shown;
}

bool wsoft_relay_fresh(uint32_t posted_ms, uint32_t now_ms) {
  return (uint32_t)(now_ms - posted_ms) <= WSOFT_RELAY_STALE_MS;
}

static bool relay_ok(uint8_t n) {
  return n <= WSOFT_RELAY_COUNT;
}

bool wsoft_relay_map_valid(uint8_t relay_low, uint8_t relay_ok_n, uint8_t relay_high) {
  if (!relay_ok(relay_low) || !relay_ok(relay_ok_n) || !relay_ok(relay_high)) return false;
  if (relay_low != WSOFT_RELAY_UNUSED && (relay_low == relay_ok_n || relay_low == relay_high)) return false;
  if (relay_ok_n != WSOFT_RELAY_UNUSED && relay_ok_n == relay_high) return false;
  return true;
}

static uint8_t bit_of(uint8_t relay) {
  return relay == WSOFT_RELAY_UNUSED ? 0 : (uint8_t)(1u << (relay - 1));
}

uint8_t wsoft_relay_mask(WsoftRelayZone zone, uint8_t relay_low, uint8_t relay_ok_n, uint8_t relay_high) {
  if (!wsoft_relay_map_valid(relay_low, relay_ok_n, relay_high)) return 0;
  switch (zone) {
    case WSOFT_RELAY_LOW: return bit_of(relay_low);
    case WSOFT_RELAY_OK: return bit_of(relay_ok_n);
    case WSOFT_RELAY_HIGH: return bit_of(relay_high);
    default: return 0;
  }
}

uint8_t wsoft_relay_owned(uint8_t relay_low, uint8_t relay_ok_n, uint8_t relay_high) {
  if (!wsoft_relay_map_valid(relay_low, relay_ok_n, relay_high)) return 0;
  return (uint8_t)(bit_of(relay_low) | bit_of(relay_ok_n) | bit_of(relay_high));
}

const char *wsoft_relay_zone_text(WsoftRelayZone zone) {
  switch (zone) {
    case WSOFT_RELAY_LOW: return "LOW";
    case WSOFT_RELAY_OK: return "OK";
    case WSOFT_RELAY_HIGH: return "HIGH";
    default: return "NONE";
  }
}
