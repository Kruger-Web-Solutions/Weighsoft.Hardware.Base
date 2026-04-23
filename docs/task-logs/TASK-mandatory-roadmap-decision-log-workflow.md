# TASK: Mandatory roadmap and decision log (“follow the rules”)

**Status:** DRAFT / IN PROGRESS (standing workflow; not a shipped feature)

**Last updated:** 2026-04-21 (forwarder WS reconnect storm fix + flash)

## Roadmap

### Active workstream (2026-04-21): ESP32-S3 USB flash + COM16

- **Goal:** Flash the firmware that includes **SerialWriter** + **SerialWriterForwarder** (same app as default `esp32s3` build; there is no separate PlatformIO project for serialwriter only) using **`env:esp32s3`** onto the ESP32-S3 attached as **COM16**.
- **Done:** `upload_port = COM16` under `[env:esp32s3]`; `python -m platformio run -e esp32s3 -t upload` after `Get-Process python* | Stop-Process -Force` (per `platformio-upload.mdc`). PlatformIO reported **SUCCESS** (~7m20s including npm interface build and first compile).
- **Done (2026-04-21, user unplug + agent re-flash):** User unplugged/replugged the ESP32-S3 to clear possible **Windows COM “Access is denied”** / stuck-handle situations. Agent ran **`python -m platformio run -e esp32s3 -t upload`** from repo root — **SUCCESS** in **~100s**; esptool used **COM16**, chip **ESP32-S3 (QFN56) rev v0.2**, **USB-Serial/JTAG**, **~2.21 MB** app image written, **Hash verified**, **Hard resetting via RTS**. This image includes the **forwarder `deliverLine` → `updateWithoutPropagation`** change (avoids WebSocket/MQTT broadcast storm on every weight line; see `TASK-display-serial-bridge-network.md` / decision log below). **Non-developer helper:** `scripts/Esp32s3-FlashAndMonitor.ps1` (5 s pause, upload, then `pio device monitor` — agent did **not** leave monitor running; user may run the script locally for logs).
- **Pending:** User sign-off that the device behaves as expected (web UI, SerialWriter, weight forwarder under load, no COM lock when opening monitor). **Same-day responsiveness work (shipped in firmware):** **throttle `serial_writer_sent` propagation** (WebSocketTx + MQTT, **≥55 ms**), **`updateWithoutPropagation`** on intermediate sends + forwarder **`deliverLine`** metadata, **`yield()`** in writer/forwarder loops, **`main.cpp`** forwarder-before-writer **×3** paired passes per frame. **Debug:** temporary **H3** NDJSON in **`writePendingLine`** **removed** per user go-ahead. **Agent `python -m platformio run -e esp32s3 -t upload`** **SUCCESS** (~118 s) to **COM16** after cleanup (hash verified, RTS reset).
- **Done (2026-04-21, forwarder → reader WS flapping):** Runtime log showed **`WS disconnected` / `Reconnecting WS...` / `WS connecting`** in tight succession during weight traffic. **Cause:** **`checkWsConnection()`** called **`initWsClient()`** whenever **`!isConnected()`**, deleting and recreating **`WebSocketsClient`** while the library already reconnects via **`loop()`** + **`setReconnectInterval(5000)`** — **double reconnect owner**. **Fix:** **`checkWsConnection`** only calls **`initWsClient()`** when **`_wsClient == nullptr`** (first create or after **`onConfigUpdated`** teardown). **Agent `pio run -e esp32s3 -t upload`** **SUCCESS** to **COM16** after fix.

### Active workstream (2026-04-21): Agent verification — `pio run`, `npm test`, `npm run build`

- **Goal:** Run default firmware build (`esp32s3`), Jest (`npm test` in `interface/`), and standalone `npm run build`; record outcomes in this task log; **do not change product code** for findings unless a failure blocks a later step (none did).
- **Done:** **`npm test`** (**6** tests). **`npm run build`** with **`CI=true`** and with **`CI` unset** — **Compiled successfully** (eslint issues cleared). **`npx update-browserslist-db@latest`** in **`interface/`** (lockfile + **`caniuse-lite`** refreshed). **`python -m platformio run -e esp32s3`** **SUCCESS** (interface step **Compiled successfully**; flash **~84.2%**). **`npm audit`** still **out of scope** per user.
- **Pending:** Optional **`parse-ms`** / Jest story for **`time.ts`** tests; flash headroom remains an **advisory** metric only.

