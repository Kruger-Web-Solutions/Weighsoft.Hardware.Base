# Task roadmap & decision log: Serial + scale ingress (Instance 1)

**Document type:** working task log (draft)  
**Status:** `IN PROGRESS` — implementation merged in working tree; **on-device stress / noise testing not signed off**  
**Official docs:** `docs/API-REFERENCE.md`, `docs/DATA-FLOWS.md`, etc. **must not be updated** until testing is complete and the product owner explicitly signs off.

**Non-final notice:** Nothing in this log constitutes a “final” release decision until sign-off. Treat the firmware behavior described here as **candidate** until validated on hardware under load.

**“Follow the rules” workflow:** Mandatory roadmap/decision-log maintenance is described in [TASK-DOCUMENTATION-POLICY.md](TASK-DOCUMENTATION-POLICY.md) and the drafts index [README.md](README.md).

---

## 1. Task scope (roadmap)


| Phase | Goal                                                                                                                                             | Status                                                                 |
| ----- | ------------------------------------------------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------- |
| A     | Bound UART processing per `loop()` so Wi‑Fi / WebSocket stacks are not starved under continuous scale traffic                                    | **Done (code)** — pending device validation                            |
| B     | Validate assembled lines before publishing to state / REST / WS / MQTT (empty, noise, length)                                                    | **Done (code)** — pending device validation                            |
| C     | Harden weight extraction after regex match (correct handling of zero / non-numeric captures)                                                     | **Done (code)** — pending device validation                            |
| D     | Preserve “no UI change” / original project format for this workstream                                                                            | **Satisfied for Serial changes** (firmware + headers only)             |
| E     | Stress + noisy serial testing; confirm serial → network integrity                                                                                | **Not complete**                                                       |
| F     | Resolve ambiguous product note **“4 hours not 12”** (map to a concrete timer or close as N/A)                                                    | **Open**                                                               |
| G     | After sign-off: optionally promote outcomes into permanent `docs/` (API, data flows) — **blocked** until explicit approval                       | **Blocked**                                                            |
| H     | Weight forwarder: WebSocket client to **arbitrary LAN IP or hostname**; end-to-end with serial ingress (no UI changes — existing `ws_url` field) | **Not complete** — on-device LAN bench + log evidence pending operator |


---

## 2. Problem statement

- Scale data arrives on **Serial1**; the service publishes `lastLine`, `weight`, and `timestamp` to subscribers.
- Requirements: detect **corruption / truncation / malformed** input, **robust error handling**, **real-time** processing without starving the network stack, and **data integrity** from serial through to network layers.
- Constraint: **no UI changes**; keep existing project structure (no gratuitous new modules unless justified).

---

## 3. Decision log (attempts)

### Attempt 1 — Unbounded `while (SERIAL_PORT.available())` read loop

- **What:** Original pattern could read the UART for a long time if the scale flooded bytes, delaying other `loop()` work.
- **Why tried / rejected:** N/A as new attempt — identified as a **risk** under continuous input.
- **Outcome:** Superseded by bounded reads (Attempt 4).

---

### Attempt 2 — fragile zero / non-numeric handling in `extractWeight`

- **What (before change):** Used `toFloat()` combined with a check like “zero only valid if string contains `'0'`”, which mishandles true zero and some edge captures.
- **Why changed:** Fails requirement for correct parsing; false negatives on valid zero weights.
- **Outcome:** **Replaced** with: require at least one ASCII digit in the captured substring, then `toFloat()` and format to two decimals.
- **Why it works:** `0` / `0.00` contain digits; empty or alphabetic captures do not.
- **Tradeoff:** Captures like `.5` depend on `String::toFloat()` behavior (acceptable for typical scale numeric text).
- **Validation evidence (so far):** `pio run -e esp32s3` **SUCCESS** (compile/link only).

---

### Attempt 3 — Append non-printable bytes as escaped hex (`\xNN`) into `_lineBuffer`

- **What:** On invalid byte, append a literal `\xAB` style sequence to the accumulating line.
- **Why tried:** Makes noise visible in logs / downstream for debugging.
- **Outcome:** **Rejected and reverted.**
- **Why it failed:** Pollutes the scale payload; breaks regex / weight parsing; inflates buffer length; misrepresents the device’s actual frame as if it were printable text.
- **Learning:** For “validate scale string data,” **preserve semantic fidelity** — either drop bytes or reject the frame, do not inject synthetic text into the measurement channel.

---

### Attempt 4 — Bounded UART reads per `loop()` iteration

- **What:** `SERIAL_MAX_READ_BYTES_PER_LOOP` (default 512), overridable via build flag.
- **Why:** Guarantees progress through the main loop under flood conditions.
- **Outcome:** **Kept** (candidate).
- **Tradeoff:** A single `loop()` pass may not drain the entire RX FIFO; remaining bytes are read on subsequent passes — acceptable if loop rate stays high vs baud.
- **Risk:** If main loop is slow for unrelated reasons, UART FIFO could still overflow at very high line rates — mitigated by budget but not impossible.
- **Validation evidence (so far):** Build **SUCCESS**; **no FIFO overflow / loss measurement** on bench yet.

---

### Attempt 5 — Truncate over-long lines to `SERIAL_MAX_PUBLISHED_LINE_CHARS`

- **What:** If trimmed line exceeded cap, substring to max length, log truncation, still publish.
- **Why tried:** Avoid rejecting “large but mostly valid” frames.
- **Outcome:** **Rejected and reverted.**
- **Why it failed:** Publishes **truncated** data as if complete — violates integrity / truncation detection goals for downstream consumers.
- **Learning:** Prefer **explicit reject** (`too_long`) over silent truncation for measurement ingress.

---

### Attempt 6 — Reject over-long lines (`too_long`) after trim

- **What:** If `length > SERIAL_MAX_PUBLISHED_LINE_CHARS`, reject line, log `[Serial] RX rejected (too_long) …`, do not update state.
- **Why selected:** Aligns with “detect truncation / malformed” — oversize assembled line is treated as not trustworthy.
- **Tradeoff:** Legitimate industrial frames > cap require raising the cap via `-DSERIAL_MAX_PUBLISHED_LINE_CHARS=…` or protocol-specific framing later.
- **Validation evidence (so far):** Build **SUCCESS**; **no field test** with max-length frames.

---

### Attempt 7 — `prepareScaleLineForPublish` gate (empty, control-noise heuristic)

- **What:** Trim leading/trailing spaces and CR/LF; reject empty; for `length >= 20`, reject if control-character ratio exceeds threshold (`ctrl * 5 > length`).
- **Why:** Defense in depth if non-text ever appears inside a line (e.g. future code paths, partial binary protocols).
- **Interaction:** UART path also **drops** non-printable/tabs-except bytes during assembly, so this gate mainly catches remaining anomalies or pasted/test injections via future APIs.
- **Outcome:** **Kept** (candidate).
- **Risk:** Heuristic could reject unusual but valid ASCII if threshold wrong — none observed; tunable by editing the function if a scale proves otherwise.

---

### Attempt 8 — Drop non-printable bytes during line assembly (not `\r`/`\n` terminators)

- **What:** Only append bytes that pass `isAsciiPrintableOrCommonWhitespace` (printable ASCII + tab).
- **Why selected:** Keeps `_lineBuffer` as text-safe for regex without injecting escapes.
- **Tradeoff:** Binary-framed protocols using STX/ETX on the same UART would lose framing bytes — acceptable for stated **ASCII scale string** focus; document if a device needs binary mode later.
- **Validation evidence (so far):** Build **SUCCESS**; **no noisy-input bench run** logged yet.

