# Task log: Display / serial bridge / network stability

**Scope:** ESP32 “Weighsoft Serial” firmware — serial scale ingress, local Web UI (`AsyncWebSocket`), and outbound weight forwarder (`WebSocketsClient`).

## Current issue class (2026-04-22)

Runtime evidence (Serial monitor, session `36885a`) showed:

- `[AsyncWebSocket.cpp:427] _queueMessage(): Too many messages queued: closing connection` on the **device-hosted** WebSocket server.
- `[E][WiFiClient.cpp:429] write(): fail … errno: 11` and `[WeightForwarder] ws_tx_FAIL` on the **outbound** WebSocket client.
- `AGENTDBG` `main_loop_timing` with **`serialMs` in the thousands to tens of seconds**, and `sendTXT_false` with **`msSinceWfLoop` in the thousands** — meaning `WeightForwarderService::loop()` (including `_wsClient->loop()`) was not entered for long intervals.

## Fix applied (firmware)

**Attempt #15** (see [DECISION-LOG-serial-scale-instance-1.md](../drafts/DECISION-LOG-serial-scale-instance-1.md) §9.5): `WeightForwarderService::onSerialWeightUpdate` no longer calls `forwardWeight()` synchronously from inside `SerialService::update()` propagation. It stages `lastLine` / `weight`; `flushPendingSerialForward()` runs from `WeightForwarderService::loop()` after `_wsClient->loop()` on ESP32 so outbound TCP/WS is drained before `sendTXT`.

**Attempt #16** (same log §9.5): Post-#15 Serial evidence (`terminals/38.txt`) still showed **`forwarderMs` multi-second**, low heap, **`/ws/weightforwarder`** pressure from high-rate **`internal`** success updates, and **`serial_hw`** broadcasts too tight for the local AsyncWebSocket queue. Changes: `publishForwardSuccessRuntimeState()` (500 ms max **propagating** `internal` when already healthy), `AGENTDBG` **`ws_client_loop_slow`** (`hypothesisId=L`) if `_wsClient->loop()` ≥200 ms, **`serial_hw`** broadcast minimum interval **400 ms** in `WebSocketTxRx.h`.

**Attempt #18** (same log §9.5): Post-#16 Serial still hit **`abort()`** on core **1**. **PlatformIO `addr2line`** on `firmware.elf` showed **`std::bad_alloc`** in **`AsyncWebServerResponse::addHeader`** (ESPAsyncWebServer) from the **AsyncTCP** task — concurrent **HTTP** needs contiguous RAM while WS fanout fragments the heap. Changes: skip **all** WS **broadcasts** in `WebSocketTxRx.h` when **`ESP.getMaxAllocHeap()`** is below **28672** (override via `WS_TXRX_MIN_MAX_ALLOC_FOR_BROADCAST`), log **`ws_broadcast_skipped_low_max_alloc` (`hypothesisId=P`)**; **`serial_hw`** broadcast spacing **650 ms**; **`internal`** propagating interval **750 ms** in `publishForwardSuccessRuntimeState()`.

## Operator verification

After flashing, confirm for several minutes of live scale traffic:

1. No `serialMs` spikes comparable to pre-fix multi-second values in `AGENTDBG` `main_loop_timing`.
2. No burst of `WiFiClient.cpp:429` errno 11 immediately before `ws_tx_FAIL`.
3. Optional: UI “Network Error” on `/project/weightforwarder/config` should clear when REST reaches the device; `-1` HTTP auth from a **separate** test harness is a different failure class (server reachability / TLS / credentials).

## 2026-04-24 — ESP32-S3 USB flash (Serial Reader, COM2)

**Goal:** Build and upload the same **Weighsoft Serial** firmware (Serial Reader stack: `SerialService`, `WeightForwarderService`, web UI) to an **ESP32-S3** attached on **Windows COM2**, using PlatformIO environment **`[env:esp32s3]`** (line ~92 in `platformio.ini`).

**`platformio.ini` change:** `upload_port` under `[env:esp32s3]` was set from **`COM11`** to **`COM2`** so the default USB `esptool` upload targets the device on that port. **`upload_protocol = esptool`** remains (USB serial flash). Root `[env]` still defines OTA defaults, but this env overrides them for local bench uploads.

**Why document:** Other benches may use different COM IDs; the checked-in `COM2` matches this machine’s connected ESP32-S3. If upload fails with “port busy,” close Serial Monitor and any Python holding the port, then retry (see workspace PlatformIO upload rule: stop Python `platformio` helpers before upload if needed).

