# SerialWriter forwarder — WebSocket disconnect runbook

Use this document in order when the **serialWriter** device keeps losing its outbound WebSocket to the reader’s **`/ws/serial`**. It complements [SERIAL-WRITER-EXAMPLE.md](./SERIAL-WRITER-EXAMPLE.md) and [NODE-RED-SERIAL-READER-DEBUG.md](./NODE-RED-SERIAL-READER-DEBUG.md).

## `abort()` / reboot right after `DISCONNECTED` (not the reader “idle code”)

If UART shows **`abort() was called at PC 0x...`** then **`Rebooting...`**, that is **software abort** (failed assertion, heap corruption, or **C++ exception / `std::function` teardown**), **not** a brownout and not explained by a bogus **`rfc6455_code=21571`** line alone.

- **`esp_core_dump_flash: ... corrupted` / `No core dump partition found`** — partition-table noise; unrelated to `abort()` unless you use ESP-IDF core dumps.

**Symbolize the crash PC** against the **ELF that matches the flashed firmware** (same `pio run` tree):

```text
%USERPROFILE%\.platformio\packages\toolchain-xtensa-esp32s3\bin\xtensa-esp32s3-elf-addr2line.exe -pfiaC -e .pio\build\esp32s3\firmware.elf <PC_from_log>
```

Example PCs from field logs have resolved into **libstdc++** (`std::type_info`, `__cxa_end_catch`, `__cow_string` / exception helpers) — consistent with **destroying `WebSocketsClient` while the library callback / AsyncTCP path is still unwinding** (use-after-free in `std::function` / exception path).

**Firmware mitigation (0.7.8+):** [`SerialWriterForwarderService.cpp`](../src/examples/serialwriter/SerialWriterForwarderService.cpp) adds an **`onEvent` re-entrancy depth counter** with a **bounded wait** before `disconnect()`/`delete`, plus a **75 ms defer** after `DISCONNECTED` / `ERROR` before `checkWsConnection()` may call `initWsClient()` again. After flashing, stress-test by bouncing reader power or blocking port 80; confirm no `abort()` over many cycles.

## Phase 1 — Environment and baseline (no firmware change)

### 1.1 Writer UI isolation

1. Close **every** browser tab pointed at the **writer** (forwarder UI, React app, devtools).
2. Watch writer UART for **`[WS] Client connected`** (from [`WebSocketTxRx.h`](../lib/framework/WebSocketTxRx.h) on the writer’s **inbound** UI sockets — not the reader).
3. Record: **`[WS] Client connected` still appears?** yes / no. If yes, something else (another PC, phone, Node-RED to writer, etc.) is opening a socket; track it down.

### 1.2 Node-RED baseline (optional but strong A/B)

Follow [NODE-RED-SERIAL-READER-DEBUG.md](./NODE-RED-SERIAL-READER-DEBUG.md): sign-in, paste **`ws://.../ws/serial?...`** into the websocket-client node, deploy, run **debug frame summary** for several minutes.

- Note **wall-clock time** of any long gap with no frames.
- **Agent check (lab):** `GET http://127.0.0.1:3000/` should return **200** when Node-RED is running locally (verified during runbook authoring).

### 1.3 PC control channel (same LAN as reader)

From a PC, open **`ws://<reader-ip>/ws/serial?access_token=<token>`** (PowerShell / browser / `wscat` — see SERIAL-WRITER-EXAMPLE “Manual WebSocket” section).

- Leave the connection **idle** for several minutes.
- Record whether disconnect **cadence** matches the writer (same ~8 s pattern vs stable).

### 1.4 Quick interpretation

| NR + PC idle WS | Writer disconnects |
|-----------------|----------------------|
| Stable | Likely **writer** path (WiFi, dual AP+STA, lwIP) — continue with Phase 2–4 on writer. |
| Same cadence as writer | Likely **reader, AP, or proxy** — use Phase 5 **serialReader** handoff; writer heartbeat alone may not fix root cause. |
| Mixed | Capture Phase 2 table; add **reader UART** timestamps if possible. |

---

## Phase 2 — Structured log capture (template)

Copy rows while reproducing. **`t_ms`** is `millis()` from the writer log line **`[ws-event]`** (or wall time + boot offset).

| `t_ms` | event | `len` | `had_text` before DISCONNECT? | `ws-close` line (after 0.7.6+) | `last_error` / UI | WiFi ok? | NR / PC note |
|--------|-------|-------|-------------------------------|--------------------------------|---------------------|----------|--------------|
| | CONNECTED | 0 | — | — | | | |
| | TEXT | | — | — | | | |
| | DISCONNECTED | | yes/no | rfc6455_code=… | Source disconnected / … | | |

Reference log patterns: [SERIAL-WRITER-EXAMPLE.md](./SERIAL-WRITER-EXAMPLE.md) sections **“Expected log lines per state”** and **“Periodic disconnects (~8 s)”**.

