# Serial Writer Integration

## Overview

The Serial Writer integration sends data **to** the Serial1 TX pin (GPIO17). It is the direct reverse of the Serial Monitor (`serialReader` branch), which reads from RX (GPIO18).

Two new services are added:

- **SerialWriterService** — receives data via REST, WebSocket, MQTT, or BLE and writes each line to Serial1.
- **SerialWriterForwarderService** — pulls data from a remote HTTP or WebSocket source and delivers each extracted line according to **`output_targets`** (see below).

Forwarder routing (`output_targets` on `GET/POST /rest/serialWriterForwarder`, persisted in `/config/serialWriterForwarderConfig.json`):

| Value | Meaning |
|-------|---------|
| `usb_only` (default) | Each line is written to **USB CDC `Serial` only** (PC opens the ESP USB virtual COM port, when CDC is available). |
| `serial1_only` | Each line is written to **Serial1 TX (GPIO17)** only — same physical path as manual `pending_line` sends. |
| `both` | **Mirror**: same bytes and line terminator are written to **USB CDC and Serial1 TX**. |

Manual sends via `/rest/serialWriter` `pending_line` always use **Serial1 TX** (they do not follow `output_targets`).

**UART mode:** Serial1 is only driven when UART mode is **`writer`**. In `live` or `diagnostics`, SerialWriter suspends Serial1; if `output_targets` includes Serial1, the firmware **skips** the UART leg and logs a short reason on the firmware serial console (no spam on `last_error`).

---

## Architecture

```
SerialWriterService : StatefulService<SerialWriterState>
├── HttpEndpoint      GET/POST /rest/serialWriter
├── WebSocketTxRx     /ws/serialWriter
├── FSPersistence     /config/serialWriterConfig.json
├── MqttPubSub        pub: weighsoft/serialwriter/<id>/status
│                     sub: weighsoft/serialwriter/<id>/send
└── BlePubSub         UUID 12350000 (R/W/Notify)

SerialWriterForwarderService : StatefulService<SerialWriterForwarderState>
├── HttpEndpoint      GET/POST /rest/serialWriterForwarder
├── WebSocketTxRx     /ws/serialWriterForwarder
└── FSPersistence     /config/serialWriterForwarderConfig.json
```

`SerialWriterForwarderService` is the reverse of `WeightForwarderService`:

| | WeightForwarderService | SerialWriterForwarderService |
|--|---|---|
| Source | SerialService (local Serial1 RX) | Remote HTTP endpoint or WebSocket |
| Destination | Remote HTTP / WS / MQTT / BLE | SerialWriterService: USB CDC and/or Serial1 per `output_targets` |
| Trigger | `serial_hw` state update | Timer (HTTP) or WS message callback |

---

## Hardware

| Signal | GPIO | Notes |
|--------|------|-------|
| Serial1 TX | GPIO17 | Data output to external device |
| Serial1 RX | GPIO18 | Not used in writer mode (shared with SerialService) |

Both SerialService and SerialWriterService use the same UART hardware (Serial1). Only one can be active at a time — coordinated by UartModeService.

---

## UART Mode

Three modes are available, selectable via `POST /rest/uartMode`:

| Mode | Value | Description |
|------|-------|-------------|
| `live` | 0 | SerialService reads from RX (default) |
| `writer` | 1 | SerialWriterService writes to TX |
| `diagnostics` | 2 | DiagnosticsService active, both others suspended |

The UART Mode Switcher in the UI (visible on the Information tab of Serial and Serial Writer pages) lets you switch modes without using the API directly.

---

## REST API

### Serial Writer State — `GET/POST /rest/serialWriter`

**GET response:**
```json
{
  "baud_rate": 9600,
  "data_bits": 8,
  "stop_bits": 1,
  "parity": 0,
  "line_terminator": "LF",
  "pending_line": "",
  "last_sent_line": "Hello world",
  "last_sent_ms": 123456,
  "sent_count": 42
}
```

**POST — send a line:**
```json
{"pending_line": "your text here"}
```

**POST — update config:**
```json
{"baud_rate": 115200, "line_terminator": "CRLF"}
```

Config fields (`baud_rate`, `data_bits`, `stop_bits`, `parity`, `line_terminator`) are persisted to flash. Runtime fields (`last_sent_line`, `sent_count`, `last_sent_ms`) are read-only.

