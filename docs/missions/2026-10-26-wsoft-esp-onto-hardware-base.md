# Mission: W-Soft ESP firmware onto the Hardware.Base framework

STATUS: IN-FLIGHT

## MISSION BRIEF

```text
MISSION BRIEF
OBJECTIVE:    A new Hardware.Base integration branch, made from master, that runs the W-Soft
              ESP firmware (Waveshare ESP32-S3 4.3 touch LCD + DJB junction box over RS485)
              inside the Base framework, reusing W-Soft's working logic files unchanged.
CONSTRAINTS:  - Rhyno 26 Oct: Base pattern — branch from master, follow master's code and
                structure; never copy a foreign tree in.
              - Henzard (via Rhyno) 25 Oct: keep the existing ESP firmware; build on it, do not
                rewrite it, do not swap in the Pi engine.
              - No Python in product code (C++ on the ESP, React for the web UI).
              - Repo is PUBLIC (a fork; cannot be made private). No secrets: gitleaks before
                every push; OTA secret stays in an ignored header with a committed *.example.
              - Weights are whole grams; a failed poll is never a weight; approval stays OFF
                until Rhyno switches it on; approval switching stays USB/technician-only.
              - Do not invent pinouts, registers or calibration (verified map:
                Weighsoft.WSoft hardware/contracts/djb-m1x-rs485.md).
              - Standing defaults: (1) working end-to-end slice first; (2) operator settings
                live in the framework's settings UI, not env/flags; (3) the screen journey
                (boot, live weight, no signal, fault, not-for-trade) is in SUCCESS;
                (4) storage: the framework's LittleFS JSON settings — no database on the ESP.
              - Nothing is flashed from this container; flashing is a bench step on the laptop.
SUCCESS:      1. (DERIVED) New branch `wsoft-waveshare43` exists, created from master @ 0f9dd1d.
              2. (DERIVED) `pio run -e wsoft_waveshare43` builds the framework + W-Soft.
              3. (DERIVED) approval.*, trade_logic.*, hub.* are byte-identical to
                 Weighsoft.WSoft esp/src @ cfcd012 (empty diff), or every change is listed
                 and justified in the plan.
              4. (DERIVED) Host unit tests (test_trade_logic) pass: `pio test -e native`.
              5. (DERIVED) Framework web page shows live weight, stable flag, approval state
                 and audit counter, read-only.
              6. (DERIVED) gitleaks: no leaks in the branch's new commits.
              7. (DERIVED) Bench: after flashing, the screen shows the same states as today's
                 W-Soft firmware (boot, live weight, NO SIGNAL, fault, Not for trade).
ASSUMPTIONS:  | assumption | conf | blast radius if wrong |
              | Base framework builds on Arduino-ESP32 3.1 (needed by the Waveshare screen) | M | high: the port needs a core bump or a different screen driver |
              | RAM/PSRAM fits screen buffers + web server + Wi-Fi | M | medium: drop features or tune buffers |
              | ALLinOneEspSerialdevice's esp32s3 env is a usable template | H | low |
              | The thin service wrapper counts as "build on", not "rewrite" | M | high: Henzard rejects; fall back to screen-loop-only first slice |
MISSION:      weighsoft-op-feature
DEFERRED:     - Indicator → new repo Weighsoft.Indicator.Edge (separate brief; repo must be
                created by Rhyno).
              - Wi-Fi passwords already exposed in this public repo (rotate; separate brief).
GATES:        - Plan approval (scope choice): ASKED
              - Push of the new branch to the public repo: TOLD (Rhyno chose "Push now
                (public)" and "Port onto master, plan first", 26 Oct)
              - Flash to the bench ESP: ASKED
              - Merge into the integration branch: ASKED
              DEMO: required — the screen and web page are user-observable; Rhyno judges on
              the bench.
BUDGET:       standard
```

## Ledger