---

## Phase 3 — Firmware (writer, `serialWriter` branch)

From **0.7.6** onward, **`[SerialWriterForwarder][ws-close]`** lines decode the WebSocket **close frame** on `WStype_DISCONNECTED` (**0.7.8+** adds **teardown guards** — see [`abort()` section](#abort--reboot-right-after-disconnected-not-the-reader-idle-code) above):

- **`no_close_payload`** — `len == 0` on disconnect event (common with TCP reset / abnormal closure; often discussed as **1006**-class when no close frame was delivered).
- **`stack_msg text='...'`** (from **0.7.7**) — the `WebSocketsClient` library passed a **plain ASCII** status string (e.g. **`TCP connection cleanup`** from lwIP / ESP32 TCP), **not** a binary RFC 6455 close frame. Older builds mis-read the first two letters as a bogus “code” (e.g. `21571` = `'T'<<8|'C'`).
- **`rfc6455_code=<n>`** — first two payload bytes are network-byte-order [RFC 6455](https://datatracker.ietf.org/doc/html/rfc6455#section-5.5.1) close code (only when the payload does **not** look like printable ASCII stack text).
- **`reason_hex=...`** — up to **32** bytes hex of optional UTF-8 reason after the code (truncated if longer).

**Reading your sample trace:** mid-session **`no_close_payload`** disconnect → token kept → reconnect → **`stack_msg` … `TCP connection cleanup`** means the **TCP layer** dropped the socket (reader reset, WiFi glitch, reader reboot, or full listener sockets) **without** a normal WebSocket close frame. Immediately after, **`http_code=-1`** on sign-in means the writer could not open **new TCP** to the reader — same incident class (reader or path unreachable until it recovers). Correlate with **reader UART** and a **third-host** `Test-NetConnection 10.45.71.5 -Port 80` while the writer is stuck.

Source: [`SerialWriterForwarderService.cpp`](../src/examples/serialwriter/SerialWriterForwarderService.cpp) (`WebSocketsClient` outbound client).

---

## Phase 4 — After flash: classify and choose next owner

Fill this worksheet from UART (and NR/PC if run).

### Close code cheat sheet

| Code | Typical meaning |
|------|-----------------|
| 1000 | Normal closure (endpoint chose to close cleanly). |
| 1001 | Going away (e.g. server restart or navigated away). |
| 1006 | *Not* sent in frame — means no close frame / abnormal (often with **`no_close_payload`** on client). |
| 1008 | Policy violation (e.g. auth/subprotocol rejected). |
| 1011 | Server error / unexpected condition. |

### Decision

- **Writer-only** drops while PC + NR stay up → focus **WiFi**, **STA vs soft-AP**, **`WiFi.reconnect`** / `[wifi-recover]` logs, bisect notes in SERIAL-WRITER-EXAMPLE (**0.7.5+** behaviour). Do **not** change `enableHeartbeat` intervals without evidence.
- **All clients** drop on same idle cadence with **1000/1001** from server → treat as **reader policy** (see Phase 5).
- **`no_close_payload` or reset-like** on all paths → **AP / TCP / reader crash** — correlate with reader UART and power.

---

## Phase 5 — Handoff: **serialReader** integration branch (separate repo/branch)

**`/ws/serial` is not registered in the `serialWriter` tree** (no `.cpp` match in this branch). Reader behaviour is implemented on the **serialReader** (or **`serial2`**) integration baseline.

When Phase 1–4 point here, apply on **serialReader**:

1. **Find** the handler that registers **`/ws/serial`** (likely `WebSocketTxRx` or `AsyncWebSocket` on `AsyncWebServer`, same patterns as [`WebSocketTxRx.h`](../lib/framework/WebSocketTxRx.h)).
2. **Log** `WS_EVT_DISCONNECT` (and optionally `WS_EVT_ERROR`) with **`data`/`len`** if AsyncWebSocket exposes close status for that event — mirror what the writer now logs for the **client** side.
3. **Review** max concurrent WebSocket clients, **heap** under broadcast, and any **idle timer** that closes clients when scale JSON unchanged (matches “~8 s” hypothesis in SERIAL-WRITER-EXAMPLE).
4. **Align** server ping expectations with the writer’s outbound **`enableHeartbeat(15000, 3000, 2)`** if idle closes are policy-driven.

Commit those changes on the **serialReader** branch; link this runbook from that branch’s changelog or README when done.

---

## Phase 6 — Build verification

From repo root (Windows example):

```powershell
pio run -e esp32s3
```

Use the same **PlatformIO environment** you flash to the writer hardware.

---

## Related scripts

- Re-deploy Node-RED **serialReader WS monitor** tab: [`scripts/patch_nodered_serialreader_flows.py`](../scripts/patch_nodered_serialreader_flows.py).
