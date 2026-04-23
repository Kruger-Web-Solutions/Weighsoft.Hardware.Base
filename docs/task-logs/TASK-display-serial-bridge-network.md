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

## Related docs

- Primary evidence and attempt table: [DECISION-LOG-serial-scale-instance-1.md](../drafts/DECISION-LOG-serial-scale-instance-1.md) §9.3–9.6.
- STA vs outbound WS distinction: [DECISION-LOG-wifi-sta-connectivity.md](../drafts/DECISION-LOG-wifi-sta-connectivity.md) “Outbound WebSocket client”.