- **Goal:** Whenever the user says **follow the rules**, maintain a full roadmap and decision log for the **active engineering workstream** until explicit sign-off.
- **Scope:** Task logs under `docs/task-logs/` only; no promotion to official or user-facing docs before sign-off and completed testing (unless the user explicitly requests otherwise).
- **Ongoing:** Apply this workflow on every future task where the trigger phrase is used.

## Decision log

### 2026-04-21 — Forwarder WebSocket to serial reader: random disconnect / reconnect storm

| Field | Detail |
|--------|--------|
| **What was tried** | **Evidence:** user COM16 log (terminal) — **`[SerialWriterForwarder] WS disconnected`** then **`Reconnecting WS...`** / **`WS connecting`** repeating; sometimes **`Reconnecting`** twice without a clean connect; weight lines **`[SerialWriter] Sent:`** continued between drops. **Hypothesis:** application **`checkWsConnection()`** recreated the client on **every** transient **`!isConnected()`**, conflicting with **`WebSocketsClient::loop()`** internal reconnect (**`setReconnectInterval(5000)`** already set in **`initWsClient()`**). **Fix:** **`SerialWriterForwarderService::checkWsConnection`**: only **`initWsClient()`** when **`_wsClient == nullptr`**; if client exists, rely on **`_wsClient->loop()`** for reconnect. Log string renamed to **`WS client absent; creating connection...`**. |
| **Why** | User report: forwarder drops **WebSocket** to serial reader **randomly**; follow project decision-log rules. |
| **Outcome** | Code change applied; **`python -m platformio run -e esp32s3 -t upload`** **SUCCESS** to **COM16** (agent). |
| **Failure / rejection** | If a **stuck** half-open socket ever needs a forced recreate without config change, a future **long-backoff** **`initWsClient()`** could be added; not implemented here. |
| **What was learned** | **Single owner** for reconnect: either the **library** (interval + **`loop()`**) or **app-level full teardown**, not both on the same condition. **`WiFi` / captive portal** messages in the same capture are consistent with brief STA events; avoiding client delete reduces secondary flaps. |

### 2026-04-21 — Serial string sluggish + random freezes (propagate throttle + forwarder metadata)

| Field | Detail |
|--------|--------|
| **What was tried** | **Hypotheses:** (H1) **`serial_writer_sent`** still ran **WebSocketTx::transmitData** + **MqttPubSub::publish** on **every** UART line (large JSON + `textAll`); (H2) **`Serial.printf("[SerialWriter] Sent:…")`** on USB CDC every line blocked the main loop; (H3) forwarder **`deliverLine`** still used propagated **`update(..., "internal")`** for counters → second **WS broadcast storm**. **Fix:** **`_lastSentPropagateMs`** + **`kSentPropagateMinMs = 55`**: full **`update(..., "serial_writer_sent")`** only when interval elapsed, else **`updateWithoutPropagation`**; USB log line only on propagated sends; **`yield()`** after writer/forwarder loop work; forwarder metadata → **`updateWithoutPropagation`**. **Instrumentation (removed 2026-04-21):** temporary **H3** NDJSON in **`writePendingLine`** (every **4 s**, `prop`/`skip`/`heap`) — removed after user authorized cleanup; product behavior unchanged except no debug lines on USB. |
| **Why** | User report: display string **unresponsive** and **freezes at random intervals** under continuous weight updates. |
| **Outcome** | **`python -m platformio run -e esp32s3`** **SUCCESS** (~114 s) after edits; flash **~84.4%**, RAM **~21.6%**. Post-cleanup: **`pio run -e esp32s3 -t upload`** **SUCCESS** (~118 s) to **COM16**. |
| **Failure / rejection** | None at compile/upload. |
| **What was learned** | **`updateWithoutPropagation`** on pending alone is insufficient if **`serial_writer_sent`** still floods subscribers; **time-based throttle** caps WS/MQTT rate while keeping **`_state`** accurate every line. Forwarder **internal** state must not use propagated **`update`** on the hot path. **`yield()`** reduces risk of watchdog / task starvation when networking stacks are busy. |

### 2026-04-21 — Follow-up: string still sluggish after throttle (main `loop()` ordering)

| Field | Detail |
|--------|--------|
| **What was tried** | **`main.cpp`**: `serialWriterForwarderService->loop()` **before** `serialWriterService->loop()`, wrapped in **up to 3** paired passes per frame so buffered WebSocket text can set `pending_line` and UART can flush in the **same** `loop()` turn (previously writer always ran **before** forwarder → **one-frame latency** every line; bursts stacked work). |
| **Why** | User reported issue still reproduced (sluggish / freezes); runtime logs not attached — architectural hypothesis that **call order** + **single forwarder `loop()` per frame** delayed UART and increased backlog. |
| **Outcome** | **`python -m platformio run -e esp32s3`** **SUCCESS** after change (agent compile). |
| **Failure / rejection** | None at build. |
| **What was learned** | Stateful ingest + UART drain order matters as much as propagate throttling; bounded multi-pass avoids starving other work while draining short WS bursts. |

