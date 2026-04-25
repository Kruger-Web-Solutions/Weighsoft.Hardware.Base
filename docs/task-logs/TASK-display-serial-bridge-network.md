# Task log: Display firmware — serial bridge, WebSocket, WiFi, stability

| Field | Value |
|--------|--------|
| **Status** | **DRAFT / IN PROGRESS** — not signed off |
| **Sign-off** | Pending (testing + your explicit approval) |
| **Hardware focus** | Node32s display (COM10 in `platformio.ini`), serial/weighing ESP as data source |
| **Last updated** | 2026-04-20 |

---

## 1. Roadmap

### 1.1 Goals

- [x] Display connects to correct serial/weighing device (not wrong COM / wrong board).
- [x] WebSocket bridge supports paths used by weighforwarder (`/ws/display`) and legacy serial (`/ws/serial`).
- [x] Parse `WebSocketTxRx`-style envelopes (`type`/`payload` with `line1`/`line2`) and `last_line` JSON.
- [x] Reduce LCD flicker and redundant work under rapid updates.
- [x] Station WiFi factory defaults: SSID **Weighsoft**, password **Monday@011** (`factory_settings.ini`).
- [ ] **Pending validation:** end-to-end on hardware (rapid updates, WiFi drop, auth on remote `/ws/display` if enabled).

### 1.2 Out of scope / deferred

- Remote WebSocket **JWT/Bearer** handshake for secured `/ws/display` on source device (mentioned as follow-up; not implemented unless added later).
- Automated stress tests in CI (manual / field validation only so far).

---

## 2. Decision log (chronological)

### 2.0 WiFi does not connect after changing factory network (2026-04-20) — **DRAFT fix**

| | |
|--|--|
| **Symptom** | Device does not join **Weighsoft** after updating `factory_settings.ini` / reflashing. |
| **Hypothesis A** | `/config/wifiSettings.json` on LittleFS still holds **previous** SSID/password; factory compile-time defaults only apply when keys are missing, not when file overwrites them. |
| **Hypothesis B** | `WiFiSettings::update` used `root["ssid"] \| FACTORY` — ArduinoJson does **not** substitute factory when JSON value is **`""`** (empty string). That yields `ssid.length()==0` and `manageSTA()` **never calls `WiFi.begin()`**. |
| **Hypothesis C** | `manageSTA()` only called `WiFi.begin()` when `(WiFi.getMode() & WIFI_STA)==0`. After first `begin()`, STA often stays enabled while disconnected → **no further `WiFi.begin()`** → no retries after failed join. |
| **What we changed** | (1) After `\|`, if `ssid` or `password` is empty and factory string is non-empty, assign factory. (2) Remove STA-mode gate; ensure ESP32 uses `WIFI_AP_STA` if AP already up else `WIFI_STA`; ESP8266 `enableSTA(true)` when STA off. (3) While `WL_IDLE`/`WL_SCAN_COMPLETED` and `reconnectAttempts>0`, skip one interval to avoid backoff inflation during handshake. |
| **Outcome** | `pio run -e node32s` **SUCCESS**; upload **COM10 SUCCESS**. |
| **Still not solved by code alone** | If FS contains a **non-empty wrong SSID** (not empty), firmware will still use that value until user saves correct WiFi in UI or erases `/config` / full flash erase. |
| **Learned** | Distinguish “factory default in ROM” vs “persisted JSON on FS”; document erase path for credential migration. |

### 2.0b Further fix — STA still would not join (2026-04-20) — **DRAFT**

| | |
|--|--|
| **What** | (1) ESP32 `onStationModeDisconnected` called `WiFi.disconnect(true)` on **every** `ARDUINO_EVENT_WIFI_STA_DISCONNECTED`, including during association, aborting join. (2) `APSettingsService::manageAP()` with `AP_MODE_DISCONNECTED` started softAP whenever mode was `WIFI_STA` and `WiFi.status() != WL_CONNECTED` — true **during** WPA handshake — so AP came up immediately and broke STA on many APs. |
| **Outcome** | Removed `disconnect(true)` from disconnect handler (log reason only). Defer softAP start while `WIFI_STA` and status is `WL_IDLE_STATUS` or `WL_SCAN_COMPLETED`. |
| **Validation** | `pio run -e node32s` SUCCESS; flash COM10 pending / done in same session. |

---

### 2.1 Wrong board flashed (COM13 vs COM10)

| | |
|--|--|
| **What** | First uploads auto-selected **COM13**; user’s display is on **COM10**. |
| **Why** | PlatformIO auto-detect picks first available port. |
| **Outcome** | Wrong MAC flashed (`38:18:2b:18:e3:cc` vs display `8c:ce:4e:a7:81:24`). |
| **Rejected / failed** | Relying on auto-detect alone. |
| **Learned** | Pin `upload_port` / `monitor_port` for the display Node32s. |

**Resolution (current, not final):** `platformio.ini` `[env:node32s]` sets `upload_port = COM10` and `monitor_port = COM10`.

---

### 2.2 Board identity confusion (ESP32-S3 vs “ESP32 S” vs Node32s)

| | |
|--|--|
| **What** | Added `default_envs = esp32s3`, added `[env:esp32s]` (`nodemcu-32s`), then user clarified hardware is **Node32s**. |
| **Why** | Misread “ESP32 S” as S3 or NodeMCU-32S. |
| **Outcome** | `esp32s` env removed; `default_envs = node32s` restored. |
| **Reverted** | Yes — `esp32s` block removed from `platformio.ini`. |
| **Learned** | Confirm exact PlatformIO `board =` with user before changing defaults. |

---

### 2.3 Serial WebSocket “disconnected” — wrong path