`pending_line` is the write trigger — set it to any non-empty string and the firmware immediately writes it to Serial1 with the configured line terminator, then clears it.

### Forwarder Config — `GET/POST /rest/serialWriterForwarder`

```json
{
  "enabled": true,
  "protocol": 0,
  "source_url": "http://192.168.1.100/rest/serial",
  "json_field": "last_line",
  "poll_interval_ms": 1000,
  "output_targets": "both",
  "auth_username": "",
  "auth_password": "",
  "connected": true,
  "last_received_line": "ST,GS,  1.23 kg",
  "last_received_ms": 123456,
  "received_count": 100,
  "last_error": ""
}
```

`protocol`: `0` = HTTP poll, `1` = WebSocket subscribe.

`output_targets`: `usb_only` | `serial1_only` | `both` (strings; integers `0`/`1`/`2` are also accepted on POST for compatibility).

### PC COM, Scale Com.exe, and GPIO17 (Phase 3)

Use this checklist so you can reproduce setup in under ~10 minutes.

1. **Choose how the PC reads data**
   - **Onboard USB CDC (`usb_only` or `both`):** Install USB drivers if needed; in Windows Device Manager, identify the **USB Serial Device** (or vendor-specific name) for the ESP. Open that COM port in PuTTY, your own tool, or **Scale Com.exe** at the baud you use for that tap (often 115200 for a monitor-style log; the **device on GPIO17** still uses **`serialWriter` baud / parity / stop** from `/rest/serialWriter`).
   - **Physical UART tap (`serial1_only` or `both` on a second PC):** Connect a **USB–TTL adapter** RX to **GPIO17 (TX)**, **GND** to board GND. Open the adapter’s COM port on the PC. Never connect 5 V TTL to a 3.3 V-only UART without a level shifter.
2. **Set UART mode to Writer** when using Serial1 (`serial1_only` or `both`): Information tab on Serial Writer (or Serial) → UART mode **Writer**.
3. **Line terminators:** Forwarder lines use the same `line_terminator` as `/rest/serialWriter` (LF / CRLF / CR / NONE). Match Scale Com.exe or parser settings accordingly.
4. **`ARDUINO_USB_CDC_ON_BOOT`:** The `esp32s3` environment in `platformio.ini` may set `ARDUINO_USB_CDC_ON_BOOT=0`. That changes whether the native USB port enumerates as a CDC COM for application use. If **no USB COM** appears for mirroring, use **`serial1_only`** plus a **USB–TTL on GPIO17**, or adjust the firmware build flags after agreeing a hardware baseline (see [INTEGRATION-WORKFLOW.md](INTEGRATION-WORKFLOW.md)).

**2-minute verification (optional):** With a fixed-rate source, compare reader line count, forwarder `received_count`, lines seen on the ESP USB COM (when `usb_only` or `both`), and lines on a tap of GPIO17 — counts should match for non-empty delivered lines.

---

## WebSocket

| Endpoint | Data |
|----------|------|
| `/ws/serialWriter` | Live `SerialWriterState` — notifies on each write and config change |
| `/ws/serialWriterForwarder` | `SerialWriterForwarderState` — connection and received line status |

---

## MQTT

| Topic | Direction | Format |
|-------|-----------|--------|
| `weighsoft/serialwriter/<id>/status` | Publish | Full `SerialWriterState` JSON |
| `weighsoft/serialwriter/<id>/send` | Subscribe | `{"pending_line":"text"}` |

---

## BLE Interface

BLE must be enabled in BLE Settings.

- **Service UUID**: `12350000-e8f2-537e-4f6c-d104768a1235`
- **Characteristic UUID**: `12350001-e8f2-537e-4f6c-d104768a1235` (Read / Write / Notify)

Write JSON to trigger a serial write or update config:

```json
{"pending_line": "Hello world"}
```

```json
{"baud_rate": 115200, "line_terminator": "CRLF"}
```

Full field reference is available on the BLE tab in the Serial Writer UI.

---

## Serial Writer Forwarder

The forwarder pulls data from a remote source and feeds each line to SerialWriterService according to **`output_targets`** (USB CDC, Serial1 TX, or both).

### HTTP Polling Mode

