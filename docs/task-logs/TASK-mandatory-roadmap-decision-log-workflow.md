# Task log: Mandatory roadmap / decision-log workflow

**Purpose:** Checklist so firmware investigations stay traceable and do not mix unrelated root causes (e.g. STA Wi‑Fi vs outbound WebSocket TX).

## When changing runtime behavior

1. **Identify subsystem** from logs (heap, `WiFi.isConnected`, `msSinceWfLoop`, AsyncWebSocket vs `WebSocketsClient`, UART availability).
2. **Record evidence** in the appropriate draft decision log under `docs/drafts/` (for this track: [DECISION-LOG-serial-scale-instance-1.md](../drafts/DECISION-LOG-serial-scale-instance-1.md) §9.5 attempt table + §9.3 symptom rows).
3. **Cross-link** if symptoms could be mistaken for another area (example: [DECISION-LOG-wifi-sta-connectivity.md](../drafts/DECISION-LOG-wifi-sta-connectivity.md) for “errno 11 + ws_tx_FAIL” vs STA association).
4. **Update `docs/task-logs/`** for the active product task so operators know what was fixed and how to re-verify (see [TASK-display-serial-bridge-network.md](TASK-display-serial-bridge-network.md)).
5. **Build smoke:** add a row to §9.6 (or equivalent) when running `pio run` / upload for that attempt.

## 2026-04-24 — debug session `76d0d7` (ESP32-S3 COM11: no REST / Live Stream text)

**Do not re-litigate (already shipped + evidence in this file):** **`useWs`** self-echo filter (**not** `origin_id === clientId` for apply); **`useRest`** not clearing `data` before poll; **idle flush** with **printable gate** to stop control-only noise as “dots”; **GPIO18 = UART RX / GPIO17 = UART TX** product pins.

**Current symptom:** Operator reports **no weight / string / data** in **REST** or **Live Stream** on **ESP32-S3**, **COM11**.

**Bench wiring vs UART direction (critical):** Firmware `Serial1.begin(..., RX, TX)` uses **`SERIAL2_RX_PIN = 18`**, **`SERIAL2_TX_PIN = 17`** — i.e. **ESP receives on GPIO18**, **ESP transmits on GPIO17**. **Scale TX (data out)** must go to **ESP GPIO18 (RX)**. **Scale RX** to **ESP GPIO17 (TX)** if the scale listens. If **scale TX is wired to GPIO17**, the ESP is listening on the **wrong pin** and **`available()` stays ~0** — matches “empty UI” without any other bug. Operator note **“TX-17, RX-18”** is only valid if **“TX”** means **ESP TX pin** (to scale RX), not scale TX.

**Hypotheses under test (instrumented, do not fix until `.cursor/debug-76d0d7.log` + optional COM11 USB lines):**

| Id | Hypothesis | Evidence to confirm |
|----|------------|---------------------|
| **H2** | **Serial1 suspended** (diagnostics / UART mode) | USB JSON **`serial_suspended_skip`** every 10 s |
| **H3** | **Idle flush drops** buffer (no printable ASCII before idle) | USB JSON **`idle_flush_skipped_non_printable`** with **`bufLen` > 0** |
| **H4** | **UART idle** — no bytes or only in buffer | USB **`serial_uart_heartbeat`**: **`uartAvail`**, **`lineBufLen`**, **`lastLineLen`** |
| **H-REST** | **API returns empty** `last_line` | Browser ingest **`rest_serial_snapshot`**: **`lastLineLen`**, **`timestamp`** |
| **H-WS** | **WS payload / apply path** | Browser ingest **`ws_serial_payload`**: **`applies`**, **`lastLineLen`**, **`origin_id`** |

**Instrumentation (this push):** `interface/src/api/serial.ts` (REST poll → ingest), `interface/src/utils/useWs.ts` (each serial WS payload → ingest), `SerialService.cpp` (USB `Serial` NDJSON lines above, **sessionId `76d0d7`**). Ingest URL: `http://127.0.0.1:7717/ingest/0bfa9906-aaa9-4a5a-9ff1-54d549900d98`; log file: **`.cursor/debug-76d0d7.log`**.

