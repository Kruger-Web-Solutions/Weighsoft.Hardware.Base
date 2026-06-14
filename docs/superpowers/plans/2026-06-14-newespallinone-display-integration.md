# ESP-Safe Integration Plan — Two Display Features onto NewEspAllInOne

**Branch:** `NewEspAllInOne` (off the `SerialReaderWriter` trunk)
**Date:** 2026-06-14
**Status:** Read-only analysis complete; executing serially.

## Goal

Lift **two display features** onto the trunk as **per-board compile-time options**, extending the trunk's existing per-board flag pattern (the way `FT_BOARD_ESP32_2432S028` gates `BoardDisplayService`). No fat binary: a small 4MB ESP must never compile in everything. Heavy display libs and fonts stay behind their flag; BLE stays off on 4MB boards.

## The two features + their sources

### 1. TFT weight screen (from `espReadWriteRHYNO`)
- `src/examples/display/DisplayService.{h,cpp}` — `TFT_eSPI` renderer, whole body under `#ifdef HAS_TFT_DISPLAY`. Draws received weight on an ILI9488 (3.5in) or ILI9341 (CYD 2.8in) panel.
- **Hard dependency:** `src/examples/remoteweight/{RemoteWeightService.h,.cpp,RemoteWeightState.h}` — the receiver service the TFT is constructed from (`DisplayService(remoteWeightService)`). **Not on trunk.** Must be lifted first.
- Configured purely via `platformio.ini` build_flags (`USER_SETUP_LOADED`, driver, pins, fonts) — no `User_Setup.h`.
- Two ready board profiles on RHYNO: `[env:esp32dev]` (ILI9488) and `[env:cyd28]` (ILI9341 + `TFT_INVERT_DISPLAY`), both `board=esp32dev`, 4MB, `partitions_4mb_ota.csv`.

### 2. Character-LCD device (from `display`)
- `src/examples/display/DisplayService.{h,cpp}` — a **completely different** class: I2C 16x2 LCD via `LiquidCrystal_I2C`, `class DisplayState`, REST `/rest/display`, WS `/ws/display`, optional BLE/serial bridge.
- Vendored lib `lib/LiquidCrystal_I2C/`.
- 4 UI tabs + `api/display.ts` + `types/display.ts` + routing/menu entries.

