# Task log: Mandatory roadmap / decision-log workflow

**Purpose:** Checklist so firmware investigations stay traceable and do not mix unrelated root causes (e.g. STA Wi‑Fi vs outbound WebSocket TX).

## When changing runtime behavior

1. **Identify subsystem** from logs (heap, `WiFi.isConnected`, `msSinceWfLoop`, AsyncWebSocket vs `WebSocketsClient`, UART availability).
2. **Record evidence** in the appropriate draft decision log under `docs/drafts/` (for this track: [DECISION-LOG-serial-scale-instance-1.md](../drafts/DECISION-LOG-serial-scale-instance-1.md) §9.5 attempt table + §9.3 symptom rows).
3. **Cross-link** if symptoms could be mistaken for another area (example: [DECISION-LOG-wifi-sta-connectivity.md](../drafts/DECISION-LOG-wifi-sta-connectivity.md) for “errno 11 + ws_tx_FAIL” vs STA association).
4. **Update `docs/task-logs/`** for the active product task so operators know what was fixed and how to re-verify (see [TASK-display-serial-bridge-network.md](TASK-display-serial-bridge-network.md)).
5. **Build smoke:** add a row to §9.6 (or equivalent) when running `pio run` / upload for that attempt.

## 2026-04-22 update

- Added **§9.5 #15** to serial-scale decision log: defer `forwardWeight` out of `SerialService::update` handler chain; documented COM10 log pattern (`serialMs` ~18415 with `msSinceWfLoop` ~8403 at `sendTXT_false`).
- Extended Wi‑Fi draft log changelog + evidence trail to **#15** so readers do not attribute outbound client starvation to STA credential bugs.
