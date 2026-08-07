# Relay Board Digital Twin Implementation Plan

> **For agentic workers:** Execute task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Document the ESP-12F 4-ch relay board wiring from photos, add RelayService firmware, and ship a browser digital twin (3D board + GPIO/stats/control) on branch `RelayBoardEspBuildIn`.

**Architecture:** Follow LedExample StatefulService pattern for 4 relays + board status. Frontend project page with CSS-3D twin, wiring diagram, live system/relay APIs (demo mode when offline). Flash via Prolific USB-serial (Windows may expose as COM2 while FriendlyName says COM3 due to Bluetooth COM conflict).

**Tech Stack:** PlatformIO `esp12e`, ESP8266React framework, React/MUI interface, mermaid wiring docs.

## Global Constraints

- Branch: `RelayBoardEspBuildIn` (new integration baseline)
- Board: ESP-12F (DOIT), PlatformIO env `esp12e`
- Default relay pins (LC-style): RY1=16, RY2=14, RY3=12, RY4=13, active LOW
- No BLE on ESP8266
- No fabricated sensors — only real/board-typical hardware

---

## Tasks

### Task 1: Wiring diagram + hardware inventory
- [x] Document serial path: Laptop USB → Prolific → DB9 → MAX3232 → ESP TX0/RX0/GND/3V3
- [x] Document power, RY1–4, headers, relays, LEDs, flash (IO0)
- [x] Save `docs/RELAY-BOARD-ESP-BUILT-IN.md`

### Task 2: RelayBoardService firmware
- [x] `src/examples/relay/` state + GPIO + REST/WS
- [x] Board status (heap, chip, GPIO map, relays, LED/buzzer if present)
- [x] Register in `main.cpp`, default env `esp12e`

### Task 3: Browser digital twin
- [x] CSS-3D board + GPIO DI/DO panel + stats + control
- [x] Wiring diagram tab
- [x] Menu + routes

### Task 4: Serial connect / flash attempt
- [x] Resolve COM3 vs COM2 Prolific conflict
- [x] Attempt serial open @ 115200; flash if port usable
  - COM2 opens; upload timed out waiting for bootloader (need IO0→GND + RST)

### Task 5: Docs index + commit/push
- [x] Link from docs README / integration workflow
- [ ] Commit and push
