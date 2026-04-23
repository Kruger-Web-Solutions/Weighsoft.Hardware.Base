# Task roadmap & decision log: Weight Forwarder `device_id`

**Document type:** working task log (draft)  
**Status:** `IN PROGRESS` — implementation candidate in working tree; **on-device protocol validation pending operator sign-off**  
**Official docs:** `docs/API-REFERENCE.md`, `docs/DATA-FLOWS.md`, etc. **must not be updated** until testing is complete and the product owner explicitly signs off (per [TASK-DOCUMENTATION-POLICY.md](TASK-DOCUMENTATION-POLICY.md)).

**Non-final notice:** Treat behavior described here as **candidate** until validated on hardware against real HTTP / WebSocket / MQTT / BLE consumers.

---

## 1. Task scope (roadmap)

| Phase | Goal | Status |
| ----- | ---- | ------ |
| A | Persist and expose auto `device_id` (MAC-based `#{unique_id}`) on REST + `/ws/weightForwarder` | **Done (code)** — pending device check |
| B | Include `device_id` in all forward payloads (HTTP, WS, MQTT, BLE), standard + display mode | **Done (code)** — pending peer verification |
| C | Read-only **Device ID** field on Weight Forwarder config UI | **Done (code)** |
| D | Backward compatibility: configs without `device_id` load and bootstrap once | **Done (code)** |
| E | Sign-off + optional promotion to `docs/API-REFERENCE.md` | **Blocked** until explicit approval |

---

## 2. Problem statement

- Multiple ESPs forward weight events; downstream systems need a stable **per-device** identifier alongside `last_line`, `weight`, and `timestamp`.
- ID must be automatic (not user-editable) and derived from device identity already used elsewhere (`SettingValue::format("#{unique_id}")`).

---

## 3. Decision log (attempts)

### Attempt 1 — User-editable `id` text field (rejected for this task)

- **What:** Original brainstorm considered a free-text config field.
- **Why rejected:** Product owner chose **auto device ID** and JSON key **`device_id`**.
- **Learning:** Lock schema early (`device_id` vs `id`).

### Attempt 2 — Candidate: `device_id` in state + payloads + read-only UI

- **What:**
  - `WeightForwarderState`: new persisted `deviceId` ↔ JSON `device_id` in `read` / `readConfig` / `updateConfig`; **not** writable via `update()` (REST/WebSocket clients cannot spoof).
  - `WeightForwarderService::begin()`: after `readFromFS()`, if `device_id` empty, set from `SettingValue::format("#{unique_id}")`, `update(..., "internal")`, then `_fsPersistence.writeToFS()` once.
  - `formatJson()`: always emit `device_id` (fallback to `SettingValue::format` if empty); standard mode keeps `last_line`, `weight`, `timestamp`; display mode adds `device_id` alongside `line1` / `line2`.
  - Bump forward `DynamicJsonDocument` capacity **256 → 384** for the extra field.
  - UI: read-only **Device ID** `TextField` on config screen; TypeScript `device_id` on `WeightForwarderData`.
- **Why selected:** Matches framework patterns (`SettingValue`), snake_case JSON, minimal flash writes, backward compatible with old JSON files missing `device_id`.
- **Tradeoffs:** Display-mode consumers that strictly allow only `line1`/`line2` must tolerate an extra key (or filter).
- **Risks:** MAC-derived id changes if STA MAC / chip identity policy changes; strict downstream parsers may need updates.
- **Validation evidence:** See §5 changelog after local `pio run` / `interface` build.

---

## 4. File touch list (this task)

| Path | Role |
| ---- | ---- |
| `src/examples/weightforwarder/WeightForwarderState.h` | State + JSON persistence for `device_id` |
| `src/examples/weightforwarder/WeightForwarderService.cpp` | Bootstrap ID, `formatJson`, document size |
| `interface/src/types/weightForwarder.ts` | TypeScript contract |
| `interface/src/examples/weightforwarder/WeightForwarderConfig.tsx` | Read-only Device ID field |

---

## 5. Changelog for *this* decision document

| Date (UTC) | Author | Change |
| ---------- | ------ | ------ |
| 2026-04-22 | Agent | Initial draft: scope, decisions, file list; status **IN PROGRESS**. |
| 2026-04-22 | Agent | Validation: `python -m platformio run -e esp32wroom32d` **SUCCESS** (exit 0); includes `interface` `npm run build` as part of PIO env. **Does not** replace on-device forward path checks (§6). |
| 2026-04-23 | Agent | Cross-ref: [DECISION-LOG-serial-scale-instance-1.md](DECISION-LOG-serial-scale-instance-1.md) **§9.7.3** (*trace one known-good line*) — operator procedure to confirm **`device_id`** + **`last_line`** + **`weight`** + **`timestamp`** on the LAN peer after REST/Serial path. |
| 2026-04-23 | Agent | **Not payload-related:** Serial Live Stream empty with **`forwarderMs`~1505** was **main-loop starvation** from **`WebSocketsClient::loop()`** during reconnect backoff — fixed in serial-scale log **§9.5 #22**; **`device_id`** / JSON shape unchanged. |
| 2026-04-23 | Agent | Read-only verification plan todos: §7 product-owner UI decision template; §8–§10 bench + `esp32wroom32d` env; **`platformio.ini`** gains `[env:esp32wroom32d]` (`esptool`, `COM10`). On-device soak still **pending operator**. |