**Next:** Operator reproduces with **ingest running** + **Serial** page open; then read **`debug-76d0d7.log`** and COM11 capture. **If `H4` shows `uartAvail:0` always** → prioritize **wiring (scale TX→GPIO18)** and **baud**. **If `H2`** → switch **UART mode** to **live** / stop diagnostics. **If `H3` + non-zero `bufLen`** → scale sends **binary/control-only** without CR/LF — adjust **baud** / **protocol** or terminator, not random UI patches.

### Analysis — repro log lines 1–20 (session `76d0d7`, browser NDJSON)

- **`H-REST` CONFIRMED:** Every `rest_serial_snapshot` shows **`lastLineLen:0`**, **`timestamp:0`**, **`weightLen:0`**, **`baud:9600`** (e.g. lines 3, 5–7, 8–18) → **StatefulService serial state on device is empty**; not a React render-only glitch.
- **`H-WS` — client filter REJECTED as root cause:** `ws_serial_payload` rows show **`applies:true`** and `origin_id:"websocket"` (e.g. lines 1–2, 4, 6, 14, 19) with empty payload → **WebSocket path is applying** updates; **no evidence** the UI is dropping `serial_hw` due to `origin_id` mismatch (that bug was already fixed in an earlier pass).
- **USB-only hypotheses (`H2`/`H3`/`H4`):** Not present in **`.cursor/debug-76d0d7.log`** (browser file). **Follow-up shipped:** REST JSON now includes **`dbg_uart_rx_avail`**, **`dbg_suspended`**, **`dbg_serial_started`**, **`dbg_line_buf_len`** (runtime-only, not persisted) so the **next** ingest capture classifies **suspended vs. no UART bytes vs. buffered non-terminated data** without COM11 capture.
- **LittleFS default:** Added **`data/config/uartMode.json`** with **`{"mode":"live"}`** so **`uploadfs`** / factory images default **UART mode** to **live monitoring** (avoids a device stuck in **diagnostics** owning Serial1). Existing boards with an old **`/config/uartMode.json`** still need **UI switch to live** or **re-upload filesystem** once.

### Scale → RS232 / serial → ESP32-S3 — how a string reaches REST / Live Stream

1. **Physical:** Scale **asynchronous serial** (often **RS-232** ±V on a DE-9, or **TTL** 3.3 V/5 V on a header) must present **logic-level signals the ESP can read**. **ESP32-S3 GPIO is 3.3 V tolerant** — **do not** wire **true RS-232 levels** directly to GPIO; use a **MAX3232** (or similar) **RS-232 ↔ TTL** converter, then **TTL TX (from converter, toward ESP)** → **ESP UART RX**.
2. **Pin roles (this firmware):** `Serial1.begin(baud, config, RX, TX)` with **`RX = GPIO18`**, **`TX = GPIO17`** (`SerialService.h`). **Data from the scale into the ESP** must enter on **GPIO18** (the UART **receive** pin). **ESP → scale** (commands, rare) uses **GPIO17**.
3. **Firmware read path:** `main.cpp` calls **`serialService->loop()`** every iteration. **`SerialService::loop()`** reads **`Serial1.available()`**, consumes bytes, assembles **`_lineBuffer`** until **CR/LF**, or **idle-flushes** (see `SERIAL_LINE_IDLE_FLUSH_MS`) when there is **printable ASCII** in the buffer. On a complete line it calls **`update(..., "serial_hw")`**, which updates **`SerialState.lastLine` / `weight` / `timestamp`**.
4. **Fan-out:** **`StatefulService`** + **`WebSocketTxRx`** + **`HttpEndpoint`** expose **`GET /rest/serial`** and **`/ws/serial`**. The **browser** polls REST (`readSerialData`) and/or opens the **WebSocket**; the UI shows **`last_line`** (after optional sanitization for control chars).

### Runtime verdict — `.cursor/debug-76d0d7.log` with `dbg_*` (REST ingest, post-fix capture)

**Evidence (representative: line 2; pattern repeats through line 109):** `rest_serial_snapshot` includes **`dbg_uart_rx_avail":0`**, **`dbg_suspended":0`**, **`dbg_serial_started":1`**, **`dbg_line_buf_len":0`**, **`lastLineLen":0`**, **`timestamp":0`**, **`baud":9600`**.