### 2026-04-21 — ESP32-S3 re-flash after unplug (COM16, forwarder anti-storm firmware)

| Field | Detail |
|--------|--------|
| **What was tried** | User unplugged/replugged the board to release stuck COM handles. From repo root: **`python -m platformio run -e esp32s3 -t upload`** (same as `scripts/Esp32s3-FlashAndMonitor.ps1` upload phase; agent session did not start an interactive **`device monitor`**). |
| **Why** | User asked the agent to perform the listed steps and record outcomes; unplug addresses prior **`PermissionError(13, 'Access is denied.')`** on **COM16** when another process held the port. |
| **Outcome** | **SUCCESS** (~100 s). esptool: **COM16**, **ESP32-S3 (QFN56) rev v0.2**, **Embedded PSRAM 8MB**, **USB mode: USB-Serial/JTAG**, flash write **~2.21 MB** @ **0x00010000**, **Hash of data verified**, **Hard resetting via RTS pin**. RAM **~21.6%**, Flash **~84.4%** (reported at upload time). |
| **Failure / rejection** | None for this upload. |
| **What was learned** | **Hardware unplug** is a valid first step when Windows denies COM access. **`updateWithoutPropagation`** for forwarder→`pendingLine` is in the flashed binary — reduces load from **`WebSocketTx`** / **`MqttPubSub`** on high-rate weight lines (see framework `WebSocketTxRx.h` handler registration). For day-to-day use, **`scripts/Esp32s3-FlashAndMonitor.ps1`** bundles the correct order (close other serial users → wait → upload → monitor). |

### 2026-04-21 — `esp32s3` upload port COM16 (SerialWriter firmware)

| Field | Detail |
|--------|--------|
| **What was tried** | Set `[env:esp32s3]` `upload_port = COM16` (was `COM11`) while keeping `upload_protocol = esptool` so this env overrides the global `[env]` OTA defaults (`espota` / IP). |
| **Why** | User requested flashing the ESP32-S3 used for this firmware—including SerialWriter—on **COM16**. |
| **Outcome** | `platformio.ini` updated; upload completed successfully to **COM16**. esptool: chip **ESP32-S3 (QFN56) rev v0.2**, **Embedded PSRAM 8MB (AP_3v3)**, USB mode **USB-Serial/JTAG**, flash write verified, hard reset via RTS. Build compiled `SerialWriterService.cpp` / `SerialWriterForwarderService.cpp` as part of the firmware. |
| **Failure / rejection** | N/A for upload step. |
| **What was learned** | SerialWriter ships in the main firmware (`main.cpp`); no separate env name is required beyond `esp32s3`. **Notable build noise (non-blocking):** npm reported many audit vulnerabilities; CRA/eslint **warnings** (e.g. trailing spaces); repeated **`-DARDUINO_USB_MODE` / `CONFIG_ESP_TASK_WDT_TIMEOUT_S` redefined** compiler notes from overlapping defines. |

### 2026-04-21 — Align repository workflow with user mandate

| Field | Detail |
|--------|--------|
| **What was tried** | Read `.cursor/rules/task-roadmap-decision-log.mdc` and `docs/task-logs/README.md` for parity with the user’s stated requirements. |
| **Why** | Confirm the codebase already documents the same constraints so behavior is consistent and traceable. |
| **Outcome** | The rule file and README already require: task file per workstream, roadmap + decision log during the task, every meaningful attempt (including failed/reverted), proposed solution with tradeoffs/risks/evidence, DRAFT until sign-off, no “final” framing before sign-off, and no official/user-facing doc updates until sign-off (unless explicitly requested). |
| **Failure / rejection** | N/A — no conflict found. |
| **What was learned** | No duplicate policy doc is required beyond this activation log; implementation is “follow existing rule + README on trigger.” |

### 2026-04-21 — Agent verification: `pio run -e esp32s3`

