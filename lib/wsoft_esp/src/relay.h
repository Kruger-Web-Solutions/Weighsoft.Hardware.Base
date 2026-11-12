#pragma once

// Hi-Lo relay zone, shared between the screen task and the web / relay service.
// The rules live in wsoft_core (wsoft_relay), host-tested.
//
// The screen task posts after every weight poll (relay_post). The relay service on the
// Arduino loop reads the zone (relay_zone) and drives the pins. A zone older than
// WSOFT_RELAY_STALE_MS reads as NONE, so a stuck screen task or an OTA update lets the
// relays go OFF.

#include <Arduino.h>

#include <wsoft_relay.h>

// Screen task only: this poll's reading. fresh = the poll gave a new reading.
void relay_post(const WsoftRelayInput &in, bool fresh);

// Any task: the zone to drive now. NONE when nothing fresh was posted.
WsoftRelayZone relay_zone();