| Check | Result | Implication |
|--------|--------|-------------|
| **`dbg_serial_started`** | **1** | **`Serial1`** was **`begin()`**’d — not a “UART never started” code path. |
| **`dbg_suspended`** | **0** | **H2 REJECTED** for this capture: **SerialService** is **not** yielding **`Serial1`** to **Diagnostics** / UART mode suspend. |
| **`dbg_uart_rx_avail`** | **0** (repeated) | At loop snapshot times, **hardware RX FIFO reports no pending bytes** on **`Serial1`** mapped to **GPIO18**. |
| **`dbg_line_buf_len`** | **0** | No partial line stuck in **`_lineBuffer`** — consistent with **no ingress**, not “idle flush printable gate ate the line” (that would need **`bufLen` > 0** before flush). |

**Conclusion (from logs, not speculation on unseen wiring):** The **empty REST / Live Stream** symptom matches **no UART bytes reaching the ESP on the configured RX pin** for the duration logged. **Do not** loop back into **WebSocket client** or **idle-flush dots** fixes for this capture — those subsystems are **not** the first failure here.

**Operator checklist (hardware / bench, outside repo):**

1. **Crossover:** **Scale TX** (line driver **out** from scale) → **ESP GPIO18** (**UART RX**). **Scale RX** ← **ESP GPIO17** if needed. **GND** common. (**Scale TX → GPIO17** is **ESP TX** — wrong direction for **receiving** weight strings.)
2. **RS-232 vs TTL:** If the cable is **true RS-232**, use a **level shifter** to **3.3 V TTL** before GPIO.
3. **Baud / format:** Match **UI / `serialConfig.json`** (e.g. **9600 8N1**) to the scale manual; wrong baud can sometimes still show **non-zero** garbage — here **`available()` stays 0**, so **wiring / levels / no TX** dominate.
4. **Confirm the scale is transmitting** (continuous or on-print); some models need **print** or **poll** on the **RX** line.

**Open question (for operator):** Is the bench **native TTL** from the scale, or **DE-9 RS-232**? (Determines whether **MAX3232** is mandatory.)

### Post-flash repro — `.cursor/debug-76d0d7.log` lines 1–22 (same signature)

- **`rest_serial_snapshot`** again shows **`dbg_uart_rx_avail":0`**, **`dbg_serial_started":1`**, **`dbg_suspended":0`**, **`dbg_line_buf_len":0`**, **`lastLineLen":0`** (e.g. lines 1–3, 5–22) after firmware flash → **no new subsystem**; still **no UART RX traffic on GPIO18** for this bench window.
- **UI follow-up (evidence-aligned, not a UART “fix”):** When REST returns that exact **`dbg_*`** pattern and **`last_line`** is empty, **`SerialRest.tsx`** shows an on-page **wiring / levels** hint so operators are not blocked on log ingest alone. Reflash still required to ship the bundled interface.

## 2026-04-24 update — serial “dots / empty line” root cause (firmware, new ESP repro)

