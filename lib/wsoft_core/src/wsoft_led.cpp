#include "wsoft_led.h"

// Kept well under full brightness: the LED sits on the board, often at eye level on a bench.
WsoftLedColour wsoft_led_colour(bool enabled, WsoftRelayZone zone) {
  if (!enabled) return {0, 0, 0};
  switch (zone) {
    case WSOFT_RELAY_LOW:
      return {96, 32, 0};  // orange
    case WSOFT_RELAY_OK:
      return {0, 96, 0};  // green
    case WSOFT_RELAY_HIGH:
      return {96, 0, 0};  // red
    default:
      return {0, 0, 0};
  }
}