| Field | Detail |
|--------|--------|
| **What was tried** | From repo root: `python -m platformio run -e esp32s3` (Windows PowerShell). |
| **Why** | User-requested automated verification of default firmware build path (includes pre-script `npm install` + `npm run build` for interface). |
| **Outcome** | **SUCCESS** in ~72.6s. RAM ~21.6%, Flash ~84.2% of app partition. |
| **Failure / rejection** | None for compile/link. |
| **What was learned** | Same non-blocking frontend noise as prior sessions: **npm audit** reported **76** vulnerabilities (14 low, 20 moderate, 37 high, 5 critical); CRA **“Compiled with warnings”** (eslint + browserslist); no compiler “macro redefined” lines captured in this run’s stdout (may still occur in verbose builds). |

### 2026-04-21 — Agent verification: `npm test` (`interface/`)

| Field | Detail |
|--------|--------|
| **What was tried** | `cd interface; $env:CI='true'; npm test -- --watchAll=false` |
| **Why** | Exercise Jest per `package.json` `test` script. |
| **Outcome** | **FAILED** — exit code **1**. Jest: “No tests found”; 151 files checked, **0** matches for `testMatch` / `testRegex`. |
| **Failure / rejection** | No `__tests__` or `*.spec.*` / `*.test.*` under `interface/src` (by design or omission). |
| **What was learned** | `npm test` is not a green CI gate today unless tests are added or `--passWithNoTests` is adopted intentionally. **No code changes made** per user instruction. |

### 2026-04-21 — User policy: Jest must run real tests; ignore `npm audit`

| Field | Detail |
|--------|--------|
| **What was tried** | User answered follow-up: (1) **`npm test` must be tested** — not `passWithNoTests`-only; (2) **`npm audit`** — ignore. |
| **Why** | Close the verification workstream with explicit product/CI expectations. |
| **Outcome** | Added **`interface/src/utils/__tests__/endpoints.test.ts`** — unit tests for **`extractErrorMessage`** (server message, axios message, default fallback). **`npm audit`** remains explicitly untracked in this task log beyond noting it exists. |
| **Failure / rejection** | N/A. |
| **What was learned** | Pure util tests avoid full-app fetch graphs while still satisfying a strict “tests must exist” gate. |

### 2026-04-21 — Agent verification: `npm run build` (`interface/`)

| Field | Detail |
|--------|--------|
| **What was tried** | `cd interface; npm run build` (standalone, after `pio run` already exercised the same script). |
| **Why** | Confirm production build without PlatformIO wrapper. |
| **Outcome** | **SUCCESS** (exit 0); same **eslint warnings** and **Browserslist: caniuse-lite is outdated** notice as during `pio run`. |
| **Failure / rejection** | None for build completion. |
| **What was learned** | Warning list is stable across runs: trailing spaces in `Layout.tsx`, `LedControlBle.tsx`, `LedControlWebSocket.tsx`, `LedExampleInfo.tsx`; **max-len** on `LedExampleInfo.tsx` L67 and `WiFiStatusForm.tsx` L157. |

### 2026-04-21 — Agent verification (continuation): `routeMatches` tests + `CI` / CRA pitfall

| Field | Detail |
|--------|--------|
| **What was tried** | Added **`interface/src/utils/__tests__/route.test.ts`** (3 cases for **`routeMatches`**). Prototyped **`time.test.ts`** against **`formatDuration`** / **`formatLocalDateTime`** — **deleted** because **`parse-ms`** is published as **ESM** and Jest (current CRA setup) throws **Unexpected token 'export'** when importing `time.ts`. Ran **`CI=true npm test`** then **`npm run build`** in the **same** PowerShell session: CRA **failed** (“Treating warnings as errors because `process.env.CI = true`”). Re-ran **`npm run build`** and **`python -m platformio run -e esp32s3`** with **`CI` removed**: both **SUCCESS** (~108s firmware build; flash **~84.2%**). |
| **Why** | User asked to continue automated testing; expand Jest coverage without fragile full-app mounts. |
| **Outcome** | **2** test files, **6** passing tests. Firmware + interface pipeline green when **`CI` is not set** during `npm run build`. |
| **Failure / rejection** | **`time.ts`**-based tests rejected for now (dependency on **`parse-ms`** ESM). |
| **What was learned** | Do not chain **`npm test`** with **`CI=true`** and **`npm run build` / `pio run`** in one shell without **`Remove-Item Env:\CI`** (or new session); CRA promotes eslint warnings to **errors** under **`CI`**. |

### 2026-04-21 — Debug checklist remediation (eslint, browserslist, CI parity)