- **Operator evidence (screenshots):** **REST** “Raw” shows a run of **middle-dot / control-sanitized** characters; **Configuration** preview **Last line** `...`; **Live Stream** timestamps with little or no visible payload; **regex empty** → “No weight extracted” is expected until configured.
- **Repro classification:** Same symptoms on a **replacement ESP32-S3** → treat as **firmware + UI + bench assumptions**, not a single bad MCU.
- **UI layer (`interface/src/examples/serial/formatSerialTimestamp.ts`):** `sanitizeLastLineForDisplay` maps **`U+0000`–`U+001F`** (and related controls) to **`U+00B7` (middle dot)**. A **REST “Raw”** field full of dots therefore means **`last_line` is mostly control / NUL / line-noise bytes**, not necessarily “missing REST.”
- **Ingress layer (`SerialService.cpp`):** Lines are committed on **CR/LF** or on **UART idle flush** (`SERIAL_LINE_IDLE_FLUSH_MS`, default **500 ms**) when the scale sends **no line terminator**. **Floating RX** or **wrong baud** often produces **short bursts of low bytes**; idle flush was **publishing those as `last_line`**, which the UI then renders as **dots**. **Fix shipped:** idle flush now **only publishes** if the pending buffer contains **at least one printable ASCII** (`0x20`–`0x7E`); **terminated lines** are still published **verbatim** (including binary preludes if the device still sends CR/LF).
- **WebSocket client (`interface/src/utils/useWs.ts`):** Payloads must **not** require `origin_id === clientId` (server uses **`websocket`**, **`serial_hw`**, etc.). Apply updates unless `origin_id` is the **self-echo** of the client’s own `websocket:<id>`; debug ingest to localhost was **removed** after this rule was validated.
- **Cross-environment UART pins (product decision):** Firmware uses **GPIO18 (RX) and GPIO17 (TX)** for **Serial1** on **all ESP32 builds** (`SerialService.h`, `DiagnosticsService.h`) so **node32s**, **esp32dev**, and **esp32s3** share the same harness. An earlier pass tried **GPIO16 RX** on classic ESP32 to match an old doc table; **that split was reverted** per operator requirement (“pins still need to be **17 and 18**”). [`docs/PIN-CONFIGURATION.md`](../PIN-CONFIGURATION.md) **node32s / esp32dev** section now matches **18/17**.
- **`[env:esp12e]` (ESP8266):** `SerialService::applySerialConfig()` is wrapped in **`#ifdef ESP32`** only — **no `Serial1.begin(...)`** on 8266 in this service path, so **hardware serial scale monitoring is not implemented for esp12e** in the same way as ESP32 targets. Treat **esp12e** as a **different product / porting tier** in decision logs, not a drop-in substitute for ESP32 serial benches.
- **Quick `platformio.ini` contrast (serial-relevant):** **`esp32s3`** adds **`-D FT_BLE=0`**, **`ARDUINO_USB_CDC_ON_BOOT=0`**, **`ARDUINO_USB_MODE=0`** (USB-**serial** monitor path vs native CDC), **`partitions_ble_ota.csv`**, board **`esp32-s3-devkitc-1`**. **`node32s` / `esp32dev`** keep default **`FT_BLE=1`** from `features.ini`, different **partition CSVs**, and **no** S3 USB build flags — **UART pin mux is the material serial difference** besides chip errata.
- **Re-verify:** Reflash target env, connect scale **TX → GPIO18**, **GND common**, **9600 8N1** (or scale’s rate), ensure **UART mode = live** not diagnostics-only; confirm **REST Raw** shows **ASCII digits** from the scale, not **control-only** bursts (**GPIO17/18** on **node32s**, **esp32dev**, and **esp32s3**).
- **Build smoke (agent, same push):** `python -m platformio run -e node32s -e esp32s3` — **both SUCCESS** after pin mux + idle-flush changes. **`esp12e`** still **fails** compile with **`BLEServer.h: No such file or directory`** when **`FT_BLE=1`** (pre-existing env gap; not introduced by this serial work).

## 2026-04-24 update — bench flash on COM18 (hardware vs firmware isolation)

- **Goal:** Rule out **stale or wrong firmware** when diagnosing serial / Wi‑Fi / UI symptoms. After this flash, if behavior matches expectations, prior issues were likely **firmware or bundled web UI**; if problems persist unchanged, bias investigation toward **UART wiring, scale, power, antenna, or host COM/driver** (still document which subsystem each symptom belongs to).
- **Host prep (Windows):** Per [platformio-upload](../../.cursor/rules/platformio-upload.mdc), **`Get-Process python* | Stop-Process -Force`** was run before upload so nothing held **COM18** open.
- **Command (does not require editing `platformio.ini`):**
  - `python -m platformio run -e esp32s3 -t upload --upload-port COM18`
- **Result:** **SUCCESS** (full build including `scripts/build_interface.py` → `npm run build`, then `esptool` write + verify + hard reset). Total wall time for this run **~2 m 46 s** (interface build dominated).
- **esptool / chip evidence (from log):** **ESP32-S3 (QFN56) revision v0.2**; **WiFi, BLE, Embedded PSRAM 8MB (AP_3v3)**; **40 MHz** crystal; **USB mode: USB-Serial/JTAG**; **MAC `44:1b:f6:8c:bd:8c`**; flash regions written from bootloader stub through app partition; **hash verified** on written blocks.
- **Repo state note:** `[env:esp32s3]` in `platformio.ini` still lists **`upload_port = COM11`** as the saved default; **COM18** was selected only via **`--upload-port`** for this bench. Operators should use their enumerated port (`pio device list` / Device Manager).
- **Build noise (non-blocking):** CRA compile **warnings** (trailing spaces, `max-len`, `no-control-regex` in `formatSerialTimestamp.ts`, etc.); GCC **`<command-line>: warning: "FT_BLE" redefined`** / **`"ARDUINO_USB_MODE" redefined`** (expected when env flags override inherited defines). None blocked link or upload.
- **LittleFS:** This command was **firmware `upload` only**. If Wi‑Fi / AP defaults on disk must match `data/config/` in the tree, run **`pio run -e esp32s3 -t uploadfs`** (or factory flow) when that gap matters for the test.
- **Cross-link:** Operational post-flash checks remain in [TASK-display-serial-bridge-network.md](TASK-display-serial-bridge-network.md).