### Also lifted (prerequisite, shared by the TFT)
- **Remote-weight RECEIVER** (`remoteweight/*` + `RemoteWeightMonitor.tsx`) — genuinely new on trunk, drops in cleanly. (The multi-URL **sender** rewrite is **out of scope** — trunk's single-URL `WeightForwarder` is left untouched, so no persisted config resets.)

## The collision (and the fix)

Both features ship `src/examples/display/DisplayService.{h,cpp}` with class **`DisplayService`** — same path, same filename, same class name, for two unrelated devices. They cannot coexist. Additionally, RHYNO's `[env:esp32dev]` (a TFT board) collides with trunk's existing `[env:esp32dev]` (generic troubleshooting board).

**Resolution — distinct module names + distinct flags:**

| What | TFT feature | Character-LCD feature |
|---|---|---|
| Folder | `src/examples/tftdisplay/` | `src/examples/lcddisplay/` |
| Class | `TftWeightDisplayService` | `LcdDisplayService` (+ `LcdDisplayState`) |
| Files | `TftWeightDisplayService.{h,cpp}` | `LcdDisplayService.{h,cpp}` |
| Flag | `FT_TFT_WEIGHT_SCREEN` (replaces `HAS_TFT_DISPLAY`) | `FT_DISPLAY_LCD` (new) |
| Web route / menu | none (hardware-only) | `lcd-display/*` / "LCD Display" |
| Env rename | `[env:esp32dev]`→`[env:esp32dev_tft]`, `[env:cyd28]`→`[env:cyd_tft]` | — |

Trunk **never** gets a `src/examples/display/` folder, and trunk's existing `src/BoardDisplayService.*` (the "board display" concept) is left untouched — so the overloaded "display" naming is fully disambiguated.

## Feature-flag matrix (per board)

| Board / env | Flash | TFT screen | Remote bridge (receiver) | Char-LCD | BLE |
|---|---|---|---|---|---|
| **esp32s3** (16MB) | `partitions_ble_ota.csv` | off | **ON** (`FT_REMOTE_WEIGHT`) | off | off (forced) |
| **esp32dev_tft** (NEW, ILI9488, 4MB) | `partitions_4mb_ota.csv` | **ON** | **ON** (required by TFT) | off | off |
| **cyd_tft** (NEW, CYD 2.8in, 4MB) | `partitions_4mb_ota.csv` | **ON** (+`TFT_INVERT_DISPLAY`) | **ON** | off | off |
| **firebeetle32** (existing CYD owner, 4MB) | `partitions_ble.csv` | off | off | off | per features |
| **nodemcu_esp32** (DEFAULT, 4MB) | `partitions_ble.csv` | off | off | off | per features |
| **esp32dev** (generic troubleshooting) | `partitions_custom.csv` | off | off | off | per features |
| **DevKit / esp32devkit** (4MB) | — | off | off | optional `FT_DISPLAY_LCD` if I2C LCD wired | off |
| **esp12e** (ESP8266) | littlefs | off | off | off | off |

**Rules baked into the matrix:**
- The **S3** carries the heavy network bridge; **4MB stays minimal**, BLE off.
- **No env compiles all three.** TFT and LCD are **mutually exclusive per board** (one physical panel).
- **Never** set `FT_TFT_WEIGHT_SCREEN` and `FT_BOARD_ESP32_2432S028` in the same env — two TFT drivers (`TFT_eSPI` vs `bb_spi_lcd`) would fight for the same CYD pins. `firebeetle32` stays the `BoardDisplayService` CYD; `cyd_tft` is the new remote-weight viewer.

## Unit breakdown (medium PR bucket, all serial)

- **U1 — Scaffold** (small, 4 files): `partitions_4mb_ota.csv` (copy from RHYNO — not on trunk), `lib/framework/Features.h` (`#ifndef/#define =0` defaults for the three new flags). Per-board flags go in the **env** build_flags, not `features.ini`.
- **U2 — Remote-weight receiver** (medium, 7 files): `remoteweight/*` + `RemoteWeightMonitor.tsx` + additive `ProjectRouting.tsx`/`ProjectMenu.tsx` + `main.cpp` wiring, all under `FT_REMOTE_WEIGHT`.
- **U3 — TFT screen** (medium, 4 files): `tftdisplay/TftWeightDisplayService.{h,cpp}` (renamed), `platformio.ini` (`esp32dev_tft` + `cyd_tft` envs), `main.cpp` (construct after `remoteWeightService->begin()`), under `FT_TFT_WEIGHT_SCREEN`.
- **U4 — Character-LCD** (medium-large, ~13 entries): `lcddisplay/LcdDisplayService.{h,cpp}` (renamed classes), `lib/LiquidCrystal_I2C/`, 5 renamed UI tabs + `api/lcddisplay.ts` + `types/lcddisplay.ts`, additive routing/menu, `main.cpp`, under `FT_DISPLAY_LCD`.

> `src/main.cpp`, `ProjectRouting.tsx`, `ProjectMenu.tsx` are each touched by multiple units → **strictly serialised**, one working tree, no parallel writes.

## Lift order (serial on NewEspAllInOne)

1. **U1** scaffold → commit.
2. **U2** receiver (TFT depends on it) → commit.
3. **U3** TFT screen (renamed + envs) → commit.
4. **U4** character-LCD (renamed + vendored lib) → commit.
5. Full-matrix build sweep, then open PR.

Each step is one coherent commit. Commit before any branch switch (house rule).

## ESP-safety rules (every new service)

1. **No blocking in `loop()`** — millis()-gated only, no `delay()`.
2. **Guard inactive `loop()`s** — flag-off compiles the `.cpp` body to empty no-op `begin()`/`loop()` (mirror `BoardDisplayService`); `main.cpp` calls are guarded or null-pointer-checked.
3. **Heap floor** — keep `REMOTE_WEIGHT_MIN_FREE_HEAP=6000` drop-guard, dead-WS-client reap, and config-persist-only-on-change. Stay well above a 30KB floor under POST load.
4. **Feed the watchdog** — boot-time blocking (TFT ~4s colour-flash) stays in `begin()` before `server->begin()`, under the 10s WDT; make the colour-flash optional. `loop()` never approaches the WDT.
5. **Cross-task safety** — the async HTTP update handler only sets flags + copies Strings; all TFT/SPI drawing stays on the main `loop()` task.
6. **USB-CDC echo** — keep `Serial.setTxTimeoutMs(0)` so async-task echo can't block the web server.
7. **One panel, one owner** — never combine `FT_TFT_WEIGHT_SCREEN` with `FT_BOARD_ESP32_2432S028`; LCD/TFT flags mutually exclusive per board.
8. **BLE bridge scan** — LCD's synchronous 5s BLE scan only exists under `FT_BLE=1` + `bridge_mode='ble'`; it compiles out on the 4MB display envs (BLE off) — keep it that way.
9. **Stay out of the UART suspend/resume contract** — both displays observe data, take no Serial1, so they are NOT added to `UartModeService::applyMode()` (mirror `BoardDisplayService`).
10. **Keep the always-on set small** — enable each flag only on the boards that need it; default/minimal envs build none of the three.

## Verify steps

- **U1:** `pio run -e nodemcu_esp32` green (flags default 0); CSV parses.
- **U2:** `nodemcu_esp32` green (receiver compiled out); a temp env / S3 with `FT_REMOTE_WEIGHT=1` compiles + links; interface build picks up the new route/menu/tsx.
- **U3:** `pio run -e esp32dev_tft` **and** `-e cyd_tft` compile + link with `TFT_eSPI`, fit `partitions_4mb_ota.csv` (app < 1.75MB); `nodemcu_esp32` still green; trunk's `[env:esp32dev]` unchanged.
- **U4:** a board/temp env with `FT_DISPLAY_LCD=1` links `LcdDisplayService` + `LiquidCrystal_I2C`; `nodemcu_esp32` green; interface rebuilds the `lcd-display` route/tabs.
- **Full matrix before PR:** `esp12e`, `nodemcu_esp32`, `firebeetle32` (still `BoardDisplayService` CYD, not new TFT), `esp32dev`, `esp32s3` (receiver on, BLE off), `esp32dev_tft`, `cyd_tft`. No env compiles all three; `firebeetle32` still fits `partitions_ble.csv`.

## Risks

1. **Dual TFT owner on CYD** — `BoardDisplayService` (firebeetle32) vs `TftWeightDisplayService` (cyd_tft) on the same panel; must stay in separate envs, never both flags in one binary.
2. **TFT hard-depends on the receiver** — U3 blocked by U2; TFT envs must set both `FT_REMOTE_WEIGHT=1` and `FT_TFT_WEIGHT_SCREEN=1`.
3. **Shared-file serialisation** — `main.cpp` (U2/U3/U4) and routing/menu (U2/U4) edited by multiple units; strictly serial, no parallel writes.
4. **Flash budget** — TFT_eSPI + fonts + receiver must fit 1.75MB app0; RHYNO ~1.65MB is tight. Trim unused `LOAD_FONT*` if it overflows.
5. **TFT boot blocking** — ~4s colour-flash before `server->begin()`; acceptable under 10s WDT but make optional.
6. **Vendored LiquidCrystal_I2C** relies on `lib_compat_mode=strict` bypass; confirm it resolves only on ESP32 LCD envs.
7. **Sender out of scope** — only the receiver + two displays are lifted; trunk's single-URL forwarder and its `weightForwarderConfig.json` are untouched.
8. **Silent-off flags** — per-board flags have no `features.ini` entry; mitigated by explicit `#ifndef/#define =0` defaults in `Features.h` (U1) and this documented matrix.