Every `poll_interval_ms` milliseconds, the forwarder GETs `source_url`, parses the JSON response, extracts `json_field`, and writes the line to the configured sink(s) if non-empty.

Example: to mirror a remote Serial Monitor in real-time:
```json
{
  "enabled": true,
  "protocol": 0,
  "source_url": "http://192.168.1.100/rest/serial",
  "json_field": "last_line",
  "poll_interval_ms": 500,
  "output_targets": "usb_only"
}
```

### WebSocket Subscribe Mode

Connects to `source_url` as a WebSocket client. Each incoming JSON message is parsed and `json_field` is extracted and written to the configured sink(s).

Supported message shapes for `json_field` extraction:

- Top-level field: `{"last_line":"..."}`
- Framework payload wrapper: `{"type":"payload","payload":{"last_line":"..."}}`

The configured field value is trimmed of leading/trailing ASCII whitespace before delivery. Whitespace-only values are treated as empty (see `last_error` below).

#### `last_error` and serial log tags (Phase 2)

When a WebSocket text frame cannot be turned into a forwarded line, `last_error` on `/rest/serialWriterForwarder` is set and the serial console emits **one line per failure category** with a stable tag:

| Situation | `last_error` (WS) | Serial log tag |
|-----------|-------------------|----------------|
| JSON parse failed | `WS JSON parse error: <ArduinoJson code>` | `[SerialWriterForwarder][ws-parse-error]` |
| Valid JSON, field absent at top-level and under `payload` | `WS field '<json_field>' not found (top-level/payload)` | `[SerialWriterForwarder][ws-field-missing]` |
| Field present but empty or whitespace-only after trim | `WS field '<json_field>' is empty` | `[SerialWriterForwarder][ws-field-empty]` |
| Successful line (HTTP or WS) | cleared (`""`) | `[SerialWriterForwarder][delivered]` (includes `field=`, `path=` when applicable, `len=`) |

HTTP polling uses the same mapping logic with tags `[http-parse-error]`, `[http-field-missing]`, `[http-field-empty]`.

While the WebSocket transport is connected, parse and mapping failures keep `connected=true` so the UI still shows a live socket with a visible `last_error` (transport disconnects still set `connected=false`).

#### Phase 2 verification matrix (manual)

Inject or replay these three WS text payloads against the configured `json_field` (default `last_line`) and confirm `received_count`, `last_received_line`, and `last_error`:

| # | Payload | Expected |
|---|---------|----------|
| 1 | `{"type":"payload","payload":{"last_line":"OK1"}}` | `received_count` increases; `last_received_line` is `OK1`; `last_error` cleared; log `[delivered]` with `path=payload` |
| 2 | `{"last_line":"OK2"}` | Same with top-level path |
| 3 | `{not valid json` or non-JSON text | `last_error` starts with `WS JSON parse error:` within one message; log `[ws-parse-error]`; device stays up |

#### Automated tests (mapping only)

Shared extraction helpers live in [SerialWriterForwarderJsonMapping.h](../src/examples/serialwriter/SerialWriterForwarderJsonMapping.h). Unit tests cover flat, wrapped, missing field, empty, whitespace-only, and invalid JSON.

Build tests only (no upload / no on-device run):

```text
pio test -c platformio_test.ini -e esp32s3_test_mapping --without-uploading --without-testing
```

Example: subscribe to a remote device's serial WebSocket:
```json
{
  "enabled": true,
  "protocol": 1,
  "source_url": "ws://192.168.1.100/ws/serial",
  "json_field": "last_line",
  "output_targets": "both"
}
```

### Authentication

If the source device requires login, provide `auth_username` and `auth_password`. The forwarder signs in to `/rest/signIn` on the source device and appends the token as a Bearer header (HTTP) or `?access_token=` query parameter (WebSocket). Re-authentication is triggered automatically on 401 responses.

The default factory credentials baked into both the source and writer firmwares come from `factory_settings.ini` (`FACTORY_ADMIN_USERNAME=admin`, `FACTORY_ADMIN_PASSWORD=admin`). On a deployed device these may have been changed via Security settings — use whatever the source device currently accepts on `/rest/signIn`.

For security, `GET /rest/serialWriterForwarder` deliberately omits `auth_password`. The response includes `auth_password_set: true|false` so the UI can show whether a password is stored without leaking it. On `POST`, an empty `auth_password` is treated as **"keep existing"** so re-saving the form does not wipe the stored credential. To explicitly remove the stored password, send `{"auth_password_clear": true}` (the UI exposes a "Clear stored password" button when one is stored).