## 2026-04-24 update — hardware target + task log

- **Serial Reader on ESP32-S3 (COM2):** The active firmware image is still the **Weighsoft Serial** app (`src/main.cpp` — serial ingress + forwarder + local Async WebSocket). For USB flashing to a specific bench port, `[env:esp32s3]` in `platformio.ini` now uses **`upload_port = COM2`** (was `COM11`) with **`upload_protocol = esptool`**. This is a **port binding** for upload only; it does not change runtime behavior. Operators on other USB ports should override locally or temporarily set `upload_port` / use `pio run -e esp32s3 -t upload --upload-port <PORT>`.
- **Task log cross-link:** [TASK-display-serial-bridge-network.md](TASK-display-serial-bridge-network.md) §2026-04-24 documents the same change and re-verification steps (operator section remains source of truth for post-flash checks).
- **Build smoke (2026-04-24):** `pio run -e esp32s3 -t upload` — **compile/link OK** for `esp32s3`; **upload not completed** in agent session (`COM2` missing / not accessible: `FileNotFoundError`). Does not block decision-log content; append a successful on-bench upload row to [DECISION-LOG-serial-scale-instance-1.md](../drafts/DECISION-LOG-serial-scale-instance-1.md) §9.6 when an operator confirms flash on hardware.

## 2026-04-24 update — upload vs firmware (classification)

- **Separate failure classes:** **USB serial / `esptool` upload** (ROM bootloader, COM port, drivers, cable, BOOT/RESET) is **not** the same subsystem as **STA Wi‑Fi**, **outbound `WebSocketsClient`**, or **AsyncWebSocket** queue pressure documented in §2026-04-22. Do not merge upload troubleshooting into the serial-bridge runtime attempt table without labeling it **tooling/host**.
- **In-repo hardware spec for `esp32s3`:** See [TASK-display-serial-bridge-network.md](TASK-display-serial-bridge-network.md) section **“ESP32-S3 upload failures: hardware & driver compatibility”** — summarizes `board = esp32-s3-devkitc-1`, **`partitions_ble_ota.csv` requires ≥8 MB flash**, `esptool` expectations, **`ARDUINO_USB_*` flags** (runtime serial only), Windows **VID** → driver mapping, dual-USB jack note, and **`flash_id` / `pio device list`** commands for evidence.
- **Agent limitation:** Driver correctness for a specific PC cannot be verified from the repository; only **Device Manager Hardware Ids** + successful **`esptool … flash_id`** on the bench close that loop.

## 2026-04-24 update — vendored Windows USB drivers

- **Location:** [`docs/vendor-drivers/windows/`](../vendor-drivers/windows/) — contains **Espressif** `ESP32.USB.JTAG-v6.1.7600.16386.zip` and **Silicon Labs** `CP210x_Universal_Windows_Driver.zip`, plus [`README.md`](../vendor-drivers/windows/README.md) and [`SOURCES.txt`](../vendor-drivers/windows/SOURCES.txt) (URLs + install summary).
- **Rationale:** Supports **offline** driver install for the most common ESP32-S3 bench cases (`VID_303A` native USB path, **CP210x** UART bridge). **WCH CH343/CH340** is documented in README with an external link only (not mirrored in-repo in this pass).
- **Cross-link:** [TASK-display-serial-bridge-network.md](TASK-display-serial-bridge-network.md) subsection **“Windows drivers vendored in-repo”** ties this to the upload-failure investigation.

## 2026-04-24 update — ESP32-S3 STA + captive soft AP (Wi‑Fi stack)