---

### Attempt 9 — Phase E stress / noisy serial (plan execution; on-device deferred)

- **What:** Sustained high-rate lines, mixed binary + text, watchdog and serial → network integrity under load (roadmap Phase E).
- **Why:** Plan requires evidence before treating Instance 1 serial path as validated beyond compile-only.
- **Outcome (this session):** **Deferred to operator** — no live ESP in agent environment; procedure remains §6 checklist + existing Attempts 4/6/7/8 rationale.
- **Validation evidence (this session):** `pio run -e esp32s3` **SUCCESS** (compile/link only); **no** flood or noise bench run executed here.
- **Learning:** Agent session can record build smoke and checklists; **field stress** must be appended when hardware run completes.

---

## 4. Related / parallel work (context only — not all signed off)

Captured so another developer understands nearby changes referenced in chat; **verify git history** for exact ownership and scope.


| Area                       | Notes                                                                                                                                                                 |
| -------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Wi‑Fi / soft‑AP timing     | Work to reduce AUTH / AP flapping during STA association (association guard, `stopAP` behavior).                                                                      |
| Weight forwarder WebSocket | Header formatting / Bearer JWT path; reconnect guards.                                                                                                                |
| Display sink `/ws/display` | New sink path for display ESPs.                                                                                                                                       |
| UI                         | Per product request, **Serial** UI was kept at original format; any mismatch between UI copy and firmware auth style is a known consistency risk until docs/sign-off. |


These items are **not** claimed as fully validated in this document unless separately tested and approved.

---

## 5. Open items (explicit)

1. **“4 hours not 12”** — no confirmed mapping to a repository constant; needs product clarification (JWT TTL, watchdog, AP guard, NTP, etc.).
2. **Device validation:** continuous flood, noisy bytes, and WS/REST integrity checks under load.
3. **Promotion to official `docs/`** — **forbidden** until sign-off per project rules above.
4. **Weight forwarder LAN validation (Phase H):** confirm DNS/TCP diag, WebSocket handshake, JSON payload on peer (e.g. Node-RED), serial-driven forwards, `connected` / `last_error` sane; record results in §9.5.

---

## 6. Sign-off checklist (for product owner)

- Bench stress: sustained high-rate lines, no watchdog resets, acceptable latency to WS subscribers  
- Noise: mixed binary + text — logs show rejects/drops, no silent corruption of `lastLine` / `weight`  
- Integrity: oversize line rejected, not truncated  
- Zero-weight case confirmed on real scale or recorded fixture  
- Confirm `SERIAL_MAX_PUBLISHED_LINE_CHARS` / read budget defaults for production scales  
- **Phase H — Weight forwarder (LAN WebSocket):** `[WeightForwarder] WS diag` shows DNS OK (or use raw IPv4 in `ws_url`) and TCP connect succeeds to target host:port  
- **Phase H:** Serial shows `WebSocket connected`; peer receives JSON (`last_line`, `weight`, `timestamp` unless display mode) matching scale updates (within `MIN_FORWARD_INTERVAL` rate cap)  
- **Phase H:** No unintended UI changes — configuration uses existing Weight Stream Forwarder screen only  
- Explicit written sign-off to treat this task as **FINAL** and to update official documentation if desired

**Sign-off name / date:** *(pending)*  

**Note:** Task status remains **IN PROGRESS** until the above boxes are checked and sign-off is recorded. Official `docs/*.md` promotion stays **blocked** per §1 phase G.

---

## 7. File touch list (this workstream)

