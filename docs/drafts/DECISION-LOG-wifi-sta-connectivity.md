# Task roadmap & decision log: STA WiFi after factory credential change

**Document type:** draft task log  
**Status:** `REVERTED` — `WiFiSettingsService.{h,cpp}` restored to **git HEAD** (original framework). Only `factory_settings.ini` keeps STA SSID/password **Weighsoft** / **Monday@011**; `FACTORY_WIFI_APPLY_STAMP` removed. ESP32 ctor now explicitly initializes `_stopping(false)` (small safety fix on top of HEAD).  
**Official docs:** not updated (per policy).

---

## Problem

After changing `FACTORY_WIFI_SSID` / `FACTORY_WIFI_PASSWORD` in `factory_settings.ini` and flashing, the ESP32 **did not join** the new network.

---

## Root cause analysis (evidence-based)

1. **LittleFS overrides factory defaults**  
   `WiFiSettingsService::begin()` calls `FSPersistence::readFromFS()`, which loads `/config/wifiSettings.json`. Those values replace compile-time `FACTORY_WIFI_*` whenever the file exists (normal design).

2. **ArduinoJson `|` operator does not treat empty string as “missing”**  
   Previous code used `root["ssid"] | FACTORY_WIFI_SSID`. If JSON contained `"ssid":""` or `"password":""`, the empty string **wins** — factory defaults never apply. Result: `WiFi.begin` with empty SSID (no association) or wrong password (auth failure).

3. **Password `Monday@011` is passed correctly through PlatformIO**  
   Verbose build flags show `-DFACTORY_WIFI_PASSWORD=\\\"Monday@011\\\"` intact — **not** an `@` truncation issue in this environment.

4. **Rejected hypothesis (without device logs):** RF band (2.4 vs 5 GHz only AP) — cannot fix in firmware; still possible if router is 5 GHz–only.

---

## Attempts / decisions

| # | What | Outcome |
|---|------|---------|
| A | Assume INI `@` escaping broken | **Rejected** — build flags contain full password |
| B | Merge rules: empty SSID → factory; missing password key → factory; empty password + SSID matches factory SSID + factory password non-empty → factory password | **Selected** — fixes bad/empty persisted rows without breaking intentional open networks on non-factory SSIDs |
| C | `FACTORY_WIFI_APPLY_STAMP` + `wifi_factory_stamp` in JSON; on boot if `stamp < STAMP`, overwrite STA creds from factory and persist | **Selected** — fixes “old SSID/password still in LittleFS after factory change” |
| D | Always reset `wifi_factory_stamp` to `0` when JSON key absent | **Rejected** — UI POST omits key → would reset stamp every save → factory overwrite every boot |
| E | Preserve `wifi_factory_stamp` when JSON key absent (keep prior `settings` value) | **Selected** — pairs with (C) safely |

---

## Tradeoffs / risks

- **Stamp `1` in repo `factory_settings.ini`:** First boot after this firmware on any device with `wifi_factory_stamp < 1` **once** reapplies factory STA credentials. Fleets with **different** saved SSIDs will be overwritten unless they bump their stamp in JSON or set `FACTORY_WIFI_APPLY_STAMP=0` / remove the define.
- **Empty password + SSID equals factory SSID:** Treated as mis-save and filled with factory password — **cannot** model an open (no-PSK) network whose SSID exactly equals `FACTORY_WIFI_SSID` while `FACTORY_WIFI_PASSWORD` is non-empty. Edge case.

---

## Validation (so far)

- PlatformIO verbose output: `FACTORY_WIFI_PASSWORD` and `FACTORY_WIFI_APPLY_STAMP=1` present in compile line.
- `pio run -e esp32wroom32d -t compiledb` **SUCCESS** (sanity).

---

## Follow-up (still IN PROGRESS — user reported “does not work”)

**Additional root causes addressed:**