**Re-verify** after flash: use the [Operator verification](#operator-verification) list above; confirm serial monitor shows the Weighsoft Serial banner and expected `AGENTDBG` behavior under load.

**Agent run (2026-04-24):** `python -m platformio run -e esp32s3 -t upload` after the `platformio.ini` change. **Build completed** (`firmware.bin` for ESP32-S3; flash ~85.7% of app partition). **Upload step failed** in this environment: `Could not open COM2` / `FileNotFoundError` — no serial device present at COM2 here (device unplugged, different COM number, or driver). **Operator action:** With the ESP32-S3 on COM2, close anything using the port, run the same command (or IDE upload); confirm **Device Manager** shows **COM2** for the board.

## 2026-04-24 — ESP32-S3 upload failures: hardware & driver compatibility (in-repo verification)

**Problem statement (operator):** Upload to ESP32-S3 still fails after wiring cleanup, USB port changes, BOOT/RESET download mode, and trying **multiple** ESP32-S3 boards. Need to confirm **drivers** and **hardware vs project expectations**.

### What this repository actually targets (`[env:esp32s3]`)

| Item | Value in tree | Why it matters |
|------|----------------|----------------|
| PlatformIO `board` | `esp32-s3-devkitc-1` | Maps to Espressif **ESP32-S3-DevKitC-1** class boards (PlatformIO / Espressif board profile: **8 MB flash**, **no PSRAM** in typical `N8` profile). |
| `board_build.partitions` | `partitions_ble_ota.csv` | Header in file states layout needs **8 MB+ flash** and **does not work on 4 MB** (would boot-loop / mis-flash if forced). |
| `upload_protocol` | `esptool` | Expects a **serial bootloader** path: UART bridge COM port, or a supported USB-serial/JTAG interface that exposes a COM port on Windows. |
| Firmware `Serial` / USB flags | `-DARDUINO_USB_CDC_ON_BOOT=0`, `-DARDUINO_USB_MODE=0` | Affects **runtime** Arduino `Serial` routing (UART vs USB-CDC after your sketch runs). **Does not replace** the need for a working **ROM bootloader** connection over the port `esptool` uses for upload. |

**Conclusion (compatibility):** Any **ESP32-S3** with **≥ 8 MB flash** and a **working serial/USB connection to the chip’s UART0 bootloader** is **architecturally compatible** with this build. **4 MB-only** S3 modules or boards that only expose **Ethernet/JTAG without a usable serial path** are **not** a drop-in match for this `env` without changing `board` / partition CSV and upload method.

### Drivers — what we can and cannot verify from the repo

- **Cannot verify remotely:** Whether Windows has loaded the correct driver for **your** cable, hub, and board revision (no access to Device Manager or VID/PID on the agent side).
- **Verified from prior operator evidence (screenshot, same track):** **“Unknown device”** (yellow bang) alongside **“USB Serial Device (COMx)”** strongly suggests **one USB function is enumerated** while **another composite interface** still needs a driver (common on dual-USB ESP32-S3 devkits and on **Espressif USB-JTAG/Serial** `VID_303A` devices).

**Windows driver identification (operator):**

1. Device Manager → **Unknown device** → **Properties** → **Details** → **Hardware Ids**.
2. Match typical patterns (not exhaustive):
   - **`VID_303A`** (Espressif) → use **Espressif Windows USB drivers** / ESP-IDF driver bundle so **USB JTAG/serial debug unit** and related interfaces install cleanly.
   - **`VID_10C4&PID_EA60`** (Silicon Labs) → **CP210x** VCP driver from Silicon Labs.
   - **`VID_1A86`** (WCH) → **CH340/CH343** driver from WCH.
3. After install: **unplug/replug**, confirm **no** persistent yellow bang on a device that should accompany the board.

**Physical USB socket:** On many ESP32-S3 DevKit boards there are **two** USB-C ports — one wired to the **USB-UART bridge** (usual `esptool` COM port) and one to the **native USB** (different driver stack). If upload always fails on one jack, try the **other** while watching which COM appears / disappears.

### Commands for evidence (operator → paste into task / decision log)

```powershell
python -m platformio device list
python "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py" --chip esp32s3 -p COM14 flash_id
python -m platformio run -e esp32s3 -t upload --upload-port COM14
```

Replace `COM14` with the COM shown for the board. If `flash_id` fails, capture **full** stderr (port busy, permission, timeout, “Failed to connect”).

### If multiple boards all fail upload on this PC

Treat as **host-side** until `flash_id` succeeds on at least one board: try **another USB cable** (data-capable), **direct motherboard USB2 port**, disable aggressive security software on COM access, and re-check **Unknown device** resolution above.

### 2026-04-24 — Windows drivers vendored in-repo (offline install)

**Action taken:** Downloaded and committed vendor driver archives under [`docs/vendor-drivers/windows/`](../vendor-drivers/windows/) so the bench can install without hunting URLs during an outage.

| File | Purpose |
|------|---------|
| `ESP32.USB.JTAG-v6.1.7600.16386.zip` | Espressif **USB JTAG / serial** stack (typical for **`VID_303A`** and native-USB ESP32-S3 interfaces that show as Unknown / composite). |
| `CP210x_Universal_Windows_Driver.zip` | Silicon Labs **CP210x** VCP for UART bridge chips on many DevKit **UART-labeled** USB ports (`VID_10C4` / `PID_EA60` family). |

**Supporting files:** [`README.md`](../vendor-drivers/windows/README.md) (install hints, WCH note), [`SOURCES.txt`](../vendor-drivers/windows/SOURCES.txt) (exact download URLs + date).

**Not bundled:** WCH **CH340/CH343** — use official [WCH CH343 driver page](https://www.wch-ic.com/downloads/CH343SER_ZIP.html) when Hardware Ids indicate WCH (`VID_1A86` / similar).

**Why:** Prior Device Manager evidence (Unknown device + COM port) pointed at **missing or partial Windows drivers**; hosting zips in-repo documents what was fetched and gives a repeatable offline path.

## 2026-04-24 — ESP32-S3 Wi‑Fi: STA reliability + always-on setup AP

**Operator expectation:** Device joins **STA** (internet / LAN) when credentials are valid, and exposes a **soft AP** for configuration without losing access after STA connects.

**Changes (summary):** See [TASK-mandatory-roadmap-decision-log-workflow.md](TASK-mandatory-roadmap-decision-log-workflow.md) §**2026-04-24 — ESP32-S3 STA + captive soft AP**. Highlights: **`[env:esp32s3]`** sets **`FT_BLE=0`**; **`data/config/apSettings.json`** defaults **`provision_mode` 0** (AP always); **`data/bleSettings.json`** defaults BLE **off**; framework sets **`WIFI_AP_STA`** before **`softAP`** when already in STA mode, **`WiFi.setSleep(false)`** before STA connect, and fixes **`APSettings::update`** provision-mode validation.

**Re-verify:** Push LittleFS when `data/config` changed: `python -m platformio run -e esp32s3 -t uploadfs` (or erase flash + full upload) so **`apSettings.json`** reaches the device. Then connect to the soft AP (**password** default **`esp-react`** unless changed), open **`http://192.168.4.1`**, confirm STA SSID/password in Wi‑Fi settings; confirm outbound paths (NTP / forwarder) once STA shows connected.

## 2026-04-24 — Serial UI empty + USB log silent (debug session `76d0d7`)

**Symptoms (operator):** REST **“No data received yet”**, WebSocket **“Waiting for data…”**, and **USB serial monitor shows no logs** while the web UI loads at a LAN IP (e.g. `10.45.71.248`).

**UART pinout (firmware `SerialService`):** `Serial1.begin(..., rx, tx)` uses **`SERIAL2_RX_PIN = 18`**, **`SERIAL2_TX_PIN = 17`** (`src/examples/serial/SerialService.h`). Convention: **peripheral TX → ESP RX (GPIO18)**; **peripheral RX → ESP TX (GPIO17)**; **GND common**; **3.3 V logic** (many scales are 5 V TTL — level shifting may be required; 5 V on GPIO can damage the S3).

**Bench checklist (wiring labels ambiguous):** If the cable lists **“TX → PIN17”** meaning *device transmit wire to ESP pin 17*, that ties **TX to ESP TX** (both outputs) — **swap** so **device TX → GPIO18 (ESP RX)**. Confirm **baud / parity** in **Serial → Configuration** matches the scale. Lines only publish to REST/WS after **`\n` or `\r`** (`SerialService.cpp`); continuous stream without line breaks will keep **`last_line`** empty.

**Diagnostics vs Serial:** `DiagnosticsService` calls **`suspendSerial()`** when a diagnostic test owns `Serial1` — if a test is stuck on, scale traffic is blocked.

**Instrumentation added (session `76d0d7`, do not remove until verified):**

- **Browser → NDJSON ingest:** `interface/src/examples/serial/SerialRest.tsx` (hypothesis **A**: REST payload shape / empty `last_line`) and `SerialWebSocket.tsx` (hypothesis **B**: WS `connected` vs payload). Ingest URL: `http://127.0.0.1:7717/ingest/0bfa9906-aaa9-4a5a-9ff1-54d549900d98` with header `X-Debug-Session-Id: 76d0d7`. Logs accumulate under **`.cursor/debug-76d0d7.log`** when the UI is opened from a browser on the **same machine** as the ingest server.
- **Firmware → USB Serial:** `SerialService::loop` heartbeat emits one NDJSON line per ~5 s (hypothesis **C**: suspended / `available()` / line buffer length). Capture with **115200** on the **UART USB** port of the DevKit.

**Hypotheses under test**

| Id | Area | Idea |
|----|------|------|
| A | REST / device | API returns 200 with empty `last_line` because UART never completed a line or never received bytes. |
| B | WebSocket / UI | Socket not `connected`, or payload path not updating UI (see `useWs` `origin_id` filtering in `interface/src/utils/useWs.ts`). |
| C | Firmware UART | `Serial1` suspended, not started, or `available()==0` continuously. |
| D | Line protocol | Scale sends data without `\n`/`\r`, so `last_line` never commits. |
| E | USB logging | Wrong COM port, wrong baud, or firmware hang before `Serial` output — explains **both** UI empty and **silent** monitor. |

**Next:** Rebuild + flash so UI includes ingest hooks; reproduce per `<reproduction_steps>` in the agent reply; analyze **`debug-76d0d7.log`** + any USB NDJSON lines.

### Post-repro analysis (session `76d0d7`, `.cursor/debug-76d0d7.log`)

| Hypothesis | Verdict | Evidence |
|------------|---------|----------|
| **A** (empty device state) | **CONFIRMED** | Repeated lines e.g. L2, L7, L9: `hasDataObject:true`, `lastLineLen:0`, `timestamp:0` — REST reaches device but **`last_line` never set**. |
| **B** (WS broken) | **REJECTED** | L4–L5: `connected:true`, `hasPayload:true` with `lastLineLen:0` — socket delivers payloads; payload is empty serial state, not a transport failure. |
| **C** (UART suspended / no bytes) | **INCONCLUSIVE** from NDJSON file alone | USB heartbeat NDJSON was not in ingest file (device-side). If after fixes **`available`** stays 0, re-check **TX→GPIO18 (RX)** / baud / Diagnostics suspend. |

**Fixes applied (2026-04-24, post-log):** (1) **`useWs.ts`** — apply **`message.payload`** when `origin_id` matches (removed logic that kept the **first** payload forever). (2) **`useRest.ts`** — stop clearing `data` before each poll (removed spurious `hasDataObject:false` flicker in logs). (3) **`SerialService.cpp`** — idle flush of `_lineBuffer` for frames **without `\r`/`\n`** (default **500 ms**); later tightened to **publish only if printable ASCII** exists (see §Display vs bytes). **Instrumentation retained** until verification complete.

### 2026-04-24 — Display vs bytes (post-verification logs + UI screenshots)

**Runtime (`.cursor/debug-76d0d7.log` ~L780+):** `hypothesisId:B` entries show **`connected:true`**, **`lastLineLen`** varying **1–42** with **`weightPresent:false`** — payload reaches the browser; empty regex matches Configuration screenshot.

**Root cause (display):** **`SerialRest.tsx`** wraps raw data with **`sanitizeLastLineForDisplay`** (control / C1 → U+00B7, often looks like a **dot**). **`SerialWebSocket.tsx`** rendered **`line.text` raw** — NUL / control-only payloads appear **blank after `[Boot + …]`** in Live Stream.

**Root cause (noise lines):** Idle flush could publish **`last_line`** consisting only of **control** bytes (e.g. framing noise or wrong baud), which REST shows as a run of **middots**.

**Follow-up fixes:** (1) **`SerialWebSocket.tsx`** — render **`sanitizeLastLineForDisplay(line.text)`** like REST. (2) **`SerialService.cpp`** — **`SERIAL_LINE_IDLE_FLUSH_MS`** default **500 ms**; **printable-only idle publish** was **reverted** after a reproduction log showed **`lastLineLen` 0** for the whole capture (risk: delayed ASCII / binary prelude discarded before newline).

**Repro log (session `76d0d7`, short capture):** REST **`lastLineLen:0`**, WS **`hasPayload:false`** after connects — treat as **no `last_line` on device** for that window (reflash build, scale off, or UART not delivering `\n`/idle flush window). Compare with earlier capture (~L780+) where **`lastLineLen` 1–42** proved ingress alive.

**Follow-up repro (same session):** Log lines **L4–L5, L40** show REST **`lastLineLen` 1 and 5** with non-zero **`timestamp`**, while WS rows stay **`hasPayload:false`** — **confirmed** `useWs` was rejecting payloads: server **`origin_id`** is **`websocket`** / **`serial_hw`** (`WebSocketTxRx.h`), not **`websocket:<clientNumericId>`**. **Fix:** `interface/src/utils/useWs.ts` applies payload when **`!clientId || origin_id !== clientId`** (ignore self-echo only). Debug ingest hooks removed from Serial UI / heartbeat NDJSON removed from `SerialService.cpp`.

## Related docs

- Primary evidence and attempt table: [DECISION-LOG-serial-scale-instance-1.md](../drafts/DECISION-LOG-serial-scale-instance-1.md) §9.3–9.6.
- STA vs outbound WS distinction: [DECISION-LOG-wifi-sta-connectivity.md](../drafts/DECISION-LOG-wifi-sta-connectivity.md) “Outbound WebSocket client”.