---

## Troubleshooting

This section is the runbook for the two failures most commonly hit during forwarder bring-up on the `serialWriter` branch.

### "I see no logs on COM3 / Nothing further"

Symptom: `pio device monitor -p COMx -b 115200` connects but the device prints nothing — not even the boot banner.

Root cause: the `esp32s3` environment in [platformio.ini](../platformio.ini) sets `-DARDUINO_USB_CDC_ON_BOOT=0` and `-DARDUINO_USB_MODE=0`. With CDC off, all `Serial.print*` calls go to **UART0**, not to native USB CDC. On the Espressif **ESP32-S3 DevKitC-1** there are two USB-C ports:

| Port (silkscreen) | Connected to | Behaviour with `CDC_ON_BOOT=0` |
|----|----|----|
| `UART` (left) | On-board USB-Serial bridge → ESP32-S3 UART0 (GPIO43/44) | **Required.** Logs appear here as a normal COM port. |
| `USB` (right) | ESP32-S3 native USB (GPIO19/20) | No COM port enumerates because CDC is disabled at boot. |

Fix:

1. Plug the USB-C cable into the **UART** port (the one labelled UART, on the left of the board).
2. In Windows Device Manager, expand **Ports (COM & LPT)** and confirm the new entry — typically `Silicon Labs CP210x USB to UART Bridge (COMx)`.
3. Run `pio device monitor -p COMx -b 115200` against that COM number. You should see the boot banner ending with `=== System Ready! ===` followed by `[SerialWriterForwarder] Loaded config: ...`.

If you specifically need logs on the native USB port, change `ARDUINO_USB_CDC_ON_BOOT=1` and `ARDUINO_USB_MODE=1` in `platformio.ini` (note: this is a hardware baseline change — discuss before switching).

### Source Connection stays Disconnected

Once logs are visible, every WS attempt produces structured tags. Filter the monitor output for `SerialWriterForwarder` and look for these lines:

```text
[SerialWriterForwarder][ws-attempt] n=1 url='ws://10.45.71.5/ws/serial' host=10.45.71.5 port=80 path=/ws/serial wifi_ip=192.168.3.55
[SerialWriterForwarder][ws-attempt] auth=token-ok user=admin token_len=183
[SerialWriterForwarder][ws-event] type=CONNECTED t=12345 len=0
[SerialWriterForwarder][ws-event] type=TEXT t=12678 len=82
[SerialWriterForwarder][delivered] field=last_line path=payload len=18
```

If you do not see the sequence above, match the actual `last_error` shown in the UI Status page against this matrix:

| `last_error` (UI) | Likely cause | What to check |
|----|----|----|
| `No WiFi` | WiFi STA on the writer is not connected | Confirm WiFi Connection page shows an IP. Reconnect to the AP. Logs will show `[ws-attempt] skipped reason=wifi-down` and `[ws-reconnect] reason=wifi-down`. |
| `Source URL not configured` | `source_url` is empty | Set the URL on the Configuration tab. |
| `Auth failed: sign-in to <baseUrl> did not return token` | Source device rejected `/rest/signIn` | Logs show `[http-auth] failed url=... http_code=401`. Verify `auth_username` and that the **stored** password matches the source. Note that the form shows a blank password field by design — see the next section. |
| `Connecting to <host>:<port><path>` (persists for >5s) | TCP connect failing | Source unreachable. From a PC on the same network: `Test-NetConnection <host> -Port 80`. Verify IP address and that the source firmware is running. |
| `Source closed (auth or path?)` (disconnect happens before any frame is received) | Source rejected the WS handshake | Most often a stale or invalid `access_token`. The forwarder forces re-auth on the next attempt. If it persists, the **path** may be wrong — confirm the source serves `/ws/serial` (it does on the `serialReader` branch). |
| `Source disconnected` | Was connected, then dropped | Network blip, source rebooted, or source-side WS handler closed. The forwarder will reconnect within 5 s. |
| `WS error: <text>` | Underlying socket error | The text after the colon comes verbatim from the WebSockets library. Treat as a transport-level failure (e.g. TLS handshake on a `wss://` URL — only `ws://` is supported by the parser). |
| `WS field '<json_field>' not found (top-level/payload)` | Connected and receiving frames, but the configured field does not exist | Check the source's WS payload shape. The forwarder accepts both top-level (`{"last_line":"..."}`) and wrapped (`{"type":"payload","payload":{"last_line":"..."}}`). |
| `WS field '<json_field>' is empty` | Field exists but is empty/whitespace | Source produces empty values; pick a different `json_field`. |
| `WS JSON parse error: <code>` | Frame is not valid JSON | Source is sending plain text or a different framing. |