| Issue | Fix |
|-------|-----|
| ESP32 ctor ends in `WIFI_MODE_NULL`; `WiFi.begin()` without an explicit `WiFi.mode(WIFI_STA)` / `WIFI_AP_STA` is unreliable | `ensureWifiModeForStaAssociation()` before `WiFi.begin()` |
| `onStationModeDisconnected` called `WiFi.disconnect(false)` on **every** disconnect, racing `WiFi.begin()` / auth | Removed disconnect call; log reason only |
| Hostname in JSON could contain literal `#{platform}-#{unique_id}` without `SettingValue::format`, breaking `setHostname` | Always run `SettingValue::format()` on loaded hostname |
| Leading/trailing spaces in SSID/password from JSON | `.trim()` after load |
| Devices already at `wifi_factory_stamp=1` after first fix never re-pulled factory creds | Bump `FACTORY_WIFI_APPLY_STAMP` to **2** in `factory_settings.ini` |
| STA drops with reason 2 (`AUTH_EXPIRE`) then `STA_STOP` while logs show immediate `WiFi.disconnect` from disconnect handler | Remove `WiFi.disconnect()` from `onStationModeDisconnected`; reconnect via `manageSTA` with explicit mode fixup (no bogus `getMode() & WIFI_STA` gate); throttle duplicate `WiFi.begin` during in-flight auth |

---

## Changelog

