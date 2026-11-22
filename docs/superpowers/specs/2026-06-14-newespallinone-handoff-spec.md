# NewEspAllInOne — All-in-One Firmware Display Integration: Developer / Agent Handoff Spec

> **Branch:** `NewEspAllInOne` (off the `SerialReaderWriter` trunk) · **HEAD:** `7c16c0b` · **Status:** 2 of 4 units done (U1, U2 + a trunk writer fix). U3 (TFT) and U4 (LCD) remain.
> **Mode for the next worker:** Implement U3 then U4 on this same branch, build-verify each, then open one PR.

---

## 1. What this is (plain words first)

We are building **one firmware** that can run on several different little controller boards, where each board only includes the features it actually needs — so the firmware stays small and fast on every board instead of cramming everything into one bloated build.

This task adds **two optional screens** to that firmware:

1. **A colour TFT weight screen** — a graphical screen that shows the live weight (received over WiFi from another board). This is the "viewer" board.
2. **A character LCD** — a simple text screen (the classic 16x2 blue/green display) that shows weight/status.

Each board picks **at most one** of these at build time. A board never carries both, and never carries a screen it doesn't have wired up. The work is "lifting" these two screens from two older code branches onto the current main line and making them **switchable per board** instead of always-on.

**Plain status:** the plumbing that feeds the weight data (the "receiver") is already done and tested. What's left is wiring up the two actual screens and adding the board configurations that turn each one on.

**Technical framing:** the trunk is an ESP8266React-based StatefulService firmware. The two displays become **per-board compile-time feature flags** (`FT_TFT_WEIGHT_SCREEN`, `FT_DISPLAY_LCD`), each defaulting OFF and enabled only in the specific board's `build_flags`. No fat binary: a board that doesn't set the flag compiles the feature out entirely.

---

## 2. Current state — built & verified

All on branch `NewEspAllInOne`, all committed. Working tree also carries unrelated docs noise (` M docs/README.md`, `?? docs/LIZY-RESTORE.md`) — **do NOT include those in any U3/U4 commit or PR.**

| Unit | Commit | What landed | Verified |
|---|---|---|---|
| Plan + queue | `ae21774` | `docs/superpowers/plans/2026-06-14-newespallinone-display-integration.md` + `docs/superpowers/queue.md` | n/a (docs) |
| **U1 scaffold** | `a68e3b0` | `partitions_4mb_ota.csv` (dual OTA app0/app1 @ `0x1C0000`=1.75 MB each + `0x70000`=448 KB spiffs); `lib/framework/Features.h:46-62` adds the 3 flags, all `#ifndef … 0` (OFF by default) | builds green |
| **S3 writer fix** | `87c2605` | `SerialWriterService.{h,cpp}`: `outputSerial()` returns `Stream&` (common base of `HardwareSerial` + S3 USB-CDC `Serial`); `Serial.begin()` gated under `#if ARDUINO_USB_CDC_ON_BOOT`. Pre-existing trunk fix, unrelated to displays but required for S3 to compile. | esp32s3 green |
| **U2 receiver** | `7c16c0b` | `src/examples/remoteweight/{RemoteWeightService.h, .cpp, RemoteWeightState.h}` (whole `.cpp` body under `#if FT_ENABLED(FT_REMOTE_WEIGHT)`). main.cpp: include (`:9`), guarded global (`:129`), guarded construct after `weightForwarderService->begin()` (`:289-302`), guarded `loop()` (`:389-391`). `platformio.ini`: `-D FT_REMOTE_WEIGHT=1` on **esp32s3 only** (line 107). UI: `interface/src/examples/remoteweight/RemoteWeightMonitor.tsx` + additive `ProjectMenu.tsx` / `ProjectRouting.tsx`. | nodemcu_esp32 green (compiled out); esp32s3 green (Flash 67.8%); interface tsc clean — *author-claimed in commit body; not re-run in this audit* |