The forwarder **forces re-authentication on every disconnect**, so a wrong-password loop will produce one `[http-auth] failed http_code=401` per reconnect attempt (every ~5 s). That is the fastest way to see the credential rejection from logs alone.

### Password field appears blank after reload

This is intentional. `GET /rest/serialWriterForwarder` omits `auth_password` so the secret never leaves the device. The Configuration page surfaces this with a chip:

- `password stored` (green outline) — a non-empty password is on flash.
- `not set` (grey outline) — no password is stored.

Behaviour on Save:

- Type a new password to **replace** the stored one.
- Leave the field **blank** to **keep** the stored password (this is the bug-prevention introduced in this branch — a blank field on Save no longer wipes the stored credential).
- Click **Clear stored password** to send `{"auth_password_clear": true}` and remove it.

This means re-saving the form to change e.g. the `source_url` no longer accidentally breaks authentication.

### Manual WS sanity check (without the forwarder)

When the forwarder UI says Disconnected and the cause is unclear, take the source out of the picture by connecting to its `/ws/serial` directly from a PC.

1. Fetch a JWT from PowerShell:

   ```powershell
   $r = Invoke-RestMethod -Method Post `
     -Uri http://10.45.71.5/rest/signIn `
     -Body '{"username":"admin","password":"admin"}' `
     -ContentType application/json
   $token = $r.access_token
   $token
   ```

   A successful response returns `access_token` (a long JWT). HTTP `401` here means the source credentials you are configuring on the writer are wrong — fix them on the source first.