- `src/examples/serial/SerialService.cpp` — validation, read budget, assembly filter, `extractWeight`  
- `src/examples/serial/SerialService.h` — tunable `SERIAL_MAX_`* macros  
- `src/examples/weightforwarder/WeightForwarderService.cpp` — WS client forward path, `parseWsUrl`, diagnostics (Phase H validation target); §9.5 **#17** adds `AGENTDBG` `wf_loop_wall` (`hypothesisId=M`) for long `loop()` wall time
- `platformio.ini` — shared `[env]` **`-D WEBSOCKETS_TCP_TIMEOUT=1500`** (links2004/WebSockets; avoids ~5 s `WebSocketsClient::loop()` stalls on failed TCP connect per §9.3 / §9.5 **#21**)
- `src/examples/weightforwarder/WeightForwarderService.cpp` — §9.5 **#22**: skip **`_wsClient->loop()`** during service reconnect backoff so **`/ws/serial`** Live Stream is not starved ( **`AGENTDBG` `hypothesisId=Q`**, session **`cd341b`**)

---

## 8. Changelog for *this* decision document


| Date (UTC) | Author | Change                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| ---------- | ------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 2026-04-20 | Agent  | Initial draft created per “follow the rules” documentation workflow; records attempts including hex-escape and truncation approaches later rejected.                                                                                                                                                                                                                                                                                                                                           |
| 2026-04-20 | Agent  | WiFi: verified `factory_settings.ini` already matches STA SSID **Weighsoft** / password **Monday@011**; corrected README AP bullets (were “Weighsoft-HW”; AP is `ESP8266-React-#{unique_id}` per ini). Flash: `pio run -e esp32wroom32d -t upload` **SUCCESS** (COM13, ESP32-D0WD).                                                                                                                                                                                                            |
| 2026-04-20 | Agent  | **“Follow the rules”** invocation: added [TASK-DOCUMENTATION-POLICY.md](TASK-DOCUMENTATION-POLICY.md) and [README.md](README.md) under `docs/drafts/`; cross-linked from this log. No official `docs/` promotion (task still **IN PROGRESS** / pending sign-off). **Note:** README WiFi section was edited earlier in the same calendar day as a **factual alignment** with `factory_settings.ini` during a user-requested credential check—not claimed as formal task sign-off documentation. |
| 2026-04-21 | Agent  | Added §9 runbook: Weight Forwarder → Node-RED WebSocket at `ws://software:3000/weighsoft-weight` (payload shape, DNS/port pitfalls, diagnostics). §9.6 attempt log started; **no firmware changes** in this edit—operational guidance only.                                                                                                                                                                                                                                                    |
| 2026-04-21 | Agent  | **Plan: Serial + LAN WS forwarding:** Added roadmap **Phase H**, open item **#4**, sign-off checklist rows for LAN WS + no UI; §9.5 rows for plan execution + compile smoke; §3 **Attempt 9** (Phase E deferred); §9.6 build evidence. **No** edits to `interface/` or official `docs/`.                                                                                                                                                                                                       |
| 2026-04-21 | Agent  | §9.3 symptom row + §9.5 **#5**: hostname `software` resolved to wrong IP (`192.168.10.117` vs Node-RED `10.175.183.160`); recommend literal `ws_url` or DNS fix; noted UI dual meaning of “WebSocket”.                                                                                                                                                                                                                                                                                         |
| 2026-04-21 | Agent  | §9.5 **#6**: throttled Serial diagnostics in `SerialService.cpp` + `WeightForwarderService.cpp` for forward-path root-cause; `pio run -e esp32s3` **SUCCESS**.                                                                                                                                                                                                                                                                                                                                 |
| 2026-04-21 | Agent  | §9.3 symptom + §9.5 **#7**: removed broken `WebSocketsClient::setReconnectInterval(~ULONG_MAX)` (blocked first `tcp->connect` per library `loop()`); §8 changelog.                                                                                                                                                                                                                                                                                                                             |
| 2026-04-22 | Agent  | §9.3 symptom row (errno 11 / `WiFiClient::write` vs STA), §9.5 **#8**: `AGENTDBG` NDJSON on Serial for session `36885a`; build `pio run -e esp32wroom32d` **SUCCESS**; `upload_port` **COM10** for `[env:esp32wroom32d]`; full **upload** to COM10 **SUCCESS** (RTS reset). |
| 2026-04-22 | Agent  | §9.3 row (`msSinceWfLoop` thousands), §9.4 path hint for `debug-36885a.log` at repo root, §9.5 **#8** outcomes filled from pasted COM10 logs, **#9** fix (`main.cpp` loop order + `forwardViaWs` pre-`sendTXT` `_wsClient->loop()`). |
| 2026-04-22 | Agent  | Post-fix COM10 run still reproduced (`sendTXT_false`, errno 11, `msSinceWfLoop~8479`) and showed local `[AsyncWebSocket] Too many messages queued`; added §9.5 **#10** instrumentation: `main.cpp` loop-segment telemetry (`hypothesisId=F`) and `WebSocketTxRx.h` server WS pressure logs (`hypothesisId=G`); built + uploaded COM10 successfully. |
| 2026-04-22 | Agent  | Runtime proved `SerialService::loop()` starvation (`AGENTDBG` `main_loop_timing`: `serialMs=18680` ms). Root cause identified as unsigned budget underflow in `readBudget-- > 0`; fixed to pre-check/decrement form; built + uploaded COM10 (§9.5 **#11**). |
| 2026-04-22 | Agent  | New runtime after #11 still showed `serialMs=4694` and renewed errno 11 bursts; added hard serial RX time budget (`SERIAL_MAX_READ_MS_PER_LOOP=25`) with `hypothesisId=H` budget-hit telemetry (§9.5 **#12**). |
| 2026-04-22 | Agent  | New runtime after #12 still showed AsyncWebSocket queue overflow + panic (`abort()` from `operator new` path in AsyncTCP accept). Added Serial publish-rate cap (`SERIAL_MIN_PUBLISH_INTERVAL_MS=100`) with `hypothesisId=I` throttle telemetry (§9.5 **#13**); later flash+test still reproduced errno 11 / `sendTXT_false` with `msSinceWfLoop~9016` (`terminals/31.txt`). |
| 2026-04-22 | Agent  | Added WS-edge throttle for `serial_hw` fanout in `WebSocketTxRx::transmitData()` (`hypothesisId=J`, 200 ms minimum between server broadcasts) as §9.5 **#14**; build + upload to COM10 succeeded, verification pending. |
| 2026-04-22 | Agent  | §9.5 **#17:** post-#16 user Serial paste recorded (`forwarderMs` multi-second, `serialMs` ~80, errno 11, `sendTXT_false`, no `hypothesisId=L` in snippet); **`AGENTDBG` `wf_loop_wall` (`hypothesisId=M`)** added in `WeightForwarderService.cpp` (§9.6 build row). |
| 2026-04-22 | Agent  | §9.5 **#20** + §8: reverted off a non-logged `serial_hw`–only max-alloc fork; restored **#19** single-floor `WebSocketTxRx.h` behavior; `platformio.ini` **`default_envs = esp32s3`**; trimmed `wf_heartbeat` JSON to pre-experiment shape. |
| 2026-04-23 | Agent  | **Serial Reader debug chat plan:** §9.3 symptom row (**`ws_client_loop_slow` ~5005 ms** / **`WEBSOCKETS_TCP_TIMEOUT`**), §9.5 **#21**, new **§9.7** operator checklists (STA / LittleFS vs factory, **trace-one-line**, **Phase E**). `pio run -e esp32wroom32d` **SUCCESS** (compile smoke). §9.3/#21 cite **`ensureWebSocketClientAllocated()`** for **`setReconnectInterval(3000)`**. |
| 2026-04-23 | Agent  | **§9.5 #22 / §9.3 Live Stream row:** skip **`_wsClient->loop()`** during service backoff (session **`cd341b`** `hypothesisId=Q`); evidence COM10 **`wsLoopMs`~1505** + empty Live Stream; verification **pending** operator flash. |


---

## 9. Runbook: Weight Forwarder → Node-RED WebSocket (`ws://software:3000/weighsoft-weight`)

**Goal:** ESP32 (same firmware) connects as a **WebSocket client** to the laptop’s Node-RED listener and sends **JSON** each time the serial path publishes a new weight/line (subject to forwarder rate limits).

**Important vocabulary:** The scale’s **plain text line** becomes fields inside JSON (`last_line`, `weight`). The device does **not** send the raw serial string alone unless you build that on the Node-RED side from `msg.payload`.

### 9.1 What the firmware actually sends (default “display mode” off)

After a successful upgrade, each forward is one **text** WebSocket message whose body is JSON, for example:

```json
{"last_line":"…full or trimmed scale line…","weight":"12.34","timestamp":12345}
```

- `timestamp` is `millis()` on the device (milliseconds since boot), **not** wall clock.
- If **Display mode** is enabled in the Weight Forwarder UI, the payload becomes `{"line1":"…","line2":"…"}` (16-character padded/truncated fields) instead.

Reference: `WeightForwarderService::formatJson` / `forwardViaWs` in `src/examples/weightforwarder/WeightForwarderService.cpp`.

### 9.2 Noob-friendly checklist (end-to-end)

1. **Put the laptop and the ESP32 on the same LAN** (same Wi‑Fi SSID as the ESP’s STA, or routed path the ESP can reach—same subnet is simplest).
2. **On the laptop, confirm Node-RED is really listening on TCP 3000** for a **WebSocket** (not plain HTTP). The path in the URL must match what Node-RED expects (here: `/weighsoft-weight`).
3. **Windows firewall:** if the ESP never connects, allow **inbound** TCP **3000** for Node-RED (or `node.exe`) on the laptop.
4. **Hostname `software`:** the ESP resolves this with **DNS** (what your router hands out). It is **not** automatically the Windows PC name unless your network registers that name. If unsure, ping `software` from another machine on the LAN, or use the laptop’s **IPv4 address** in the URL instead, e.g. `ws://192.168.1.42:3000/weighsoft-weight`.
5. **Port in the URL is mandatory for port 3000:** If you omit `:3000`, the firmware defaults WebSocket port to **80** for `ws://` URLs—Node-RED would not be hit. Always include `:3000` for this setup.
6. **On the device web UI** (browser → ESP’s IP): open **Weight Stream Forwarder** (or equivalent menu entry).
  - Check **Enable Weight Forwarding**.
  - **Protocol:** WebSocket Client.
  - **WebSocket URL:** `ws://software:3000/weighsoft-weight` (or `ws://<laptop-ip>:3000/weighsoft-weight`).
  - Leave **Target login** empty unless your Node-RED path requires JWT/Basic auth compatible with this firmware (see UI helper text).
  - **Save** configuration.
7. **Serial must be producing lines** the service accepts as valid scale text; forwarding runs off the same update path as the serial weight pipeline. If nothing ever updates `weight` / `last_line`, the forwarder has nothing new to send (within rate limits).
8. **Serial Monitor** (same baud as firmware, correct COM port): watch for lines starting with `[WeightForwarder]` — connect attempts, `WS event=…`, `WS diag: host=… dnsResult=…`, and `WebSocket connected`. Every ~15s while disconnected, **WS diag** logs DNS + a short TCP probe to the host/port.

### 9.3 Interpreting common symptoms (before log paste)


| Symptom                                                                                                   | Likely cause                                                                                                                                                                                                                                                                                                                                        |
| --------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `dnsResult` not `1` or resolved `0.0.0.0`                                                                 | Hostname `software` not known on this LAN — use laptop IP or fix router/DNS.                                                                                                                                                                                                                                                                        |
| TCP connect fails in diag                                                                                 | Firewall, wrong port, Node-RED not bound to `0.0.0.0`, or ESP not on same network.                                                                                                                                                                                                                                                                  |
| `WS fatal: HTTP 200 (not WebSocket)`                                                                      | URL hits an **HTTP** server that returns a normal page (no WebSocket upgrade). Fix Node-RED to expose a **websocket-in** (or correct path).                                                                                                                                                                                                         |
| `WS not connected` when weight updates                                                                    | Handshake not complete yet, backoff after errors, or fatal suppress window (5 min after repeated bad handshake)—see Serial logs.                                                                                                                                                                                                                    |
| `dnsResult=1` but `tcpConnect=0` and ESP `localIP` is same /24 as **another** machine you care about      | **Hostname resolved to the wrong host** (e.g. `software` → `192.168.10.117` while Node-RED is `10.175.183.160`). Use `**ws://10.175.183.160:3000/…`** in `ws_url` or fix DNS/hosts so the name points at the Node-RED box.                                                                                                                          |
| `WS probe` shows `101 Switching Protocols` but **no** `WS event=CONNECTED` / endless `ws_handshake_stall` | **links2004/WebSockets** `WebSocketsClient::loop()` gates the first `tcp->connect()` on `(millis()-_lastConnectionFail) < _reconnectInterval`. Setting `_reconnectInterval` to `~ULONG_MAX` with `_lastConnectionFail==0` after `begin()` **blocks the initial connect indefinitely** — remove that pattern; rely on service-level backoff instead. |
| Repeated `[E][WiFiClient.cpp:429] write(): fail ... errno: 11` then `ws_tx_FAIL` / `sendTXT returned false` | On ESP32 Arduino, **errno 11 is usually `EAGAIN`** (would-block / TX path not ready) even if the core prints `"No more processes"`. Often **outbound TCP/WebSocket TX** (peer slow, buffer full, half-open socket), not necessarily STA Wi‑Fi auth loss — confirm with `WiFi.isConnected()` + RSSI in the same timeframe. See §9.5 #8 (`AGENTDBG` NDJSON on Serial). |
| `AGENTDBG` **`msSinceWfLoop` in the thousands** right before `sendTXT_false` while earlier `pre_sendTXT` lines show small values | Strong signal that **`WeightForwarderService::loop()` (and thus `WebSocketsClient::loop()`) was not entered for a long wall‑clock interval** before `sendTXT` ran — TCP TX not drained, errno 11 accumulates. **Mitigation:** run weight forwarder loop **before** `SerialService::loop()` in `main.cpp`, and call `_wsClient->loop()` immediately before `sendTXT` (§9.5 **#9**). |
| `[AsyncWebSocket.cpp:427] _queueMessage(): Too many messages queued: closing connection` appears near outbound failures | This is the **ESP local AsyncWebServer WebSocket** queue (browser/UI client side), not the outbound Node-RED WebSocketsClient. Indicates server-side WS backpressure / listener overload that can correlate with heap pressure and event-loop stalls; instrument server broadcast path (`WebSocketTxRx.h`) before changing behavior (§9.5 **#10**). |
| `AGENTDBG` `main_loop_timing` shows `serialMs` in seconds (e.g. 18680 ms) | `SerialService::loop()` consumed the whole loop tick; inspection found budget underflow bug in `while (SERIAL_PORT.available() && readBudget-- > 0)`: when budget reaches 0, post-decrement wraps to `UINT_MAX`, effectively unbounding reads under continuous input. Fix to `readBudget > 0` + explicit decrement (§9.5 **#11**). |
| `serialMs` remains multi-second (e.g. 4694 ms) after #11 while `msSinceWfLoop` stays low (3–109 ms) | Underflow was real but not the only stall source. Remaining long time is spent inside SerialService work during burst processing/fanout. Add explicit RX wall-clock cap in addition to byte cap (§9.5 **#12**). |
| `serialMs` in the **multi-second** range **after** #11–#14 while `pre_sendTXT` still shows low `msSinceWfLoop`, then later `sendTXT_false` with **`msSinceWfLoop` in the thousands** (e.g. 8403) on the same run | **Outbound forwarding still ran inside `SerialService::update()`’s handler chain:** `WeightForwarderService::onSerialWeightUpdate` called `forwardWeight` → `forwardViaWs` while `main.cpp` had not yet reached `weightForwarderService->loop()` for a long interval (serial phase + nested work). Fix: **defer** `forwardWeight` to `WeightForwarderService::loop()` after `_wsClient->loop()` (§9.5 **#15**). |
| After **#15**, `main_loop_timing` shows **`forwarderMs` in the thousands** while **`serialMs` stays tiny**, plus heap **below ~25k**, local **`_queueMessage(): Too many messages queued`**, and `sendTXT_false` with **`msSinceWfLoop` in the thousands** | Stall is inside **`WeightForwarderService::loop()`** (often long **`WebSocketsClient::loop()`** under errno **11**), compounded by **high-rate `internal` state** updates fanning to **`/ws/weightforwarder`**. Mitigation: throttle propagating **`internal`** success updates + widen **`serial_hw`** broadcast spacing + log slow **`_wsClient->loop()`** (§9.5 **#16**). |
| **`abort() was called` on core 1** after errno **11** bursts / **`listeners`:2** on `serial_hw` | **Addr2line** on firmware.elf: **`std::bad_alloc`** → `AsyncWebServerResponse::addHeader` (`WebResponses.cpp:181`, `_headers.emplace_back`) from **`AsyncTCP`** `_async_service_task` (`AsyncTCP.cpp:245`). ESPAsyncWebServer throws when **HTTP** response assembly cannot allocate; heavy **WS broadcasts + fragmentation** leave **too little contiguous heap**. Mitigation **#18**: skip WS **broadcasts** when **`ESP.getMaxAllocHeap()`** is below a safe floor; widen **`serial_hw`** spacing; relax **`internal`** UI period **750 ms** (§9.5 **#18**). |
| **`hypothesisId:P`** every ~4 s with **`maxAlloc` ~25588** and **`freeHeap` ~38k**, no **`abort`** / **`sendTXT_false`** in the same capture (`terminals/43.txt`) | **#18 default floor 28672** was **above** this board’s **steady-state** largest block (~25.5k on BLE partition / runtime), so **`serial_hw` broadcasts were skipped almost always** while outbound **`ws_tx_ok`** stayed healthy — **over-conservative**. **#19**: default **`WS_TXRX_MIN_MAX_ALLOC_FOR_BROADCAST` → 20480** so ~25.5k passes; still skips the measured pre-panic dip (**~9204**). |
| **`AGENTDBG` `hypothesisId=L`** `ws_client_loop_slow` with **`wsLoopMs` ~5005** repeating, **`main_loop_timing`** **`forwarderMs` ~5037**, `wifi:1`, then **`WS event=DISCONNECTED`** / **TCP connection cleanup** | **`WebSocketsClient::loop()`** blocks on **`WiFiClient::connect(..., WEBSOCKETS_TCP_TIMEOUT)`**. The links2004/WebSockets **default `WEBSOCKETS_TCP_TIMEOUT` is 5000 ms**, which can monopolize **`WeightForwarderService::loop()`** for ~5 s per failed outbound connect attempt. **Mitigation (in tree):** shared **`[env]`** **`-D WEBSOCKETS_TCP_TIMEOUT=1500`** in `platformio.ini`; **`_wsClient->setReconnectInterval(3000)`** in `WeightForwarderService::ensureWebSocketClientAllocated()` immediately after **`new WebSocketsClient()`** so the library does not retry failed TCP every **~500 ms** with a **5 s** block each time. **Peer-side:** correct Node-RED **websocket-in** path (not plain HTTP), firewall, and URL. See §9.5 **#21**. |
| **Serial Live Stream stuck “Waiting for data…”** while **`wsLoopMs` ~1505**, **`forwarderMs` ~1536**, **`serialMs`/`serialFlushMs` ~0**, `wifi:1`, **`WS … TCP connection cleanup`** | **`_wsClient->loop()`** still runs **before** `checkWsConnection()`; during **service-level reconnect backoff** (`_nextWsConnectAllowedAtMs`) the library can block for **`WEBSOCKETS_TCP_TIMEOUT`** (~1500 ms) **every** `loop()` tick even though **`begin()`** is not reissued yet — starving **`esp8266React->loop()`** / **`flushPublishPhase()`** / **`/ws/serial`** fanout. **Mitigation §9.5 #22:** skip **`_wsClient->loop()`** when disconnected, **not** `_wsHandshakeInFlight`, and **`millis() < _nextWsConnectAllowedAtMs`**; **`AGENTDBG` `hypothesisId=Q`** `ws_loop_skipped_service_backoff` (session `cd341b`) when skip fires (10 s throttle). |


### 9.4 What to paste if it still fails

From **Serial Monitor**, copy from first `[WeightForwarder]` after boot through several reconnect cycles, plus: ESP **WebSocket URL** as saved (redact passwords), laptop **IP**, and confirmation Node-RED listens on **3000** with path **/weighsoft-weight**.

**Optional NDJSON capture file:** create or overwrite **`debug-36885a.log`** in the **repository root** (same folder as `platformio.ini`: `…\Weighsoft.Hardware.Base\debug-36885a.log`). Paste lines that start with `AGENTDBG ` so each JSON object is on one line after the prefix.

### 9.5 Attempt history (Node-RED WebSocket integration)


| #   | Date (UTC) | What we tried                                                                               | Why / hypothesis                                                                                                                                                          | Outcome                                                                                                                                                                                                       | Learning                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| --- | ---------- | ------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1   | 2026-04-21 | Documented runbook (§9.1–9.4); no repo code edits                                           | User asked for noob-friendly steps + maintained decision log per task policy                                                                                              | **Pending user bench run**                                                                                                                                                                                    | Payload is **JSON** with `last_line` / `weight`; URL must include `:3000`; hostname must resolve from ESP.                                                                                                                                                                                                                                                                                                                                                       |
| 2   | 2026-04-21 | Plan “Serial + LAN WS”: roadmap Phase H, §5/#4, §6 WS items, §3 Attempt 9, §9.6 build smoke | **Follow the rules** — update log at start of build/validation track; **no UI**                                                                                           | **Doc + compile smoke complete**; LAN bench still operator-owned                                                                                                                                              | Same-network forwarding uses existing `ws_url` (IP or DNS); official `docs/` untouched.                                                                                                                                                                                                                                                                                                                                                                          |
| 3   | 2026-04-21 | `pio run -e esp32s3` (default PlatformIO env)                                               | CI-style evidence that serial + weight forwarder sources still compile                                                                                                    | **SUCCESS** (link only)                                                                                                                                                                                       | Does not replace on-device WS handshake or stress tests.                                                                                                                                                                                                                                                                                                                                                                                                         |
| 4   | 2026-04-21 | Firmware defect hunt for WS/LAN                                                             | Plan phase 3 — only if bench proves bug                                                                                                                                   | **N/A this session**                                                                                                                                                                                          | No failure logs provided; no code change until bench proves firmware fault.                                                                                                                                                                                                                                                                                                                                                                                      |
| 5   | 2026-04-21 | User bench: `ws://software:3000/weighsoft-weight` then corrected target                     | Logs: ESP `10.175.183.66`, DNS resolved `software` → `192.168.10.117`, `tcpConnect=0` errno 113; Node-RED actually at `**http://10.175.183.160:3000/`** (same /24 as ESP) | **Mis-resolution, not subnet split**                                                                                                                                                                          | Use `**ws://10.175.183.160:3000/weighsoft-weight`** (or fix DNS so `software` → `.160`). Agent `Invoke-WebRequest` to `10.175.183.160:3000` → **HTTP 200**. STATUS UI: green “WebSocket” = browser↔ESP; red “Connection” = ESP→Node-RED.                                                                                                                                                                                                                         |
| 6   | 2026-04-21 | Throttled `[WeightForwarder]` / `[Serial]` Serial0 logs for forward path                    | User: correct `ws_url`, `tcpConnect=1`, still UI Disconnected / no Node-RED; need to distinguish **empty regex weight**, **WS gate**, **handshake stall**, **TX fail**    | **Code**                                                                                                                                                                                                      | Added production-style logs (no UI change): `SerialService` logs `no_weight_capture` every 5s when line parses but regex yields no weight; `WeightForwarderService` logs `skip_forward: empty_weight`, `ws_not_ready`, `ws_handshake_stall`, `ws_tx_ok` / `ws_tx_FAIL`, `outbound_ws: handshake COMPLETE`. UI “Connection” stays false until **successful** `sendTXT` sets `connected` (or briefly true after internal WS CONNECTED then cleared if TX blocked). |
| 7   | 2026-04-21 | Runtime: `WS probe` `101` + valid upgrade headers but no `CONNECTED`; stall logs            | **links2004/WebSockets** `loop()` reconnect throttle interacted badly with `setReconnectInterval(0xFFFFFFFE)`                                                             | **Fix:** removed `setReconnectInterval(WS_LIBRARY_RECONNECT_DISABLE_MS)` and dropped `WS_LIBRARY_RECONNECT_DISABLE_MS` constant; library default (~500 ms) applies; outer service still owns backoff/destroy. | Manual probe proved Node-RED OK; bug was **firmware** gating first TCP inside library, not LAN/WiFi (see [DECISION-LOG-wifi-sta-connectivity.md](DECISION-LOG-wifi-sta-connectivity.md) only for unrelated STA history).                                                                                                                                                                                                                                         |
| 8   | 2026-04-22 | Serial lines `AGENTDBG {…}` in `WeightForwarderService` (debug session `36885a`, NDJSON one line per event) | Operator: COM10 ESP32-WROOM-32D random loss; logs show `WiFiClient::write` errno **11** then `ws_tx_FAIL` — need **heap / wifi / rssi / errno / msSinceWfLoop** correlation | **Flash** COM10 **SUCCESS**; **runtime evidence** (Serial paste): `sendTXT_false` shows **`errno`:11**, **`wifi`:1**, heap **10380**, **`msSinceWfLoop`:8837** — **A+B confirmed**; **C** low heap correlates; **D** starvation of `_wsClient->loop()` vs `sendTXT` path **confirmed** (see §9.3 `msSinceWfLoop` row). | **EAGAIN** + WiFi still up ⇒ not STA drop; root cause class = **not servicing `WebSocketsClient::loop()` often enough** relative to outbound sends (ordering vs `SerialService::loop()`). |
| 9   | 2026-04-22 | `main.cpp`: call `weightForwarderService->loop()` **immediately after** `esp8266React->loop()`, **before** `serialService->loop()` (single WF pass per main iteration). `WeightForwarderService::forwardViaWs`: `_wsClient->loop()` once **before** `sendTXT`. | Same failure class as #8 — drain lwIP/TCP WS TX before serial can enqueue another `sendTXT`; pump client immediately pre-send. | **Code fix** (instrumentation **retained** for post-fix verification). | Complements roadmap **Phase A** (“do not starve Wi‑Fi / WebSocket stacks”): forwarder’s outbound client must not trail serial ingress in the same `loop()` turn. |
| 10  | 2026-04-22 | Post-fix runtime logs (COM10) still failed; added deeper instrumentation in `main.cpp` and `WebSocketTxRx.h` while retaining prior logs. | New hypotheses: **F** main-loop segment stalls prevent timely `_wsClient->loop()`, **G** local AsyncWebSocket server broadcast/backpressure contributes to memory/TCP pressure. Trigger evidence included `[AsyncWebSocket.cpp:427] _queueMessage(): Too many messages queued`. | **Instrumentation + flash to COM10 SUCCESS** (`pio run -e esp32wroom32d -t upload`). Awaiting next runtime capture. | #9 helped pre-send timing in normal windows, but failure still hit `sendTXT_false` with `msSinceWfLoop~8479`; likely an additional stall/pressure subsystem beyond simple call ordering. |
| 11  | 2026-04-22 | Root-cause fix in `SerialService::loop()`: replace `while (SERIAL_PORT.available() && readBudget-- > 0)` with `while (SERIAL_PORT.available() && readBudget > 0) { readBudget--; ... }`. | Runtime evidence from #10: `main_loop_timing` showed `serialMs=18680` ms and websocket failures immediately after (`errno 11`, `ws_tx_FAIL`); this pointed at serial loop overrun. | **Code fix + flash to COM10 SUCCESS** (`pio run -e esp32wroom32d`, `-t upload`). Instrumentation retained for verification. | The bug was arithmetic (unsigned wrap), not just architecture. Under sustained UART availability the loop budget could reopen to `UINT_MAX`, starving all other services and cascading into WebSocket send failure. |
| 12  | 2026-04-22 | Added hard wall-clock guard in `SerialService::loop()` RX phase: `SERIAL_MAX_READ_MS_PER_LOOP` (default 25 ms), plus `AGENTDBG` `hypothesisId=H` when the time budget is hit. | Post-#11 logs (`terminals/30.txt`) still showed `serialMs=4694` and repeated errno 11 bursts, proving long serial phase remained possible even with byte budget. | **Code fix + build SUCCESS** (`pio run -e esp32wroom32d`); one upload attempt failed because COM10 was busy (monitor held port), so verification run is pending fresh flash. | Combining byte + time budgets prevents SerialService monopolizing `loop()` under pathological burst/fanout conditions. |
| 13  | 2026-04-22 | Added Serial publish-rate guard in `flushPendingSerialPublish()`: `SERIAL_MIN_PUBLISH_INTERVAL_MS` (default 100 ms). Pending latest sample is retained; added `AGENTDBG` `hypothesisId=I` (`serial_publish_throttled`). | Runtime after #12 still showed AsyncWebSocket queue overflow + panic; backtrace resolved to `operator new` / AsyncTCP accept path right after queue pressure (`_queueMessage(): Too many messages queued`). | **Flashed + tested**, but issue persisted: queue-overflow events remained and outbound ws still hit errno 11 / `sendTXT_false` (`msSinceWfLoop~9016`) with a later `serialMs=19026` stall in the same run (`terminals/31.txt`). | Throttling `serial_hw` state updates alone is insufficient; pressure also comes from WebSocket fanout behavior and reconnect churn. |
| 14  | 2026-04-22 | Added `serial_hw` broadcast throttle inside `WebSocketTxRx::transmitData()` (`hypothesisId=J`), limiting server WS fanout to one broadcast every 200 ms (state updates still happen for downstream services). | #13 logs showed continued AsyncWebSocket queue overflow with listeners and renewed outbound failures; need to directly cap local WS enqueue pressure without slowing all serial state consumers. | **Code fix + build/upload SUCCESS** (`pio run -e esp32wroom32d`, `-t upload`). Verification run pending. | Moves throttling to the WS fanout edge, preserving SerialService state cadence while reducing browser queue buildup. |
| 15  | 2026-04-22 | **Defer outbound forward:** `onSerialWeightUpdate` only copies `lastLine`/`weight` into pending fields; `flushPendingSerialForward()` runs at start of `WeightForwarderService::loop()` **after** `_wsClient->loop()` on ESP32 (and `#else` branch on non-ESP32). | Post-#14 COM10 logs (`terminals/31.txt`): still `_queueMessage(): Too many messages queued`, heap fell to ~17k, `main_loop_timing` **`serialMs=18415`** then `sendTXT_false` **`msSinceWfLoop`:8403** — proves **multi-second `serialService::loop()`** included work that **prevented** `WeightForwarderService::loop()` from advancing (forward + internal state WS still chained off `SerialService::update` handlers). | **Code fix**; `pio run -e esp32wroom32d` **SUCCESS** (§9.6). On-device verification pending. | **#9’s `msSinceWfLoop` interpretation generalizes:** starvation is not only “RX budget” — **any** heavy work in the **same** `SerialService::update()` propagation (including outbound `sendTXT`) blocks the next `weightForwarderService->loop()` until `serialService::loop()` returns. |
| 16  | 2026-04-22 | **Throttle `/ws/weightforwarder` internal fanout:** `publishForwardSuccessRuntimeState()` — propagate `internal` at most **500 ms** when already `connected` with empty `lastError`; otherwise `updateWithoutPropagation` keeps REST/state fresh. **`AGENTDBG` `ws_client_loop_slow` (`hypothesisId=L`)** when `_wsClient->loop()` ≥200 ms (5 s log throttle). **`serial_hw` server broadcast** min interval **200→400 ms** in `WebSocketTxRx.h`. | Post-#15 logs (`terminals/38.txt`): `serialMs` ~0–81 (**#15 confirmed**), but `forwarderMs` **3794** / **1504**, heap **~17–22k**, AsyncWebSocket queue overflow, errno **11** burst, `sendTXT_false` **`msSinceWfLoop`:8925** — stall moved into forwarder loop; only WeightForwarder uses origin **`internal`** for high-rate success updates → redundant UI WS + heap churn. | **Code fix**; `pio run -e esp32wroom32d` **SUCCESS** (§9.6). On-device verification pending. | Separates **STA**-unrelated symptoms from **second** starvation axis: library `loop()` under TCP pressure + **local** WS enqueue from dual sockets. |
| 17  | 2026-04-22 | **Post-#16 user evidence (Serial paste)** + **`AGENTDBG` `wf_loop_wall` (`hypothesisId=M`)** when `WeightForwarderService::loop()` wall time ≥**250 ms** (5 s throttle): `wfTotalMs`, `wsLoopMs`, `serialFwdPending`, heap, `wifi`. | Operator paste after #16: `main_loop_timing` **`forwarderMs` 5956 / 4902 / 18934** with **`serialMs` ~80**, **`wifi`:1**, RSSI good, bursts **`WiFiClient::write` errno 11** (`"No more processes"` string), **`sendTXT_false`** with **`msSinceWfLoop` ~8922**, then disconnect/reconnect. Snippet had **no** `hypothesisId=L` (`ws_client_loop_slow`) lines — consistent with **L** firing only when a **single** `_wsClient->loop()` ≥200 ms, while **`forwarderMs`** in `main.cpp` can reflect **time since last full `main` iteration** (other services) plus multi-iteration stalls. | **Evidence logged**; **instrumentation** narrows “whole forwarder `loop()` slow” vs “single `wsClient->loop()` slow” on next capture. | Same failure **surface** as #8–#16: cooperative `loop()` + TCP **EAGAIN**; recurrence expected until peer/stack pressure and inter-service timing are fully bounded. |
| 18  | 2026-04-22 | **`WebSocketTxRx.h`:** skip **all** WS **broadcasts** when `ESP.getMaxAllocHeap() < WS_TXRX_MIN_MAX_ALLOC_FOR_BROADCAST` (**28672** default); `AGENTDBG` **`ws_broadcast_skipped_low_max_alloc` (`hypothesisId=P`)** (4 s throttle). **`serial_hw`** min broadcast **400→650 ms**. **`publishForwardSuccessRuntimeState`:** propagating **`internal`** floor **500→750 ms**. | Post-#16/17 Serial (`terminals/38.txt`): **`abort()`** on core **1**; **addr2line** on `.pio/build/esp32wroom32d/firmware.elf` shows **`std::bad_alloc`** in **`AsyncWebServerResponse::addHeader`** under **`AsyncTCP`** — not a Wi‑Fi auth bug; follows errno **11** + **`listeners`:2** + `serial_ws_broadcast_throttled`. | **Code fix**; `pio run -e esp32wroom32d` in §9.6. | **Max free block** matters more than **`getFreeHeap()`** for STL + AsyncWebServer; deferring WS fanout under pressure protects **HTTP** paths from **`abort()`**. |
| 19  | 2026-04-22 | **`WS_TXRX_MIN_MAX_ALLOC_FOR_BROADCAST` default 28672 → 20480** in `WebSocketTxRx.h` (still overridable by build flag). | Post-#18 flash (`terminals/43.txt`): **no** `abort` / `sendTXT_false` / errno **11** in long run; **`P`** logged with **`maxAlloc` 25588** vs **`freeHeap` ~38k** — floor was **above normal plateau**, starving **`serial_hw`** WS UI despite healthy forwarder; prior panic dip had **`maxAlloc` ~9204**. | **Code tune**; build smoke §9.6. | Guard must sit **below** typical **`ESP.getMaxAllocHeap()`** for this image or UI never gets **`serial_hw`** broadcasts. |
| 20  | 2026-04-22 | **Align tree with §9.5 #19 baseline + S3 default env:** removed a short-lived **`serial_hw`–only lower max-alloc floor** (not in this log’s attempt table); restored **single** `ESP.getMaxAllocHeap()` check against **`WS_TXRX_MIN_MAX_ALLOC_FOR_BROADCAST` (20480)** only; reverted extra `wf_heartbeat` JSON fields in `WeightForwarderService.cpp`; `platformio.ini` **`default_envs = esp32s3`** per §9.6 / operator board. | Operator asked to revert off the experimental fork and match the documented **#19** tune; Wi‑Fi STA doc remains **REVERTED** separately ([DECISION-LOG-wifi-sta-connectivity.md](DECISION-LOG-wifi-sta-connectivity.md)). | **Doc + code**; rebuild with `pio run` (default **esp32s3**). | If **`maxAlloc`** on a given board stays **below 20480** while forwarder is healthy, **`serial_hw`** UI broadcasts can still be skipped (**#18** class); tune with **`-DWS_TXRX_MIN_MAX_ALLOC_FOR_BROADCAST=…`** rather than diverging from the logged #19 contract without new evidence. |
| 21  | 2026-04-23 | **Serial Reader (COM10) debug chat plan:** document **STA vs outbound WS vs serial vs server-WS** classification; **§9.7** checklists (**STA / `wifiSettings.json`**, **trace-one-line**, **Phase E**). **Verified in tree:** `[env]` **`-D WEBSOCKETS_TCP_TIMEOUT=1500`** (`platformio.ini`); **`setReconnectInterval(3000)`** after **`new WebSocketsClient()`** in `WeightForwarderService::ensureWebSocketClientAllocated()` (pairs with §9.3 row for ~**5005 ms** `wsLoopMs` / default **5000 ms** library timeout). | Separate **COM10 `NO_AP_FOUND` (201)** captures (**`wifi:0`**) from **STA-up** stalls where **`forwarderMs` ~ `WEBSOCKETS_TCP_TIMEOUT`** and **`ws_client_loop_slow`** — first bucket is RF/config/LittleFS; second is library TCP connect timeout + reconnect cadence + peer handshake. | **`pio run -e esp32wroom32d` SUCCESS** (agent compile smoke). **STA join, end-to-end trace, and Phase E stress** remain **operator bench** — append new rows here when logs exist. | Do not treat **`errno:11` / `sendTXT_false`** as STA credential bugs when **`wifi:1`** in the same window ([DECISION-LOG-wifi-sta-connectivity.md](DECISION-LOG-wifi-sta-connectivity.md) §Outbound WebSocket client). Payload shape includes **`device_id`** ([DECISION-LOG-weight-forwarder-device-id.md](DECISION-LOG-weight-forwarder-device-id.md)). |
| 22  | 2026-04-23 | **Skip `WebSocketsClient::loop()` during service reconnect backoff** when `!isConnected() && !_wsHandshakeInFlight && millis() < _nextWsConnectAllowedAtMs` (`WeightForwarderService.cpp`). **Runtime evidence:** Serial paste `wsLoopMs` **1504–1505**, **`forwarderMs` ~1536**, **`serialMs` 0**, **`wifi:1`**, Live Stream **“Waiting for data…”** — cooperative **`loop()`** starvation of **`flushPublishPhase`** + AsyncWebServer. **`AGENTDBG` `ws_loop_skipped_service_backoff` (`hypothesisId=Q`, session `cd341b`)** throttled 10 s. | **Hypothesis Q CONFIRMED** from logs: library **`loop()`** blocked ~**`WEBSOCKETS_TCP_TIMEOUT`** even while **`checkWsConnection()`** would not yet call **`begin()`** again; skipping **`loop()`** in that window restores fast ticks for **`/ws/serial`**. | **`pio run -e esp32wroom32d` SUCCESS** (post-change compile). **On-device verification pending** — expect **`forwarderMs` << 200** during backoff bursts and Live Stream lines when scale sends text; outbound peer still must be fixed for **`CONNECTED`**. | **Does not** replace correct **`ws_url`** / Node-RED **`websocket-in`**; only removes **pointless** 1.5 s blocks between our own backoff ticks. |


*Add new rows here after each fix attempt (config change, firmware change, Node-RED change), including failures and what the logs proved.*

### 9.6 Build smoke (agent session)


| Command              | Result  | Notes                                                             |
| -------------------- | ------- | ----------------------------------------------------------------- |
| `pio run -e esp32s3` | SUCCESS | Logged after run; refresh this table if env or toolchain changes. |
| `pio run -e esp32wroom32d` | SUCCESS | 2026-04-22 agent build after `AGENTDBG` instrumentation (`WeightForwarderService`). |
| `pio run -e esp32wroom32d` | SUCCESS | 2026-04-22 after §9.5 **#9** (`main.cpp` loop order + `forwardViaWs` pre-`sendTXT` `_wsClient->loop()`); ~95 s incremental. |
| `pio run -e esp32wroom32d` | SUCCESS | 2026-04-22 after §9.5 **#10** instrumentation (`main_loop_timing`, `server_ws_broadcast`, `makeBuffer_null`); ~147 s build. |
| `pio run -e esp32wroom32d -t upload` | SUCCESS | 2026-04-22 flash of §9.5 **#10** instrumentation to COM10 (esptool write + RTS reset). |
| `pio run -e esp32wroom32d` | SUCCESS | 2026-04-22 after §9.5 **#11** serial budget-underflow fix (`readBudget > 0` then decrement); ~101 s build. |
| `pio run -e esp32wroom32d -t upload` | SUCCESS | 2026-04-22 flash of §9.5 **#11** fix to COM10 (esptool write + RTS reset). |
| `pio run -e esp32wroom32d` | SUCCESS | 2026-04-22 after §9.5 **#12** serial RX time-budget guard + `hypothesisId=H`; ~134 s build. |
| `pio run -e esp32wroom32d -t upload` | FAILED | 2026-04-22 COM10 busy (`Access is denied`) while monitor held port; retry required before verification run. |
| `pio run -e esp32wroom32d` | SUCCESS | 2026-04-22 after §9.5 **#13** serial publish-rate throttle (`SERIAL_MIN_PUBLISH_INTERVAL_MS`, `hypothesisId=I`); ~116 s build. |
| `pio run -e esp32wroom32d -t upload` | FAILED | 2026-04-22 COM10 busy (`Access is denied`) while monitor held port; close monitor and retry flash before validation run. |
| `pio run -e esp32wroom32d` | SUCCESS | 2026-04-22 clean rebuild after artifact lock while validating §9.5 **#13**; ~255 s after clean. |
| `pio run -e esp32wroom32d -t upload` | SUCCESS | 2026-04-22 flash retry for §9.5 **#13** succeeded (esptool write + RTS reset). |
| `pio run -e esp32wroom32d` | SUCCESS | 2026-04-22 after §9.5 **#14** WS-edge `serial_hw` broadcast throttle (`hypothesisId=J`); ~117 s build. |
| `pio run -e esp32wroom32d -t upload` | SUCCESS | 2026-04-22 flash of §9.5 **#14** fix to COM10 (esptool write + RTS reset). |
| `pio run -e esp32wroom32d` | SUCCESS | 2026-04-22 after §9.5 **#15** defer `forwardWeight` out of `SerialService::update` handler chain (`WeightForwarderService.{h,cpp}`). |
| `pio run -e esp32wroom32d` | SUCCESS | 2026-04-22 after §9.5 **#16** internal UI throttle + `ws_client_loop_slow` log + `serial_hw` 400 ms broadcast (`WeightForwarderService.{h,cpp}`, `WebSocketTxRx.h`). |
| `pio run -e esp32wroom32d` | SUCCESS | 2026-04-22 after §9.5 **#17** `wf_loop_wall` (`hypothesisId=M`) in `WeightForwarderService.cpp` (agent build smoke). |
| `pio run -e esp32wroom32d` | SUCCESS | 2026-04-22 after §9.5 **#18** max-alloc WS broadcast gate + `hypothesisId=P` + `serial_hw` 650 ms + internal 750 ms. |
| `pio run -e esp32wroom32d -t upload` | SUCCESS | 2026-04-22 post–“issue reproduced” user session: flash to **COM10** after port released (`esptool` write + RTS reset, ~100 s). |
| `pio run -e esp32wroom32d` | SUCCESS | 2026-04-22 after §9.5 **#19** lower `WS_TXRX_MIN_MAX_ALLOC_FOR_BROADCAST` to **20480** (`WebSocketTxRx.h`); ~135 s build. |
| `pio run -e esp32wroom32d` | SUCCESS | 2026-04-23 after §9.5 **#21** / §9.7 debug-chat documentation (`WEBSOCKETS_TCP_TIMEOUT` + operator checklists); agent compile smoke. |
| `pio run -e esp32wroom32d` | SUCCESS | 2026-04-23 after §9.5 **#22** skip `_wsClient->loop()` during service backoff (`WeightForwarderService.cpp`); agent compile smoke. |


### 9.7 Serial Reader (ESP32-WROOM-32D / COM10) — debug chat operator checklists

#### 9.7.1 Classify the current failure bucket (Serial0)

Before changing regex or forwarder code, tag the session:

1. **STA down** — `[Diag][WiFi]` reason **201** (`NO_AP_FOUND`), `[Diag][Health]` `wifi:0`, STA IP **0.0.0.0**: fix **2.4 GHz SSID visible**, **router/AP**, or persisted **`/config/wifiSettings.json`** (LittleFS overrides factory defaults; see [DECISION-LOG-wifi-sta-connectivity.md](DECISION-LOG-wifi-sta-connectivity.md)).
2. **Outbound LAN WebSocket** — `wifi:1` but `ws_tx_FAIL` / `sendTXT_false` / errno **11** / large **`msSinceWfLoop`**: cooperative **`loop()`** starvation or TCP backpressure (§9.5 **#8–#17**); not the same class as **STA down (item 1)**.
3. **Library TCP connect stall** — **`ws_client_loop_slow`** with **`wsLoopMs` ≈ `WEBSOCKETS_TCP_TIMEOUT`** and **`forwarderMs`** matching: failed peer / wrong URL / handshake; mitigations **#21** (shorter timeout + wider reconnect interval) + **#22** (skip **`loop()`** during **service** reconnect backoff so **`/ws/serial`** is not starved between **`begin()`** attempts).
4. **Serial path** — `[Serial] RX rejected …`, `no_weight_capture`: regex or validation (§3 Attempts **6–8**).
5. **Local UI AsyncWebSocket** — `_queueMessage(): Too many messages queued`: server fanout to browsers (§9.5 **#10**).

#### 9.7.2 STA and persisted Wi‑Fi (verify before forwarder tuning)

- Confirm the target SSID is on **2.4 GHz** (ESP32 STA does not use 5 GHz–only networks).
- After a successful join, diagnostics should show STA associated and a **non-zero** STA IP.
- If `factory_settings.ini` credentials seem ignored, inspect device **`/config/wifiSettings.json`**: empty JSON strings for `ssid` / `password` can override factory in non-obvious ways ([DECISION-LOG-wifi-sta-connectivity.md](DECISION-LOG-wifi-sta-connectivity.md)).

#### 9.7.3 Trace one known-good line (end-to-end)

With STA up and the scale emitting one stable accepted line:

1. **Serial0** — no `[Serial] RX rejected (…)` for that line; optional `AGENTDBG` **`serial_publish_sent`** (`hypothesisId=I`).
2. **REST** — authenticated **`GET /rest/serial`**: **`last_line`**, **`weight`**, **`timestamp`** reflect the sample.
3. **Optional browser** — **`/ws/serial`**: UI may lag REST because **`serial_hw`** server broadcasts are throttled (`WebSocketTxRx.h`); REST remains authoritative for “latest line.”
4. **Forwarder** — `[WeightForwarder] serial_ok` (throttled); **`AGENTDBG` `pre_sendTXT`** (`hypothesisId=D`) before outbound send when WS is ready.
5. **LAN peer** — one WebSocket text message; JSON includes **`device_id`**, **`last_line`**, **`weight`**, **`timestamp`** in standard mode ([DECISION-LOG-weight-forwarder-device-id.md](DECISION-LOG-weight-forwarder-device-id.md)).

#### 9.7.4 Phase E stress / noisy serial (roadmap)

When STA and **`ws_url`** are stable, run §3 **Attempt 9** / §6 sign-off style checks:

- Sustained high-rate lines: no watchdog reset; **`AGENTDBG` `main_loop_timing`** (`hypothesisId=F`) stays bounded.
- Mixed binary + text: logs show rejects/drops; no silent corruption of **`lastLine`** / **`weight`**.
- Oversize line: **`too_long`** reject, not truncation.
- Optional: browser on **`/ws/serial`** plus LAN forwarder concurrently.

Append **§9.5** rows and **§8** changelog lines after each bench iteration (per [TASK-DOCUMENTATION-POLICY.md](TASK-DOCUMENTATION-POLICY.md)); do not promote to official `docs/` until product sign-off.


GS,[[:space:]]*\+[[:space:]]*([[:digit:]]+\.[[:digit:]]+|[[:digit:]]+)[[:space:]]*kg is the string