**Verified live in this audit (read-only):** Features.h flags present & default 0; `partitions_4mb_ota.csv` exact layout above; `platformio.ini` has exactly **5** envs (`esp12e`, `nodemcu_esp32`, `firebeetle32`, `esp32dev`, `esp32s3`) — `FT_REMOTE_WEIGHT=1` only on esp32s3 (line 107), `FT_BOARD_ESP32_2432S028=1` only on firebeetle32 (line 78); **no** `tftdisplay`/`lcddisplay`/`LiquidCrystal` tracked files exist yet; main.cpp construct/loop insertion points confirmed.

**Honest caveat:** "verified/green" everywhere means **compiles + links + fits partition** (and interface `tsc` clean). There are **no automated firmware/UI tests** in this repo. Display behaviour can only be confirmed on physical panels.

---

## 3. What remains

### U3 — TFT weight screen (`FT_TFT_WEIGHT_SCREEN`)

**Source:** `espReadWriteRHYNO:src/examples/display/DisplayService.{h,cpp}` (confirmed present). TFT screen is **hardware-only** — no UI files (plan: "Web route / menu: none").

**Hard dependency:** the TFT screen reads its weight from the U2 receiver, so **every TFT env must set BOTH `FT_REMOTE_WEIGHT=1` AND `FT_TFT_WEIGHT_SCREEN=1`.** Features.h does NOT auto-imply it.

**Checklist:**

