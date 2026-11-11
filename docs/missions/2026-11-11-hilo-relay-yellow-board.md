# Plan: Hi-Lo relay on the W-Soft yellow board (CYD)

Date: 11 Nov 2026 (SAST). Author: Claude (thread "Add Hi-Lo relay to W-Soft boards").
Status: IN-FLIGHT. Rhyno said "keep going get it all done" (11 Nov 2026, 21:25 SAST). Nothing flashed.

## Rules read before planning

| Rule source | What it means here |
|---|---|
| Hardware.Base `docs/INTEGRATION-WORKFLOW.md` + `.cursor/rules/integration-branch-confirmation.mdc` | Work on a task branch made from the matching integration branch. Confirm the branch with Rhyno before building. |
| `.cursor/rules/feature-flags.mdc`, `backend-service-pattern.mdc`, `adding-device-projects.mdc` | Operator settings are a StatefulService saved to the file system, not build flags. A relay follows the LED pattern (read + write). Board pins may be build flags. |
| `.cursor/rules/software-versioning.mdc` | New feature = MINOR version bump. |
| repo-review kit (CLAUDE.md) | Plan (intel) before code. A test must be seen to fail. Verify: tests + build green. Draft PR, merge commits only. No secrets. Extend the existing record, no parallel one. |
| Project rules (Rhyno, Henzard) | Keep the existing ESP firmware. No Python. Never invent pinouts. Flashing only with Rhyno's go. Real SHAs only. |
| Second Brain `AGENTS.md` | Times in SAST. Record outcome, owner, next action, evidence. No secrets. |
| Indicator decision `2026-09-25-relay-board-speaks-firmware-contract.md` | The relay REST contract (`POST /rest/relayBoard` with `relayN`, `GET /rest/relayBoardStatus`) is already the agreed language. Edge speaks it. |

## Relay code we already have

| Where | What | Use it? |
|---|---|---|
| Hardware.Base `RelayBoardEspBuildIn` @ b1d0f27, `src/examples/relay/RelayBoardService.*` | 4 relays on pins, REST + WebSocket + MQTT, `setWeightBandRelays(low, ok, high, zone)` | YES: port this service. Same contract, so Edge and the web page can also see and test the relays. |
| Same branch, `src/examples/liveweight/LiveWeightService.*` | Own Low/OK/High band maths + settings `relay_low / relay_ok / relay_high / range_enabled` | Reuse the setting NAMES only. The band maths stays W-Soft's `wsoft_hilo_check()` (Hub limits), so there is one truth. |
| Hardware.Base `growbox`, `GrowboxService.h` | Relay write with an active-high / active-low setting | Reuse the idea (active level is a setting per board). |
| Edge `apps/server/src/features/automation/engine.ts`, `devices/weighsoft-relay.ts` | Edge holds a relay per Hi-Lo verdict over Wi-Fi | No code reuse (Node, other device). Same words: LOW / OK / HIGH. |
| Relay ESP 192.168.1.14 (8 relays, other firmware, login) | Different contract | Not used. |

## Pins (yellow board, ESP32-2432S028)

Source: community pin list witnessmenow/ESP32-Cheap-Yellow-Display `PINS.md` (read 11 Nov 2026) and this repo (`platformio.ini` env `wsoft_cyd28`: GPIO17 = blue RGB LED).

| Relay | Meaning | Pin | Board part |
|---|---|---|---|
| 1 | LOW | GPIO17 | blue LED pad |
| 2 | OK | GPIO16 | green LED pad |
| 3 | HIGH | GPIO4 | red LED pad |

- These LEDs are "active low": pin LOW = on. So the relay output is active LOW too. The little light then shows which relay is on.
- At boot the pins rest HIGH, so an active-low relay stays OFF during start-up. None of the three is a boot strapping pin.
- CN1 (GPIO22/27) stays with the scale. GPIO35 is input only. GPIO21 is the backlight.
- The LED example service must stop using GPIO17 on this board.
- NOT bench-proven. A click test of each pin comes before any real use.

## What gets built

1. `lib/wsoft_core/src/wsoft_relay.*` (pure C++, host tested): from "relay switch on, scale linked, no alarm, product picked, Hi-Lo state" it gives the zone: none / LOW / OK / HIGH. Anything unsafe gives none = all relays OFF. A zone must hold for 2 polls (about 0.5 s) before a relay changes, so it does not chatter at the limit.
2. `src/examples/relay/RelayBoardService.*`: ported from b1d0f27, ESP32 status fields, pins and active level from build flags, a relay with no pin is skipped.
3. W-Soft settings (web page, saved): relay output ON/OFF (default OFF), relay number for LOW / OK / HIGH (default 1 / 2 / 3).
4. Hook: after every weight poll the screen task sets the zone; the pin write takes microseconds. Old relay off before new relay on.
5. `platformio.ini` env `wsoft_cyd28` only: `WSOFT_HAS_RELAY=1`, the three pins, active low. Waveshare build unchanged.
6. Docs: wiring + click test in `docs/WSOFT-CYD28.md`. Version 0.6.1 -> 0.7.0.

## Safety (best practice)

- All relays OFF at boot, on NO SIGNAL, on a stale reading, on alarm, with no product, and when the switch is off.
- A watchdog restart returns the pins to rest = OFF.
- Relay module: opto-isolated, own 5 V supply for the coils. Never power coils from the board's 3V3 pin.
- A switched relay proves the pin, not the contact. Bench test with the real light or gate.

## Branch and gate

- Integration branch: `claude/project-thread-9f4l7x` @ 53a0cce (W-Soft; PRs #33 to #39 merged here).
- Task branch: `claude/project-thread-s7ewih`, draft PR into `9f4l7x`.
- Gate by hand (this branch has no kit scripts): host tests (each new test seen red first), `pio run -e wsoft_cyd28` and `-e wsoft_waveshare43`, gitleaks, diff review. Flash = Rhyno's go only.

## Not in this job

- Waveshare relay (no free pin on a plug; needs an RS485 or I2C relay box).
- Removing unused menus (Serial, Diagnostics, Weight Forwarder show 404 on W-Soft). Separate small cleanup.

## Open points

- Which relay module Rhyno has (trigger level, 3.3 V input ok). Decided on the bench by the click test; the active level is one build flag.
- Relays follow the LIVE Hi-Lo state (with the 0.5 s hold). If Rhyno wants "only when steady", it is one rule in `wsoft_relay`.
