#pragma once

// The "ESP LED": the board's own RGB LED shows the Hi-Lo zone (Rhyno, 21 Nov 2026).
// Under = orange, correct = green, over = red, anything else = dark. Pure C++ for host tests
// (test/test_wsoft_led). The zone is the same one the relays and the screen use
// (wsoft_relay.h: held for two polls, NONE with no signal, an alarm, no product or an empty
// scale), so the light, the relays and the screen never disagree. The W-Soft > Settings
// switch "ESP LED" turns the light off altogether; it does not depend on the relay switch.

#include <stdint.h>

#include "wsoft_relay.h"

struct WsoftLedColour {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

// The colour to show now. Dark (0, 0, 0) when the switch is off or the zone is NONE.
WsoftLedColour wsoft_led_colour(bool enabled, WsoftRelayZone zone);

inline bool wsoft_led_same(const WsoftLedColour &a, const WsoftLedColour &b) {
  return a.red == b.red && a.green == b.green && a.blue == b.blue;
}