| Field | Detail |
|--------|--------|
| **What was tried** | **Hypotheses:** (H1) CRA “Compiled with warnings” under normal build but **`CI=true`** promotes those warnings to **hard failures**; (H2) failures were **`no-trailing-spaces`** and **`max-len`** on specific lines; (H3) Browserslist noise was outdated **`caniuse-lite`**; (H4) fixing (H2) makes **`CI=true`** builds green without changing CRA config. **Actions:** Removed whitespace-only blank lines / trailing space in **`sx={{`** (`Layout.tsx`), **`LedControlBle.tsx`**, **`LedControlWebSocket.tsx`**, **`LedExampleInfo.tsx`**; split **`LedExampleInfo`** long `secondary` string; added **`rssiQualityLabel()`** in **`WiFiStatusForm.tsx`** to shorten the RSSI line; ran **`npx update-browserslist-db@latest`** in **`interface/`**. **Runtime evidence:** temporary NDJSON append in **`config-overrides.js`** (webpack hook) wrote **`debug-52c919.log`** line showing `env: "production"`, **`CI: "true"`** during a green compile; instrumentation **removed** after verification. Re-ran **`$env:CI='true'; npm run build`** (no webpack log hook): **Compiled successfully**. **`CI=true npm test`**: **6** passed. **`pio run -e esp32s3`**: **SUCCESS**, interface step **Compiled successfully**. |
| **Why** | User requested checklist items fixed and recorded (except **`npm audit`** per prior instruction; flash item advisory only). |
| **Outcome** | **H1–H4 CONFIRMED** by before/after: pre-fix **`CI=true`** failed on eslint; post-fix **`CI=true`** and default **`npm run build`** both **Compiled successfully**; Browserslist update applied via lockfile refresh. |
| **Failure / rejection** | **`npm audit`** not addressed (user policy). **`parse-ms`/Jest** not in this checklist — still optional follow-up. |
| **What was learned** | **`CI=true`** is a valid gate once eslint is clean; PlatformIO **`npm run build`** benefits the same way if **`CI`** leaks into the shell. |

## Debug checklist (arranged status after 2026-04-21 remediation)

| # | Issue | Status | Notes |
|---|--------|--------|--------|
| 1 | **`npm test`** (was: exit 1, no tests) | **Resolved** | **`interface/src/utils/__tests__/endpoints.test.ts`**, **`route.test.ts`** — **6** tests; **`CI=true npm test`** passes. |
| 2 | **`npm audit`** (76 findings on `npm install`) | **Out of scope** | Per user: **ignore**; no lockfile audit remediation in this workstream. |
| 3 | **Browserslist / `caniuse-lite` outdated** | **Resolved** | **`npx update-browserslist-db@latest`** in **`interface/`**; **`package-lock.json`** updated. |
| 4 | **ESLint `no-trailing-spaces`** (Layout, LED BLE/WS/Info) | **Resolved** | Trailing whitespace removed; see decision log row above for file list. |
| 5 | **ESLint `max-len`** (`LedExampleInfo.tsx`, `WiFiStatusForm.tsx`) | **Resolved** | Split long **`secondary`** / introduced **`rssiQualityLabel`**. |
| 6 | **Flash headroom (~84.2%)** | **Advisory** | No partition change; re-measured after build — still **~84.2%**; track for future growth. |
| 7 | **`CI` + CRA** (warnings → errors under **`CI=true`**) | **Resolved** | Root cause was eslint violations; with them fixed, **`CI=true npm run build`** and **`pio run`** interface step **Compiled successfully**. Optional hygiene: unset **`CI`** between commands if other warnings reappear. |

**Instrumentation cleanup:** Temporary webpack NDJSON hook in **`interface/config-overrides.js`** and the repo-root **`debug-52c919.log`** artifact were **removed** after verification; no debug ingest hooks remain in the codebase.

## Current approach (not final until sign-off on a given task)

- **Why selected:** Matches user non-negotiables and existing project rules.
- **Tradeoffs:** Extra overhead on small tasks; benefit is full reconstructability and honest record of dead ends.
- **Risks:** Forgetting to update the task file mid-stream; mitigated by treating updates as part of the same cadence as code changes when the trigger is active.
- **Validation:** Policy alignment verified by file review. For the COM16 flash workstream: **`esp32s3` upload SUCCESS** on hardware (see decision log above, including **2026-04-21 — ESP32-S3 re-flash after unplug**). Functional tests on the board are still **pending user sign-off**. For the agent verification workstream: **`pio run -e esp32s3`**, **`npm run build`** (with or without **`CI=true`**), and **`npm test`** are green post-remediation; **`npm audit`** remains ignored; see **“Debug checklist (arranged status…)”** table and remediation decision log row.

## Sign-off

- [ ] User explicitly signs off on this standing workflow document (optional; workflow remains in effect per user instruction regardless).