2. Open the WebSocket directly. Either:

   - Browser DevTools console:

     ```js
     const ws = new WebSocket('ws://10.45.71.5/ws/serial?access_token=PASTE_TOKEN_HERE');
     ws.onmessage = (e) => console.log(e.data);
     ws.onclose = (e) => console.log('closed', e.code, e.reason);
     ws.onerror = (e) => console.log('error', e);
     ```

   - Or [`wscat`](https://github.com/websockets/wscat):

     ```bash
     npx wscat -c "ws://10.45.71.5/ws/serial?access_token=PASTE_TOKEN_HERE"
     ```

3. Expected behaviour: connection stays open and frames stream as the source emits them. If this works but the forwarder still fails, the issue is on the **writer** (credentials, WiFi, URL parsing) — re-read the matrix above. If this fails too, the issue is on the **source**.

### Expected log lines per state

The stable tag pattern is `[SerialWriterForwarder][<category>]`. Grep for it on the writer's UART log to map runtime state to a single category in one pass.

```text
# WiFi down on writer
[SerialWriterForwarder][ws-attempt] skipped reason=wifi-down
[SerialWriterForwarder][ws-reconnect] reason=wifi-down

# Source URL not configured
[SerialWriterForwarder][ws-attempt] skipped reason=no-source-url

# Auth attempt + ok
[SerialWriterForwarder][http-auth] ok url=http://10.45.71.5/rest/signIn token_len=183
[SerialWriterForwarder][ws-attempt] auth=token-ok user=admin token_len=183

# Auth attempt + failure (wrong password / source returned non-200)
[SerialWriterForwarder][http-auth] failed url=http://10.45.71.5/rest/signIn http_code=401
[SerialWriterForwarder][ws-attempt] auth=token-failed user=admin (sign-in did not return access_token)

# Successful WS lifecycle with one delivered frame
[SerialWriterForwarder][ws-event] type=CONNECTED t=12345 len=0
[SerialWriterForwarder][ws-event] type=TEXT t=12678 len=82
[SerialWriterForwarder][delivered] field=last_line path=payload len=18

# Disconnect before any frame (typical for handshake rejection)
[SerialWriterForwarder][ws-event] type=DISCONNECTED t=12390 len=0
# -> last_error="Source closed (auth or path?)"

# Disconnect after a frame had been received
[SerialWriterForwarder][ws-event] type=CONNECTED t=12345 len=0
[SerialWriterForwarder][ws-event] type=TEXT t=12678 len=82
[SerialWriterForwarder][ws-event] type=DISCONNECTED t=92500 len=0
# -> last_error="Source disconnected"

# Reconnect tick
[SerialWriterForwarder][ws-reconnect] reason=not-connected
```

---

## File Structure

### Backend (`src/examples/serialwriter/`)

| File | Purpose |
|------|---------|
| `SerialWriterState.h` | State struct: config + pending/sent line + counters |
| `SerialWriterForwarderState.h` | State struct: forwarder config + runtime status |
| `SerialWriterService.h/.cpp` | Writer service: manual sends to Serial1; forwarder sends per sink bitmask (USB / Serial1) |
| `SerialWriterForwarderJsonMapping.h` | Shared HTTP/WS JSON extraction: top-level and `payload.<json_field>`, trim, outcomes |
| `SerialWriterForwarderService.h/.cpp` | Forwarder: pulls from remote HTTP/WS; uses mapping helpers; structured `last_error` and logs |
| `platformio_test.ini` | Minimal PlatformIO config to compile mapping unit tests without full firmware `lib_deps` |
| `test/test_forwarder_json_mapping/` | Unity tests for `SerialWriterForwarderJsonMapping` |

### Modified Backend Files

| File | Change |
|------|--------|
| `src/UartMode.h` | Added `WRITER` enum value (2) |
| `src/UartModeService.h/.cpp` | Added `setSerialWriterService()` and `writer` mode handling |
| `src/main.cpp` | Instantiates and links new services; BLE callback registered |

### Frontend (`interface/src/examples/serialwriter/`)

| File | Purpose |
|------|---------|
| `SerialWriter.tsx` | Tab router |
| `SerialWriterInfo.tsx` | Hardware info + UART mode switcher |
| `SerialWriterConfig.tsx` | Baud rate, data bits, line terminator config |
| `SerialWriterSend.tsx` | Text input + Send button + live status |
| `SerialWriterBle.tsx` | BLE UUIDs and usage |

### Frontend (`interface/src/examples/serialwriterforwarder/`)

| File | Purpose |
|------|---------|
| `SerialWriterForwarder.tsx` | Tab router |
| `SerialWriterForwarderConfig.tsx` | Protocol, URL, JSON field, poll interval, auth |
| `SerialWriterForwarderStatus.tsx` | Live connection status |

---

## Verification Checklist

- [ ] UART Mode switches between `live`, `writer`, and `diagnostics` correctly
- [ ] POST `{"pending_line":"test"}` to `/rest/serialWriter` — verify TX on GPIO17
- [ ] `sent_count` increments on each successful write
- [ ] Forwarder `output_targets`: `usb_only` / `serial1_only` / `both` behave as documented (mirror counts match over 2 minutes when using `both`)
- [ ] Config changes persist across reboot (`baud_rate`, `line_terminator`)
- [ ] WebSocket `/ws/serialWriter` notifies on each send (check `sent_count` / `last_sent_line`)
- [ ] MQTT send topic delivers line to serial
- [ ] BLE write `{"pending_line":"ble test"}` triggers serial write
- [ ] Forwarder HTTP mode: configure source URL, enable → verify `last_received_line` in UI
- [ ] Forwarder WS mode: connect to WS source with both flat and wrapped payloads → verify streaming
- [ ] Forwarder auth: configure credentials → verify 401 retry and successful auth
- [ ] Source WS Disconnected reproduces correct `last_error` categories (`No WiFi`, `Auth failed: ...`, `Source closed (auth or path?)`, `Source disconnected`) per the Troubleshooting matrix
- [ ] Save with blank password preserves stored credential (UI chip stays `password stored`; subsequent reconnect logs `auth=token-ok`)
- [ ] SerialService (live mode) still functions after switching back from writer mode
- [ ] Firmware builds without errors: `platformio run -e esp32s3`
- [ ] Forwarder JSON mapping tests build: `pio test -c platformio_test.ini -e esp32s3_test_mapping --without-uploading --without-testing`
- [ ] Frontend builds without errors: `cd interface && npm run build`
