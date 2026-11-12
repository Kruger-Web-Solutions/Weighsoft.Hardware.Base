#pragma once

// Hi-Lo relay outputs (LOW / OK / HIGH lights or gates), pure C++ for host tests
// (test/test_wsoft_relay). The board code (lib/wsoft_esp/src/relay.*) feeds it every poll;
// src/examples/relay/RelayBoardService drives the pins.
//
// Safety first: anything that is not a clear, live, checked weight gives NONE, and NONE
// means every Hi-Lo relay is OFF.

#include <stdint.h>

#include "wsoft_hilo.h"

// Same numbers as the relay board firmware (branch RelayBoardEspBuildIn,
// RelayBoardService::setWeightBandRelays): 0 none, 1 low, 2 ok, 3 high.
enum WsoftRelayZone { WSOFT_RELAY_NONE = 0, WSOFT_RELAY_LOW = 1, WSOFT_RELAY_OK = 2, WSOFT_RELAY_HIGH = 3 };

static const uint8_t WSOFT_RELAY_COUNT = 4;        // relays the board service knows (1..4)
static const uint8_t WSOFT_RELAY_UNUSED = 0;       // "no relay for this zone"
static const uint8_t WSOFT_RELAY_DEFAULT_LOW = 1;
static const uint8_t WSOFT_RELAY_DEFAULT_OK = 2;
static const uint8_t WSOFT_RELAY_DEFAULT_HIGH = 3;
// A new zone must show on this many polls in a row before a relay switches (2 x 250 ms),
// so a weight sitting on a limit does not make a relay chatter.
static const uint8_t WSOFT_RELAY_HOLD_POLLS = 2;
// The screen task posts the zone after every poll. Older than this = the task is stuck:
// the relays go OFF.
static const uint32_t WSOFT_RELAY_STALE_MS = 2000;

struct WsoftRelayInput {
  bool linked = false;   // the scale answers
  bool alarm = false;    // the junction box reports a blocking alarm
  bool blanked = false;  // the reading is blanked (shows ----)
  bool picked = false;   // a Hi-Lo product is in use
  int32_t value = 0;     // the reading, in display steps (0 or less = nothing on the scale)
  WsoftHiloState state = WSOFT_HILO_NONE;  // wsoft_hilo_check() of that reading
};

// The zone this reading asks for. NONE unless: linked, no alarm, not blanked, a product is
// picked, something is on the scale (value > 0) and the check gave LOW, PASS or HIGH.
WsoftRelayZone wsoft_relay_want(const WsoftRelayInput &in);

struct WsoftRelayHold {
  WsoftRelayZone shown = WSOFT_RELAY_NONE;
  WsoftRelayZone candidate = WSOFT_RELAY_NONE;
  uint8_t count = 0;
};

// One poll. fresh = this poll gave a new reading. Returns the zone to drive.
// - NONE applies at once (OFF is never delayed).
// - A poll that is not fresh changes nothing (a failed poll is never a weight).
// - Any other zone applies after WSOFT_RELAY_HOLD_POLLS fresh polls in a row asking for it.
WsoftRelayZone wsoft_relay_step(WsoftRelayHold *hold, WsoftRelayZone want, bool fresh);

// A zone posted at posted_ms is still good at now_ms (millis() wrap safe).
bool wsoft_relay_fresh(uint32_t posted_ms, uint32_t now_ms);

// Relay numbers for LOW / OK / HIGH: each 0 (unused) or 1..4, and no relay used twice.
bool wsoft_relay_map_valid(uint8_t relay_low, uint8_t relay_ok, uint8_t relay_high);

// Bit n-1 set = relay n ON for this zone. 0 for NONE, an unused zone or a bad map.
uint8_t wsoft_relay_mask(WsoftRelayZone zone, uint8_t relay_low, uint8_t relay_ok, uint8_t relay_high);

// The bits of the relays the map uses at all (the ones Hi-Lo owns).
uint8_t wsoft_relay_owned(uint8_t relay_low, uint8_t relay_ok, uint8_t relay_high);

// "NONE" / "LOW" / "OK" / "HIGH" for the page and the log.
const char *wsoft_relay_zone_text(WsoftRelayZone zone);