Critical path: scout → planner → plan gate (Rhyno) → builder ⇄ reviewer → qa → bench demo;
start now: scout.

| # | Agent | Objective | Status | SITREP |
|---|---|---|---|---|
| 1 | scout | INTEL PACKET: Base framework vs W-Soft ESP firmware | IN-FLIGHT | – |

## Stage 1 — INTEL PACKET (scout, 26 Oct 2026, DONE_WITH_CONCERNS, confidence medium)

Key findings (full packet: /mnt/project-files/plans/2026-10-26-wsoft-esp-intel-packet.md):

1. **Core version is the biggest risk.** Base has only built on Arduino-ESP32 2.0.x (ALLinOne esp32s3
   compile_commands: IDF v4.4.7). W-Soft's RGB screen (ESP32_Display_Panel) needs core 3.1.1
   (pioarduino 53.03.11). Framework uses unmaintained me-no-dev ESPAsyncWebServer HEAD
   (platformio.ini:41); a maintained fork is likely needed. Unverified — first thing to prove.
2. **hub.cpp collides with the framework:** own Wi-Fi task (hub.cpp:251-280), configTime (:259),
   global ArduinoOTA (:5, :310-326) which does not compile under `-D NO_GLOBAL_ARDUINOOTA`;
   includes gitignored `ota_secret.h` with no committed example. "hub.* byte-identical" cannot
   hold; changes must be listed.
3. trade_logic.* copies unchanged (pure). approval.* copies unchanged if NVS Preferences sits
   beside the framework's LittleFS JSON.
4. Plug-in pattern: `StatefulService` + `HttpGetEndpoint` / `WebSocketTx` (read-only), per
   src/examples/led/LedExampleService.h:74-93; closest prior art weighingboard WeighingService.
5. Keep `ARDUINO_USB_CDC_ON_BOOT=0` (USB technician commands on UART0), RS485 on Serial1 15/16;
   do not pull ALLinOne SerialService. W-Soft's blocking loop (≤600 ms Modbus + ≤1 s draw)
   delays the framework loop.
6. Secrets/doc drift (deferred): ALLinOne compile_commands.json exposes a real Wi-Fi password;
   README/INTEGRATION-WORKFLOW name non-existent serial2/display35 branches.

| 1 | scout | INTEL PACKET | DONE_WITH_CONCERNS | above |

## Stage 2 — PLAN (planner, DONE_WITH_CONCERNS, confidence medium)

Plan: /mnt/project-files/plans/2026-10-26-wsoft-esp-plan.md (summary). Options A screen-first /
B screen + Base web page (recommended) / C B + web pairing. Spike on core 3.1.1 decides B vs A.
PlatformIO can't fetch toolchains in the cloud container → spike + builds run on the bench
laptop or a self-hosted runner.

| 2 | planner | PLAN + scope options | DONE_WITH_CONCERNS | above |

## Decisions and ledger (continued)

- 2026-10-26 08:38 UTC: Rhyno chose option B (decision card); goal: working and deployable to the client.
- 08:40: correction — W-Soft must be first-class services (REST, WebSocket, MQTT, options) like the LED example and the other branches. Ratchet: builder re-dispatched.
- 08:42: Rhyno chose ALLinOneEspSerialdevice @ 035fc12 as the base (decision card). `wsoft-waveshare43` re-pointed from 0f9dd1d to 035fc12 (branch had no own commits).
- Push to the public repo: TOLD (Rhyno chose "Push now (public)" on the decision card, 07:52).

| 3 | post | push wsoft-waveshare43 (now 035fc12) | DONE | ls-remote |
| 4 | spike (bench laptop) | core 3.1.1 build of ALLinOne + W-Soft env | IN-FLIGHT | – |
| 5 | builder | option B on 035fc12 | DONE_WITH_CONCERNS | 8 commits 66e5235..3bc4ae2; host tests 15+17+9 pass; byte-identity + unifdef OK; interface build OK; gitleaks clean; pio never run here |