- **Symptom class:** ESP32-S3 “no internet” / unreliable setup access — treat as **STA + soft AP + radio coexistence**, not serial-forwarder or outbound WebSocket issues until logs prove otherwise.
- **`[env:esp32s3]` (`platformio.ini`):** Added **`-D FT_BLE=0`** so this board profile does not compile BLE (see `features.ini` default `FT_BLE=1` for other ESP32 envs). **Rationale:** BLE and Wi‑Fi share 2.4 GHz; starting `BLEDevice::init` while STA is joining often degrades or blocks association on ESP32-class boards. Re-enable BLE for S3 only if you add a coexistence strategy and bench validation.
- **LittleFS defaults (`data/config/`):** New **`apSettings.json`** with **`provision_mode`: 0** (`AP_MODE_ALWAYS`) so the **soft AP + captive DNS** stay up even when STA is connected — operators can always reach **`192.168.4.1`** for Wi‑Fi setup. **`bleSettings.json`** default **`enabled`: false** so filesystem-first boots avoid advertising BLE when the firmware build includes BLE on other targets.
- **Framework (`lib/framework/`):** `APSettingsService::startAP()` on **ESP32** now sets **`WIFI_AP_STA`** when the chip is already in **STA** mode before `softAP`, matching Espressif expectations for concurrent AP + STA. `WiFiSettingsService::manageSTA()` calls **`WiFi.setSleep(false)`** before **`WiFi.begin`** on ESP32 to reduce modem-sleep-related disconnects. **`APSettings::update`** fixed the **`provision_mode`** validation switch to use **`newSettings.provisionMode`** (was incorrectly reading stale **`settings`**).
- **Cross-link:** Operational checks remain in [TASK-display-serial-bridge-network.md](TASK-display-serial-bridge-network.md); append STA verification notes there when bench evidence exists.
- **LittleFS:** New defaults under `data/config/` require **`pio run -e esp32s3 -t uploadfs`** (or factory reset + image that includes the updated FS) on devices that already had a populated **`/config/`** partition; firmware-only `upload` does not always refresh LittleFS.

## 2026-04-24 update — Serial “no strings” UI vs other subsystems (debug `76d0d7`)

- **Classify first:** Empty **Serial** REST/WS is **UART ingress + line assembly + HTTP/WS fanout**, not the same as **WeightForwarder** outbound client failures or **AsyncWebSocket** queue aborts unless logs show `serial_hw` broadcasts firing with empty `last_line` from another path.
- **Operator evidence:** [TASK-display-serial-bridge-network.md](TASK-display-serial-bridge-network.md) §**2026-04-24 — Serial UI empty + USB log silent** documents **GPIO17/18** roles, **TX/RX crossover** pitfall, line-terminator requirement, **Diagnostics** `Serial1` suspend, and **session `76d0d7`** instrumentation (browser ingest + firmware heartbeat JSON on USB Serial).
- **Workflow:** Do not treat as “fixed” until **`debug-76d0d7.log`** (browser) and/or USB NDJSON heartbeats confirm which hypothesis holds; then append a row to the serial-scale decision log §9.6 / §9.5 as appropriate.

### Post-repro (session `76d0d7`) — applied code changes

- **Evidence file:** `.cursor/debug-76d0d7.log` — REST snapshots showed **`lastLineLen:0`**, **`timestamp:0`** while WS showed **`connected` + `hasPayload`** with **`lastLineLen:0`** → classify as **ingress / line assembly + WS state merge**, not generic “network stability.”
- **Shipped fixes:** `interface/src/utils/useWs.ts` (always take latest payload for matching `origin_id`); `interface/src/utils/useRest.ts` (no pre-fetch `data` clear); `src/examples/serial/SerialService.cpp` (300 ms UART idle flush for lines without CR/LF). **Debug ingest + Serial NDJSON heartbeats** left in place for one verification pass.

### 2026-04-24 — Serial display parity (UI vs ingest evidence)

- **Evidence:** `.cursor/debug-76d0d7.log` later lines show **`lastLineLen` > 0** while operator screenshots still showed **blank Live Stream text** or **REST “dots”** — classify as **UI sanitization gap + optional idle noise**, not “no serial bytes.”
- **Changes:** **`SerialWebSocket.tsx`** now uses **`sanitizeLastLineForDisplay`** for the streamed line body (parity with REST). **`SerialService.cpp`** idle flush default **500 ms**; **printable-only idle publish was reverted** (see task log) after an empty **`lastLineLen`** reproduction capture.

## 2026-04-22 update

- Added **§9.5 #15** to serial-scale decision log: defer `forwardWeight` out of `SerialService::update` handler chain; documented COM10 log pattern (`serialMs` ~18415 with `msSinceWfLoop` ~8403 at `sendTXT_false`).
- Extended Wi‑Fi draft log changelog + evidence trail to **#15** so readers do not attribute outbound client starvation to STA credential bugs.