- [ ] **Create folder** `src/examples/tftdisplay/` (distinct from LCD's path — avoids the identical-`display/` collision).
- [ ] **Lift + rename files:**
  - `DisplayService.h` → `src/examples/tftdisplay/TftWeightDisplayService.h`
  - `DisplayService.cpp` → `src/examples/tftdisplay/TftWeightDisplayService.cpp`
- [ ] **Rename class** `DisplayService` → `TftWeightDisplayService` (everywhere in both files).
- [ ] **Header guard** `DisplayService_h` → `TftWeightDisplayService_h`.
- [ ] **Swap the compile guard** (RHYNO uses old Arduino style): change `#ifdef HAS_TFT_DISPLAY` → `#if FT_ENABLED(FT_TFT_WEIGHT_SCREEN)` in **both** files. At the **top of the `.cpp`, before the guard**, add `#include <Features.h>` (mirror `RemoteWeightService.cpp:1-2`).
- [ ] **Fix self-include** in `.cpp`: `#include <examples/display/DisplayService.h>` → `#include <examples/tftdisplay/TftWeightDisplayService.h>`.
- [ ] **Keep** `#include <examples/remoteweight/RemoteWeightService.h>` and `#include <ESPFS.h>` — both exist on trunk (`lib/framework/ESPFS.h`).
- [ ] **main.cpp wiring** (all `#if FT_ENABLED(FT_TFT_WEIGHT_SCREEN)`-guarded — the *whole* file is flag-wrapped, so the class doesn't exist when off; an unguarded `new` would fail to link):
  - Include after the remoteweight include (`:9`).
  - Guarded global pointer near `:129`: `TftWeightDisplayService* tftWeightDisplayService;`
  - **Construct INSIDE the existing `FT_REMOTE_WEIGHT` block, immediately after `remoteWeightService->begin()` (after `:300`)** and before `server->begin()` (so the ~4 s boot colour-flash stays under the 10 s WDT): `tftWeightDisplayService = new TftWeightDisplayService(remoteWeightService); tftWeightDisplayService->begin();`
  - Loop call after `:391` (inside/after the `FT_REMOTE_WEIGHT` loop guard, before the `boardDisplayService` loop at `:393`): `if (tftWeightDisplayService) tftWeightDisplayService->loop();`
  - **Do NOT** touch `UartModeService::applyMode()` — TFT is observe-only (see §4).
- [ ] **platformio.ini — add TWO new envs** (renames of RHYNO's `[env:esp32dev]` and `[env:cyd28]`; **do not modify the trunk `[env:esp32dev]` block at lines 80-90**):
  - `[env:esp32dev_tft]` — `board=esp32dev`, `board_build.partitions=partitions_4mb_ota.csv`, littlefs. `lib_deps=${env.lib_deps}` + `bodmer/TFT_eSPI@^2.5.43`. `build_flags=${env.build_flags} -DCONFIG_ESP_TASK_WDT_TIMEOUT_S=10 -D FT_REMOTE_WEIGHT=1 -D FT_TFT_WEIGHT_SCREEN=1` + the **ILI9488 block verbatim from RHYNO** (`USER_SETUP_LOADED`, `ILI9488_DRIVER`, `TFT_WIDTH=320`/`HEIGHT=480`, `MISO=19 MOSI=23 SCLK=18 CS=15 DC=4 RST=2 BL=21`, `LOAD_GLCD`/`FONT2/4/6/7/8`/`GFXFF`, `SMOOTH_FONT`, `SPI_FREQUENCY=4000000`, `TOUCH_CS=22`) + UART remaps `SERIAL2_RX/TX=25/26`, `DIAG_RX/TX=33/32`. **Replace RHYNO's `-DHAS_TFT_DISPLAY=1` with the two `FT_` flags.**
  - `[env:cyd_tft]` — `board=esp32dev`, `partitions_4mb_ota.csv`. Same WDT + `FT_REMOTE_WEIGHT=1 -D FT_TFT_WEIGHT_SCREEN=1` + the **ILI9341 CYD block verbatim** (`ILI9341_DRIVER`, `-DTFT_INVERT_DISPLAY=1`, `WIDTH=240`/`HEIGHT=320`, `MISO=12 MOSI=13 SCLK=14 CS=15 DC=2 RST=-1 BL=21`, `SPI_FREQUENCY=40000000`, `TOUCH_CS=33`) + UART parks `SERIAL2_RX/TX=22/27`, `DIAG_RX/TX=4/16`.
  - For **both**: set `upload_protocol`/`upload_port` to esptool/local COM (RHYNO used espota to live IPs — comment/replace so a build-only sweep never attempts OTA).
  - **NEVER** combine `FT_TFT_WEIGHT_SCREEN` with `FT_BOARD_ESP32_2432S028` in one env (TFT_eSPI vs bb_spi_lcd clash). `cyd_tft` is the **new viewer**; `firebeetle32` stays the BoardDisplayService CYD.
- [ ] **Verify builds:**
  - `python -m platformio run -e esp32dev_tft` (compile+link TFT_eSPI; app must fit `app0` < 1.75 MB — RHYNO was ~1.65 MB, tight; if overflow, trim `LOAD_FONT7/8` first).
  - `python -m platformio run -e cyd_tft` (ILI9341 + invert).
  - `python -m platformio run -e nodemcu_esp32` still green (TFT compiled out).
  - `python -m platformio run -e esp32s3` still green.
  - `git diff` on trunk `[env:esp32dev]` block is **empty**.

### U4 — Character LCD (`FT_DISPLAY_LCD`)

**Source:** `display` branch — `src/examples/display/DisplayService.{h,cpp}`, vendored `lib/LiquidCrystal_I2C/*`, and 5 React tabs `interface/src/examples/display/*.tsx` + `api/display.ts` + `types/display.ts` (all confirmed present on `display`). The `.h` holds **two** classes: `DisplayState` and `DisplayService`.

**Single biggest correctness step:** the `display` branch compiled the LCD **unconditionally** — there is **no flag guard in the source.** U4 MUST add one, or the heavy LiquidCrystal_I2C / WebSocketsClient / MqttPubSub code links into *every* board.

**Checklist:**

- [ ] **Create folder** `src/examples/lcddisplay/`.
- [ ] **Lift + rename:** `DisplayService.{h,cpp}` → `src/examples/lcddisplay/LcdDisplayService.{h,cpp}`.
- [ ] **Rename classes** `DisplayState` → `LcdDisplayState`, `DisplayService` → `LcdDisplayService` (and the base `StatefulService<DisplayState>` → `StatefulService<LcdDisplayState>`, plus every `DisplayState::read/update`).
- [ ] **Header guard** `DisplayService_h` → `LcdDisplayService_h`.
- [ ] **ADD the missing flag guard:** at top of `.cpp` add `#include <Features.h>` then wrap the whole body in `#if FT_ENABLED(FT_DISPLAY_LCD) … #endif` (mirror `RemoteWeightService.cpp`). Guarding the `.cpp` is sufficient since main.cpp construction is guarded.
- [ ] **Fix self-include:** `#include <examples/display/DisplayService.h>` → `#include <examples/lcddisplay/LcdDisplayService.h>`.
- [ ] **Rename endpoint macros** to avoid future clash: `DISPLAY_ENDPOINT_PATH` → `LCD_DISPLAY_ENDPOINT_PATH`, `DISPLAY_SOCKET_PATH` → `LCD_DISPLAY_SOCKET_PATH` — but **keep the on-wire paths** `/rest/display` and `/ws/display`.
- [ ] **Vendor the lib:** lift `lib/LiquidCrystal_I2C/*` to trunk `lib/LiquidCrystal_I2C/`. It resolves via project-local `lib/` even under `lib_compat_mode=strict` (lib/ scanned first). `library.json` lists only atmelavr/espressif8266 but compiles on ESP32. **Confirm it does NOT get pulled into `esp12e` or other non-LCD envs** — gate by enabling `FT_DISPLAY_LCD` only on the intended ESP32 env.
- [ ] **Interface — lift + rename 5 tabs** to `interface/src/examples/lcddisplay/`: `Display.tsx` (default export `Display` → `LcdDisplay`; `useLayoutTitle('Display')` → `('LCD Display')`), `DisplayControl.tsx`, `DisplayInfo.tsx`, `DisplaySerialBridge.tsx`, `DisplayBle.tsx`. Plus `api/display.ts` → `api/lcddisplay.ts` (`DisplayData`→`LcdDisplayData`, `readDisplayData/updateDisplayData`→`readLcdDisplayData/updateLcdDisplayData`) and `types/display.ts` → `types/lcddisplay.ts`.
- [ ] **endpoints.ts:** add `export const LCD_DISPLAY_SOCKET_PATH = `${WS_BASE_URL}display`;` — **normalize to trunk's `${WS_BASE_URL}name` style** (the `display` branch used the older `WEB_SOCKET_ROOT + 'display'`). `DisplayControl.tsx` imports it from `../../api/endpoints`.
- [ ] **Routing/menu (additive, mirror U2):** in `ProjectRouting.tsx` add `import LcdDisplay from '../examples/lcddisplay/Display';` and `<Route path="lcd-display/*" element={<LcdDisplay />} />`. In `ProjectMenu.tsx` add a `TvIcon` import + `<LayoutMenuItem icon={TvIcon} label="LCD Display" to={`/${PROJECT_PATH}/lcd-display`} />` after the Remote Weight item.
- [ ] **main.cpp wiring** (all `#if FT_ENABLED(FT_DISPLAY_LCD)`-guarded):
  - Include after the remoteweight include.
  - Guarded global pointer near `:129`.
  - Construct after `weightForwarderService->begin()` (`:286`) or after the `FT_REMOTE_WEIGHT` block (`:302`), **before** `server->begin()`: `lcdDisplayService = new LcdDisplayService(server, esp8266React->getSecurityManager(), esp8266React->getMqttClient() #if FT_ENABLED(FT_BLE) , nullptr #endif ); lcdDisplayService->begin();`
  - **BLE callback only under `#if FT_ENABLED(FT_BLE)`** — inside the existing `onBleServerStarted` block add `if (lcdDisplayService){ lcdDisplayService->setBleServer(bleServer); lcdDisplayService->configureBle(); }`.
  - Loop call after `:391`: `if (lcdDisplayService) lcdDisplayService->loop();`
  - **Do NOT** touch `UartModeService::applyMode()` — LCD is observe-only.
- [ ] **platformio.ini:** no NEW board env strictly required. For verify, add `-D FT_DISPLAY_LCD=1` to a **temp/scratch ESP32 env** (copy of an esp32dev/devkit env). `lib_deps` need **no** change (`links2004/WebSockets` already at line 44 for `WebSocketsClient.h`). **Do NOT** default-enable LCD on nodemcu_esp32/firebeetle32/esp32s3. **Never** set `FT_DISPLAY_LCD` with `FT_TFT_WEIGHT_SCREEN` or `FT_BOARD_ESP32_2432S028` in one env (one physical panel per board).
- [ ] **Verify builds:**
  - Build the temp LCD env (`-D FT_DISPLAY_LCD=1`): must link `LcdDisplayService` + LiquidCrystal_I2C + WebSocketsClient.
  - `python -m platformio run -e nodemcu_esp32` green (LCD compiled out).
  - Confirm the synchronous **5 s BLE scan compiles OUT when `FT_BLE=0`** (all `connectBleBridge`/`bleNotifyCallback` etc. under `#if FT_ENABLED(FT_BLE)`) so the 4 MB LCD env never pulls the blocking scan.
  - Interface `tsc` clean; `lcd-display` route renders the tabs; `DisplayBle.tsx` tab renders but no-ops when BLE off (acceptable, matches source).

### Final (after both units)

- [ ] **Full local matrix sweep** (CI only covers 3 envs — see §5):
  `python -m platformio run -e esp12e -e nodemcu_esp32 -e firebeetle32 -e esp32dev -e esp32s3`
  then `python -m platformio run -e esp32dev_tft -e cyd_tft` (+ the temp LCD env).
- [ ] **Docs:** update `docs/API-REFERENCE.md` (new endpoints) and `docs/FILE-REFERENCE.md` (new files). Do NOT create per-service `.md` docs.
- [ ] **Version bump** (MINOR — new feature): `interface/package.json` + `src/version.h` (if present) + `CHANGELOG.md` together.
- [ ] **Open the PR** — exclude the docs/README.md + LIZY-RESTORE.md noise.

---

## 4. Architecture & rules

### Per-board compile-time feature-flag pattern (the mechanism)
1. Declare in `lib/framework/Features.h` with `#ifndef FT_X / #define FT_X 0 / #endif` (default OFF) — **already done** for all 3 flags.
2. Enable ONLY in the target env's `build_flags` with `-D FT_X=1`.
3. Wrap **all** flag-dependent code/members/includes **and the main.cpp construct/begin/loop** in `#if FT_ENABLED(FT_X) … #endif` (`FT_ENABLED(f)` is just `f`).
Use a flag only for large/optional/platform-specific features (display drivers, BLE, the receiver) — never for an ordinary runtime setting.

### Single-owner-of-Serial1 contract — displays are OUTSIDE it
`Serial1` (RX=GPIO18, TX=GPIO17) may be open in **exactly one** module at a time. `UartModeService::applyMode()` is the sole arbiter: it suspends all four owners (SerialService reader, SerialWriterService writer, the forwarder, DiagnosticsService) then resumes the active mode's owner. **A module joins this contract ONLY if it directly opens/reads/writes Serial1.**
**Both new displays observe only** — they snapshot weight via `read()` accessors and never touch the UART (exactly like `BoardDisplayService` and `RemoteWeightService`). **Therefore: do NOT add the TFT or LCD service to `UartModeService`, do NOT give them suspend/resume, do NOT add a `setX()` call to `applyMode()`.**

### Service registration pattern (main.cpp)
Services are heap `new`-ed file-scope pointers, constructed in `setup()` in the numbered `[x/10]` boot sequence, `->begin()` after construction, `->loop()` in global `loop()`. Cross-service links wired by setter injection after all services exist. **Construction must run before `server->begin()`.**

### main.cpp construction asymmetry (the trap)
`BoardDisplayService` is `new`-ed **unconditionally** with only its bodies `#if`-guarded. **Do NOT copy that for the displays** — both new display services wrap the *entire file* (constructor included) in the flag, so the class doesn't exist when the flag is off. Their main.cpp `new` **must** be inside `#if FT_ENABLED(FT_…)`, or the link breaks when the flag is off.

### Collision-rename convention
Both source features live at the identical path `src/examples/display/`. To coexist on trunk: TFT → `src/examples/tftdisplay/TftWeightDisplayService`, LCD → `src/examples/lcddisplay/LcdDisplayService`. Folder, class, header-guard, self-include, and (LCD) endpoint-macro identifiers all rename; on-wire REST/WS paths stay as-is.

### Backend service rules (apply to lifted code)
`StatefulService<T>` composes infra as members (never inherits). Never touch `_state` directly — use `read()`/`update()` with origin-tracking. `MqttPubSub` ctor is `(read, UPDATE, …)`. Endpoint naming `/rest/{name}`, `/ws/{name}`, every endpoint gets a security predicate. JSON keys snake_case in C++, camelCase in TS.

---

## 5. Build / flash / verify

### Env table (post-U3; `*_tft` exist only after U3 lands)

| Env | Board | Flash / partitions | Key display flags | Upload |
|---|---|---|---|---|
| `esp12e` | ESP8266 | 4 MB, platform default | none (FT_BLE forced 0) | USB; CI build-only |
| `nodemcu_esp32` | ESP32-WROOM (CH340C) | 4 MB, `partitions_ble.csv` (single factory, no OTA) | **all 3 off** — `default_envs` | esptool COM14 |
| `firebeetle32` | DFRobot FireBeetle | 4 MB, `partitions_ble.csv` | `FT_BOARD_ESP32_2432S028=1` (CYD via bb_spi_lcd); displays off | esptool COM31 |
| `esp32dev` | generic ESP32 | 4 MB, `partitions_custom.csv` (dual 1.5 MB OTA) | all off — **keep UNCHANGED** | inherits espota/USB |
| `esp32s3` | S3 DevKitC-1-N16R8 (16 MB, native USB) | 16 MB, `partitions_ble_ota.csv` (dual 2.5 MB) | `FT_REMOTE_WEIGHT=1`, `FT_BLE=0`, USB-CDC | esptool COM2 (OTA-capable) |
| `esp32dev_tft` *(U3)* | esp32dev + ILI9488 (VSPI) | 4 MB, `partitions_4mb_ota.csv` | `FT_REMOTE_WEIGHT=1 + FT_TFT_WEIGHT_SCREEN=1` | esptool COM |
| `cyd_tft` *(U3)* | esp32dev + Sunton CYD ILI9341 (HSPI, inverted) | 4 MB, `partitions_4mb_ota.csv` | `FT_REMOTE_WEIGHT=1 + FT_TFT_WEIGHT_SCREEN=1` | esptool COM |
| temp LCD env *(U4)* | ESP32 DevKit + I2C 16x2 | 4 MB, `partitions_4mb_ota.csv` | `FT_DISPLAY_LCD=1` | esptool COM |

### Build commands
```
python -m platformio run                       # default env (nodemcu_esp32)
python -m platformio run -e <env>              # one env
python -m platformio run -e <env> -t clean     # clean if stale artifacts suspected
# Pre-PR full sweep:
python -m platformio run -e esp12e -e nodemcu_esp32 -e firebeetle32 -e esp32dev -e esp32s3
python -m platformio run -e esp32dev_tft -e cyd_tft     # after U3
```

### Flash commands
```
Get-Process python* | Stop-Process -Force      # ALWAYS free the COM port first (Windows)
python -m platformio run -e <env> -t upload     # USB; port from env upload_port
# clear_otadata.py runs pre-upload on esptool uploads (no-op on espota / single-factory).
# OTA (esp32s3 + *_tft): comment esptool lines, set upload_protocol=espota,
#   upload_port=<device IP>, upload_flags=--port=8266 --auth=esp-react; enable OTA in web UI.
```

### Interface build (important nuance)
The React UI is **baked into firmware.bin as PROGMEM** (`-D PROGMEM_WWW`), not flashed separately — no `uploadfs` step. `scripts/build_interface.py` runs `npm install && npm run build` (regenerating `lib/framework/WWWData.h`) **only when the target list is empty or contains `upload`.** A pure build-only `pio run` **reuses the committed `WWWData.h`** and does NOT rebuild the UI. So after adding the LCD tabs/routes, regenerate `WWWData.h` by running an `-t upload` or building `interface/` manually; otherwise compile reuses the stale blob. Interface type-check: `npm run` / `tsc` in `interface/`.

### Test matrix & honest test story
**There are NO automated firmware/UI tests.** The only automated gate is `.github/workflows/verify_build.yml`: `pio run -e esp12e -e esp32s3 -e nodemcu_esp32` (compile+link only). **CI does NOT build `firebeetle32`, `esp32dev`, `esp32dev_tft`, `cyd_tft`, or the LCD env** — a broken TFT/LCD env will pass CI. **The pre-PR full local sweep is mandatory.** Per-unit "verify" = (a) target env compiles+links+fits, (b) `nodemcu_esp32` still green with flag off, (c) interface `tsc` clean. Display *behaviour* is manual hardware smoke only.

---

## 6. Gotchas & do-not-touch

**Gotchas:**
- **Framework drift lifting from `espReadWriteRHYNO`/`display`:** trunk's `WebSocketTxRx.h` exposes `cleanupClients()`/`countClients()` but **NOT `getWebSocket()`** (U2 already adapted). **De-risk finding: neither display calls `getWebSocket()`** — TFT uses only `addUpdateHandler`/`read`/`isDisplayEnabled`; LCD's `WebSocketTxRx<…>` uses only the ctor. So both are **LOW drift**. Still: adapt, don't assume RHYNO-only helpers exist.
- **ESP32-S3 USB-CDC:** on S3, `Serial` is USB-CDC, not `HardwareSerial`. Any lifted `Serial.begin(baud,cfg,rx,tx)` or `static_cast<HardwareSerial&>(Serial)` fails to compile. Pattern: return `Stream&`, gate `begin()` under `#if ARDUINO_USB_CDC_ON_BOOT` (see `SerialWriterService.cpp`). (TFT/LCD don't drive Serial1, so low risk — but watch any USB-echo path.)
- **Dual-TFT-owner rule:** never `FT_TFT_WEIGHT_SCREEN` + `FT_BOARD_ESP32_2432S028` in one env (TFT_eSPI vs bb_spi_lcd fight the CYD pins). `cyd_tft` is the new viewer; `firebeetle32` stays the BoardDisplayService CYD.
- **mDNS browser DISABLED:** `main.cpp:310-320` comments out the browser and sets `mdnsBrowser = nullptr;` (its `queryService()` hangs the loop on S3). Don't assume mDNS discovery is live — the receiver relies on the announced `_weighsoft._tcp` + REST POST.
- **Flag dependency:** `FT_TFT_WEIGHT_SCREEN` requires `FT_REMOTE_WEIGHT` — each TFT env must set BOTH (not auto-implied).
- **Flash budget:** TFT_eSPI + 6 fonts + receiver must fit `app0` < 1.75 MB on the 4 MB TFT envs (RHYNO ~1.65 MB, tight). On overflow, trim `LOAD_FONT7/8` first.
- **Boot blocking:** TFT `begin()` runs a ~4 s colour-flash before `server->begin()` — fine under the 10 s WDT, but it must be constructed before `server->begin()`. Lift as-is; note "make optional" as a follow-up.
- **Don't commit the docs noise** (` M docs/README.md`, `?? docs/LIZY-RESTORE.md`).

**Do-not-touch / auto-generated:**
- `lib/framework/WWWData.h` — auto-generated gzipped UI blob; never hand-edit.
- `data/www/` — build output.
- `.pio/`, `.pioenvs/`, `interface/build/`, `interface/node_modules/`, and any `.clang-format` under `.pio/libdeps/**` (third-party). Only the root `.clang-format` governs project C++.
- `scripts/build_interface.py`, `scripts/clear_otadata.py` — wired as PlatformIO `extra_scripts`; affect every build/upload.
- `features.ini`, `factory_settings.ini` — pulled via `extra_configs`; don't duplicate their flags per-env.
- **Trunk `[env:esp32dev]` (platformio.ini:80-90)** — leave unchanged.
- Root must stay clean — no stray `.cpp/.h/.ts/.md`/scripts at repo root.

---

## 7. Agent-integration rules (for an agent continuing this)

- **SERIAL execution, not parallel.** U3 and U4 both edit `src/main.cpp`, `platformio.ini`, `ProjectRouting.tsx`, `ProjectMenu.tsx`. Do them one at a time on this one branch.
- **No worktrees / no parallel checkouts here.** This is a shared working tree with concurrent agents. **Never `git checkout`/switch/worktree/clean/reset** the tree out from under another agent. Commit + push BEFORE any branch switch. If you must isolate, use your own worktree/clone — but for this task, just work in place on `NewEspAllInOne`.
- **Commit + build-verify per unit.** After U3: build `esp32dev_tft`, `cyd_tft`, `nodemcu_esp32`, `esp32s3` green + `tsc` clean → commit `feat(tftdisplay): …`. After U4: build temp LCD env + `nodemcu_esp32` green + `tsc` clean → commit `feat(lcddisplay): …`. Then the full sweep, then PR.
- **Flag matrix (never violate):** TFT envs = `FT_REMOTE_WEIGHT=1 + FT_TFT_WEIGHT_SCREEN=1`. LCD env = `FT_DISPLAY_LCD=1` alone. Never combine `FT_TFT_WEIGHT_SCREEN`/`FT_DISPLAY_LCD`/`FT_BOARD_ESP32_2432S028` in one env. Default envs keep all three OFF.
- **Where new modules live:** services under `src/examples/{tftdisplay,lcddisplay}/` (PascalCase classes); vendored lib under `lib/LiquidCrystal_I2C/`; UI under `interface/src/examples/lcddisplay/`; docs only under `docs/`.
- **main.cpp construct/begin/loop MUST be flag-guarded** (whole-file-wrapped services). Do NOT copy `BoardDisplayService`'s unguarded `new`.
- **Displays are observe-only — never join `UartModeService`.**
- **What NOT to touch:** see §6 do-not-touch list; plus the trunk `[env:esp32dev]` block and the U2/U1 commits.
- **Free COM ports before USB upload** (`Get-Process python* | Stop-Process -Force`).
- **PowerShell on Windows** (`$null`, `$env:VAR`, backtick continuation).

---

## 8. Risks & open questions

**Risks (with the recommended path):**
1. **TFT flash overflow on 4 MB.** *Recommended:* build `esp32dev_tft` first; if `app0` overflows 1.75 MB, trim `LOAD_FONT7/8` before considering a larger partition or 16 MB board.
2. **LCD missing flag guard** (source compiled unconditionally). *Recommended:* this is the #1 correctness step for U4 — add `#include <Features.h>` + `#if FT_ENABLED(FT_DISPLAY_LCD)` around the whole `.cpp` before anything else.
3. **Vendored LiquidCrystal_I2C under `lib_compat_mode=strict`.** *Recommended:* enable `FT_DISPLAY_LCD` only on the intended ESP32 env and confirm the lib is NOT pulled into `esp12e`.
4. **WWWData.h staleness on build-only compiles.** *Recommended:* regenerate via an `-t upload` or a manual `interface/` build after adding LCD UI; don't trust a plain `pio run` to refresh it.

**Open questions (need a product/owner decision — assumptions marked):**
1. **Is `esp32s3` the only receiver board?** *Assumption:* yes for now; the U3 TFT envs additionally set `FT_REMOTE_WEIGHT=1` because the TFT screen depends on the receiver for its weight feed.
2. **Partition target for TFT envs** — `partitions_4mb_ota.csv` (1.75 MB) is the plan default; only revisit if U3 overflows.
3. **Should the LCD route/menu be hidden when `FT_DISPLAY_LCD` is off?** *Assumption:* leave additive/unconditional, matching how U2 left the remote-weight route (device endpoints simply don't exist where the flag is off).
4. **Should the writer-fix (`87c2605`) + U1 scaffold land on the `SerialReaderWriter` trunk independently,** or only via this branch's eventual PR? They are arguably trunk-worthy on their own. *Recommended:* ship via this PR unless the owner wants them split out sooner.
5. **`DisplayBle.tsx` tab is dead unless `FT_BLE=1`** (off on the 4 MB LCD env). *Assumption:* acceptable — the tab renders but device-side BLE no-ops; matches source. Not a wiring bug.