---

## 6. Sign-off checklist (product owner)

- [ ] REST `GET /rest/weightForwarder` returns non-empty `device_id` matching device identity expectations  
- [ ] Outbound JSON over **WebSocket client** includes `device_id` + `last_line` + `weight` + `timestamp` (standard mode)  
- [ ] Same for **HTTP POST**, **MQTT**, **BLE** if used in deployment  
- [ ] **Display mode:** peer accepts JSON with `device_id` + `line1` + `line2`  
- [ ] Upgrade from config file **without** `device_id`: first boot writes id and forwards succeed  
- [ ] Explicit written sign-off to update official `docs/API-REFERENCE.md` (optional)

**Sign-off name / date:** *(pending)*

---

## 7. Product owner decision — Device ID UI vs “no UI changes” (**pending**)

Some verification briefs require **no UI changes**. This task added a **read-only** “Device ID” field on the Weight Forwarder configuration screen ([`interface/src/examples/weightforwarder/WeightForwarderConfig.tsx`](../../interface/src/examples/weightforwarder/WeightForwarderConfig.tsx)).

| Option | Meaning |
| ------ | ------- |
| **A — Keep read-only field** | Operators can confirm `device_id` in the UI without editing it. |
| **B — Remove field; REST/WS only** | Zero UI surface change; operators use `GET /rest/weightForwarder` or `/ws/weightForwarder` only. |
| **C — Move to Status tab only** | Config screen unchanged; ID visible elsewhere (requires follow-up work — not done here). |

**Product owner:** record choice (A / B / C) and date below.

**Decision:** *(pending — A / B / C)*  
**Name / date:** *(pending)*

---

## 8. Read-only verification report (summary)

Cross-reference: read-only audit that lists **issues only** (no firmware/UI edits in that pass) lives in the Cursor plan **“Read-only verification report”** (same content as checklist below for traceability).

**Findings captured there (short form):**

- `device_id` is present in firmware + types; **UI field exists** → see §7 if strict “no UI” applies.
- **`esp32wroom32d` env** was missing from `platformio.ini` at audit time → addressed in §9 / `platformio.ini`.
- Serial **`weight`** aligns with REST/WS schema; Live Stream depends on `last_line` being truthy (edge case noted in plan).
- Outbound forwarder WS: IP or hostname OK if URL shape and DNS are OK; **100% uptime** is not provable in static review — use §10 soak.
- Root `[env]` OTA defaults may conflict with USB workflows unless per-env overrides — `esp32wroom32d` sets `esptool` + `COM10`.

---

## 9. PlatformIO environment `esp32wroom32d`

**Added** `[env:esp32wroom32d]` in [`platformio.ini`](../../platformio.ini): same `board` / partitions / `build_flags` pattern as `esp32dev`, with **`upload_protocol = esptool`** and **`upload_port = COM10`** so the env name used in historical build logs exists again. **Adjust `upload_port`** if your PC uses a different COM port.

Build example:

```text
python -m platformio run -e esp32wroom32d
python -m platformio device monitor -e esp32wroom32d -b 115200
```

---

## 10. On-device bench checklist — Node-RED / Serialwriter (**operator**)

Agent cannot run hardware soak tests here; execute locally and append results to §5 changelog.

1. **Flash** with the env you ship (`esp32wroom32d` or other), same `factory_settings` / WiFi as production.
2. **Weight Forwarder:** set protocol WebSocket, `ws_url` e.g. `ws://10.175.183.135:3000/weighsoft-weight` (try **literal IP** first, then **hostname** if DNS is trusted).
3. **Peer capture:** confirm each message JSON includes `device_id`, `last_line`, `weight`, `timestamp` (standard mode) or `device_id` + `line1` + `line2` (display mode).
4. **Serial UI / REST:** `GET /rest/serial` shows `weight`; Live Stream shows lines when `last_line` updates (see plan edge case).
5. **Soak (stability):** run **≥ 30–60 min** under real scale traffic; log ESP `connected` / `last_error` and Node-RED disconnect events. **“Never disconnect”** is a network/product goal — document any drop with cause (WiFi, peer restart, `sendTXT` failure, etc.).

**Bench result summary:** *(operator: pending)*  
**Evidence (log path / screenshot):** *(optional)*