| Date (UTC) | Author | Change |
|------------|--------|--------|
| 2026-04-21 | Agent | Initial log: FS override + Json empty-string + stamp design. |
| 2026-04-21 | Agent | Second pass: explicit STA/AP_STA mode, remove disconnect race, hostname format, trim, stamp=2. |
| 2026-04-21 | Agent | Third pass (runtime logs): `onStationModeDisconnected` must **not** call `WiFi.disconnect(true)` — it races auth and produces `STA_DISCONNECTED` / `STA_STOP` cascades (e.g. reason `AUTH_EXPIRE`). Reconnect in `manageSTA` must not rely on `(WiFi.getMode() & WIFI_STA)` (enum is not a bitmask). Added AP→`AP_STA` / off→`STA` mode fixup, `WiFi.begin` throttle after `markStaBeginAttempt()`, and `clearStaBeginAttempt()` on connect / reconfigure / disconnect / `STA_STOP`. |
| 2026-04-21 | Agent | **Cross-reference (not WiFi):** Weight forwarder WebSocket “stall with TCP OK” traced to `WebSocketsClient::setReconnectInterval(0xFFFFFFFE)` blocking the library’s first `tcp->connect()` — fix and evidence recorded in [DECISION-LOG-serial-scale-instance-1.md](DECISION-LOG-serial-scale-instance-1.md) §9.5 #7. |
| 2026-04-22 | Agent | **Outbound WS vs STA:** User logs showed `WiFiClient.cpp:429` `write()` errno **11** (commonly `EAGAIN` on ESP32) and weight-forwarder `ws_tx_FAIL` — treats **outbound WebSocket client** TX path, not STA credential/mode bugs in this doc. Investigation + `AGENTDBG` instrumentation: [DECISION-LOG-serial-scale-instance-1.md](DECISION-LOG-serial-scale-instance-1.md) §9.5 #8. |
| 2026-04-22 | Agent | **Evidence update:** pasted Serial showed `wifi:1` and **`msSinceWfLoop` ~8837** at `sendTXT_false` — STA still associated; failure class is **WebSocketsClient / main-loop ordering**, not `WiFiSettingsService`. Mitigation **#9** in serial-scale log (`main.cpp` + pre-`sendTXT` `_wsClient->loop()`). |
| 2026-04-22 | Agent | **Post-fix persisted:** new COM10 run still hit errno 11 / `sendTXT_false` (`msSinceWfLoop~8479`) and logged local AsyncWebSocket queue overflow (`Too many messages queued`). Added serial-scale log **#10** instrumentation (`main_loop_timing`, `server_ws_broadcast`, `makeBuffer_null`); remains non-STA. |
| 2026-04-22 | Agent | **Root cause isolated in serial path:** `main_loop_timing` showed `serialMs=18680` ms; code inspection confirmed unsigned underflow in serial read budget (`readBudget-- > 0`) reopening the loop under sustained UART traffic. Fixed in serial-scale log **#11**; still non-STA. |
| 2026-04-22 | Agent | **Residual stall after #11:** new run still showed `serialMs=4694` with errno 11 bursts; mitigation escalated to explicit serial RX wall-clock cap (`SERIAL_MAX_READ_MS_PER_LOOP`) in serial-scale log **#12**. This remains non-STA. |
| 2026-04-22 | Agent | **Residual failure after #12:** queue overflow + panic persisted; backtrace resolved to `operator new` from AsyncTCP accept path following AsyncWebSocket queue pressure. Added serial publish throttling (`SERIAL_MIN_PUBLISH_INTERVAL_MS`) as serial-scale log **#13** to reduce server WS enqueue pressure. Still non-STA. |
| 2026-04-22 | Agent | **Residual failure after #13:** even with publish throttling, logs still showed AsyncWebSocket queue overflow and outbound `sendTXT_false`; mitigation moved to WS edge with `serial_hw` broadcast throttle in `WebSocketTxRx` (serial-scale **#14**). Still non-STA. |
| 2026-04-22 | Agent | **Post-#14 evidence:** `serialMs` still reached **~18 s** with `sendTXT_false` **`msSinceWfLoop` ~8400** — outbound forward still ran inside `SerialService::update()` handlers. Mitigation **#15** in [DECISION-LOG-serial-scale-instance-1.md](DECISION-LOG-serial-scale-instance-1.md) §9.5: defer `forwardWeight` to `WeightForwarderService::loop()` after `_wsClient->loop()`. Still non-STA. |
| 2026-04-22 | Agent | **Post-#15 evidence (`terminals/38.txt`):** `serialMs` near zero but **`forwarderMs` multi-second**, low heap, local AsyncWebSocket queue overflow, **`sendTXT_false`** with **`msSinceWfLoop` ~8925**. Mitigation **#16** in serial-scale log: throttle **`internal`** WS fanout after successful forwards, log slow **`_wsClient->loop()`**, widen **`serial_hw`** broadcast interval. Still non-STA. |
| 2026-04-22 | Agent | **User paste (post-#16):** re-confirms **outbound** path — `WiFiClient::write` errno **11**, high **`forwarderMs`**, `wifi=1` at failure — **not** STA credential/association regression. Cross-ref [DECISION-LOG-serial-scale-instance-1.md](DECISION-LOG-serial-scale-instance-1.md) §9.5 **#17** + new `hypothesisId=M` `wf_loop_wall` probe. |
| 2026-04-22 | Agent | **`abort()` on core 1** (post-#16): `addr2line` → **`std::bad_alloc`** in **`AsyncWebServerResponse::addHeader`** (ESPAsyncWebServer) from **AsyncTCP** — **heap fragmentation / low max contiguous block**, not STA. Mitigation **#18** in serial-scale log (`ESP.getMaxAllocHeap()` gate before WS broadcasts, `hypothesisId=P`). |
| 2026-04-23 | Agent | Cross-ref: [DECISION-LOG-serial-scale-instance-1.md](DECISION-LOG-serial-scale-instance-1.md) §9.5 **#21** + **§9.7** — COM10 **STA (`NO_AP_FOUND` / `wifi:0`)** checklist (**LittleFS** `wifiSettings.json` vs factory, 2.4 GHz) vs **outbound WebSocket** / **`WEBSOCKETS_TCP_TIMEOUT`** when STA is up. |
| 2026-04-23 | Agent | **Live Stream / `wifi:1`:** [DECISION-LOG-serial-scale-instance-1.md](DECISION-LOG-serial-scale-instance-1.md) §9.5 **#22** — UI starvation from outbound **`_wsClient->loop()`** during backoff is **not** an STA association regression; keep using §Outbound WebSocket client when **`WiFi.isConnected()`** stays true. |

---

## Outbound WebSocket client (not STA association)

Symptoms such as **`[E][WiFiClient.cpp:429] write(): fail ... errno: 11`** (Arduino core may print `"No more processes"` for errno 11) together with **`[WeightForwarder] ws_tx_FAIL`** point at the **TCP/WebSocket client stack** used to forward weight to a LAN server (buffer / peer read / half-open socket), **not** at the same class of issues as “STA does not join after factory SSID change” in §Problem above.

- **Do not conflate** with `WiFiSettingsService` / `wifiSettings.json` unless Serial also shows `AUTH_EXPIRE`, `STA_DISCONNECTED`, or loss of `WiFi.isConnected()` at the same times.
- **Evidence trail:** [DECISION-LOG-serial-scale-instance-1.md](DECISION-LOG-serial-scale-instance-1.md) §9.3 (errno 11 + `msSinceWfLoop` starvation + `abort`/bad_alloc via AsyncWebServer + AsyncWebSocket queue symptoms) and §9.5 attempts **#8–#18** (2026-04-22).