| | |
|--|--|
| **What** | Outbound client used hardcoded **`/ws/serial`** only. |
| **Why** | Original serial-example architecture assumed `/ws/serial` + `last_line`. Weighforwarder mirrors **display** stream on **`/ws/display`**. |
| **Outcome** | Connection failed or no useful payload if source only exposes `/ws/display`. |
| **Failed** | Connecting without matching source path. |
| **Learned** | Path must be configurable; default should align with weighforwarder (`/ws/display`). |

**Resolution (current):** `serial_ws_path` in REST/state, UI field, firmware uses path with leading `/` normalization; JSON parsing handles `payload` + `last_line`.

---

### 2.4 JSON shape mismatch (weighforwarder vs serial stream)

| | |
|--|--|
| **What** | Client only read root `last_line`. |
| **Why** | `/ws/display` uses `WebSocketTxRx` wire format: `{"type":"payload","payload":{...}}` plus `id` handshake. |
| **Outcome** | LCD ignored valid messages or mis-handled `id`. |
| **Failed** | Single-field `last_line` assumption for all paths. |
| **Learned** | Centralize parsing (`processInboundBridgeJson`) for WS, MQTT, BLE. |

---

### 2.5 LCD flicker / rapid updates

| | |
|--|--|
| **What** | `applyBridgeLinesToLcd` drew LCD, then `update(..., "serial_bridge")` triggered `onConfigUpdated()` which **cleared and redrew** again. |
| **Why** | Update handler did not use `originId`. |
| **Outcome** | Visible flicker under high message rate. |
| **Failed** | Unconditional full LCD refresh on every state update. |
| **Learned** | Skip redundant LCD pass when `originId == "serial_bridge"`; skip hardware update if padded lines unchanged. |

**Resolution (current):** `onConfigUpdated(originId)`; skip clear/print for `serial_bridge`; duplicate line detection in `applyBridgeLinesToLcd`.

---

### 2.6 Bridge reconnect interval

| | |
|--|--|
| **What** | `loop()` reconnect every **5 s** when disconnected; reset timer on `WStype_CONNECTED`. |
| **Why** | Recover from WiFi drops without hammering the server. |
| **Outcome** | Reasonable default; not exhaustively tuned in field. |
| **Tradeoff** | Slower reconnect than aggressive polling; lower load. |

---

### 2.7 WiFi credentials (Weighsoft / Monday@011)

| | |
|--|--|
| **What** | User requested SSID/password applied “where applicable” and flash. |
| **Why** | Deployment standard for lab/site. |
| **Outcome** | `factory_settings.ini` already contained correct `FACTORY_WIFI_*`; comment clarified; **README** section corrected (removed misleading “Weighsoft-HW” AP wording; clarified STA vs AP). |
| **Risk** | **Persisted `/config/wifiSettings.json` on device** overrides factory defaults until changed or FS erased—communicated to user. |
| **Validation** | Build + upload **SUCCESS** to COM10; **not** same as “WiFi joined Weighsoft” without runtime log. |

---

### 2.8 UI changes (Serial Bridge)

| | |
|--|--|
| **What** | Added WebSocket path field; temporarily expanded info `Alert`; reverted Alert to **original short copy** per user (“stick to original UI”). |
| **Why** | Path configuration required; verbose alert was extra. |
| **Outcome** | Path field + one-line helper retained; alert text restored to generic three-mode description. |

---

## 3. Current solution (NOT FINAL — pending sign-off)

### 3.1 Why this approach was selected

- **Configurable path** integrates weighforwarder and legacy serial without forking firmware.
- **Shared JSON parser** keeps MQTT/BLE/WS behavior aligned (integration stability).
- **Origin-aware LCD refresh** removes double-draw without changing REST/WebSocket contracts for the display service.

### 3.2 Tradeoffs

- Skipping LCD redraw for `serial_bridge` assumes bridge path always updates LCD in `applyBridgeLinesToLcd` first—true for current code paths.
- Duplicate-line suppression avoids spam but **hides** repeated identical frames (usually desirable for weight).

### 3.3 Risks (open until validated)

| Risk | Mitigation / note |
|------|-------------------|
| Secured remote `/ws/display` rejects unauthenticated client | Needs Bearer or open path on source; not in current firmware |
| Old `wifiSettings.json` on flash | User must clear WiFi or erase config for factory STA to apply |
| README / `factory_settings` touched before strict “no user-facing doc” rule | Listed here; review at sign-off |

### 3.4 Validation evidence (so far)

| Check | Result |
|--------|--------|
| `pio run -e node32s` after display changes | **SUCCESS** (multiple iterations in session) |
| `pio run -e node32s -t upload` to COM10 | **SUCCESS**; MAC `8c:ce:4e:a7:81:24` |
| Field stress / disconnect soak | **Pending** (manual) |
| User sign-off | **Pending** |

---

## 4. Sign-off checklist (for you)

When satisfied, reply with sign-off and optionally edits. The agent may then:

- [ ] Mark this document **APPROVED** with date.
- [ ] Optionally promote selected content to official docs **only if** you request it.

---

## 5. Appendix — files touched (working memory)

| Area | Files (non-exhaustive) |
|------|-------------------------|
| Firmware | `src/examples/display/DisplayService.{h,cpp}`, `platformio.ini` |
| Factory / WiFi | `factory_settings.ini` |
| UI | `interface/src/examples/display/DisplaySerialBridge.tsx`, `interface/src/types/display.ts` |
| Docs | `README.md` (WiFi/AP wording); **task logs** this folder |
| Reverted / removed | `[env:esp32s]` removed from `platformio.ini` |

---

*This file is the authoritative decision trail for this task until you sign off. After “follow the rules,” the agent must update this file (or the active `TASK-*.md` for the task) as work proceeds.*
