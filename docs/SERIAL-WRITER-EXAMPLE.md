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

The ESP32-S3 DevKitC-1 has **two USB-C ports** and the firmware uses them for two completely different purposes:

| Port (silkscreen) | What enumerates on the PC | What flows through it |
|----|----|----|
| `UART` (left) | `Silicon Labs CP210x USB to UART Bridge (COMx)` | **Logs only** — every `Serial.print*` from the firmware (boot banner, `[SerialWriterForwarder]` tags, framework messages). Always 115200 baud. |
| `USB` (right) | Generic `USB Serial Device (COMy)` | **Forwarder data only** — clean weight strings produced by the forwarder when `output_targets=usb_only` or `both`. Baud is whatever Scale Com.exe is set to (the native USB CDC stream is rate-agnostic; the value in the PC tool is just informational). |

This split is implemented by manually instantiating `USBCDC dataUsbCdc` in [src/main.cpp](../src/main.cpp) and wiring it into `SerialWriterService` via `setDataUsbCdc()`. We deliberately keep `ARDUINO_USB_CDC_ON_BOOT=0` so `Serial` stays on UART0 and **no logs leak onto the data port**. Plug both cables in to use both streams simultaneously; the firmware does not care which is plugged first.

1. **Pick output mode**
   - **Native USB CDC (`usb_only` or `both`):** Plug a USB-C cable into the **USB** port (right). Windows enumerates a generic USB Serial Device — open it in **Scale Com.exe** (or any terminal). The stream contains only forwarder lines, no log mixing.
   - **Physical UART tap (`serial1_only` or `both`):** Connect a **USB–TTL adapter** RX to **GPIO17 (TX)**, **GND** to board GND. Open the adapter's COM port on the PC. Never connect 5 V TTL to a 3.3 V-only UART without a level shifter.
2. **Set UART mode to Writer** when using Serial1 (`serial1_only` or `both`): Information tab on Serial Writer (or Serial) → UART mode **Writer**.
3. **Match line terminator:** Forwarder lines use the same `line_terminator` as `/rest/serialWriter` (LF / CRLF / CR / NONE). Configure Scale Com.exe / your parser accordingly.
4. **Logs always need the UART port.** If you are debugging, plug a second USB-C cable into the **UART** port (left) and run `pio device monitor -p COMx -b 115200` against the CP210x COM number. The firmware does not log to the native USB port — that stream is reserved for clean data.

**Verify clean data stream:** Open `pio device monitor -p <USB-port-COM> -b 9600` (baud value here is informational for native CDC) and confirm only weight strings appear, with no `[SerialWriterForwarder]` lines and no boot banner. If you see log lines on this port the USB CDC sink is misconfigured — check that `serialWriterService->setDataUsbCdc(&dataUsbCdc);` is being called in `main.cpp` before `begin()`.

**Worked example (Scale Com.exe, `ST,GS,+ 9760kg`):**

- Source emits `ST,GS,+ 9760kg` on `/ws/serial`.
- Forwarder config: `output_targets=usb_only`, `line_terminator=CRLF` (Scale Com.exe convention).
- `/rest/serialWriter` config: `baud_rate=9600`, `data_bits=8`, `parity=none`, `stop_bits=1` (only matters for the GPIO17 path; the native USB CDC stream is unaffected by these values).
- Both USB-C cables plugged in. Scale Com.exe attached to the USB-port COM at 9600 8N1 CRLF.
- Expected on the **UART** port (115200): `[SerialWriterForwarder][ws-event] type=TEXT ...` and `[SerialWriter] Forwarder sink=USB len=18` log lines.
- Expected on the **USB** port: only `ST,GS,+ 9760kg\r\n` per source frame, nothing else.

**2-minute count verification (optional):** With a fixed-rate source, compare reader line count, forwarder `received_count`, lines seen on the USB-port COM (when `usb_only` or `both`), and lines on a tap of GPIO17 — counts should match for non-empty delivered lines.

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

Root cause: the `esp32s3` environment in [platformio.ini](../platformio.ini) sets `-DARDUINO_USB_CDC_ON_BOOT=0` and `-DARDUINO_USB_MODE=0` *by design*. With CDC off, every `Serial.print*` goes to **UART0**, exposed via the on-board CP210x bridge on the **UART** USB-C port. The native **USB** port is reserved for clean forwarder data (manually instantiated `USBCDC dataUsbCdc` in `main.cpp`) and does **not** carry logs.

| Port (silkscreen) | Connected to | What it does in this firmware |
|----|----|----|
| `UART` (left) | On-board USB-Serial bridge → ESP32-S3 UART0 (GPIO43/44) | **Logs.** All `Serial.print*` output. Always 115200 baud. Required for `pio device monitor`. |
| `USB` (right) | ESP32-S3 native USB (GPIO19/20) | Enumerates as a generic USB Serial Device. The forwarder writes clean weight lines here when `output_targets=usb_only` or `both`. **Do not expect logs on this port** — it is intentionally log-free so PC-side parsers see only data. |

Fix for missing logs:

1. Plug the USB-C cable into the **UART** port (the one labelled UART, on the left of the board).
2. In Windows Device Manager, expand **Ports (COM & LPT)** and confirm the new entry — typically `Silicon Labs CP210x USB to UART Bridge (COMx)`.
3. Run `pio device monitor -p COMx -b 115200` against that COM number. You should see the boot banner ending with `=== System Ready! ===` followed by `[SerialWriterForwarder] Loaded config: ...`.

If you ever do want logs on the native USB port (and to give up the clean-data split), flip `ARDUINO_USB_CDC_ON_BOOT=1` and `ARDUINO_USB_MODE=1` in `platformio.ini` and remove the manual `USBCDC` setup in `main.cpp`. This is a hardware baseline change — discuss before switching.

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
| `Auth failed: sign-in to <baseUrl> did not return token` | `/rest/signIn` returned but no usable `access_token` (e.g. wrong password) | Logs show `[http-auth] failed ... http_code=401` (or other non-200). Verify `auth_username` and that the **stored** password matches the source. Note that the form shows a blank password field by design — see the next section. |
| `Source unreachable: TCP to <baseUrl> failed (...)` | Writer could not open a TCP connection to the reader for `POST /rest/signIn` | **`http_code=-1`** — the request never reached HTTP handling on the reader. This is **not** a SerialReader “sign-in logic” bug unless the reader firmware is not running or not listening on port 80. Typical causes: reader rebooting, reader WiFi drop, wrong IP, AP isolation, or reader out of sockets under load. Check reader UART logs, power, and `Test-NetConnection <reader-ip> -Port 80` from a PC on the same LAN. Fix is usually **environment or reader stability**; the writer only retries. |
| `Connecting to <host>:<port><path>` (persists for >5s) | TCP connect failing | Source unreachable or port closed. From a PC on the same network: `Test-NetConnection <host> -Port 80`. Verify IP address and that the source firmware is running. If logs show `http_code=-1` on sign-in, it is **not** a JSON/body rejection — the TCP path to the source failed before HTTP. |
| `Source closed (auth or path?)` (disconnect happens before any frame is received) | Source rejected the WS handshake | Most often a stale or invalid `access_token`. The forwarder forces re-auth on the next attempt. If it persists, the **path** may be wrong — confirm the source serves `/ws/serial` (it does on the `serialReader` branch). |
| `Source disconnected` | Was connected, then dropped | Network blip, source rebooted, or source-side WS handler closed. The forwarder reconnects after a short delay (5 s when healthy; see backoff below). |
| `WS error: <text>` | Underlying socket error | The text after the colon comes verbatim from the WebSockets library. Treat as a transport-level failure (e.g. TLS handshake on a `wss://` URL — only `ws://` is supported by the parser). |
| `WS field '<json_field>' not found (top-level/payload)` | Connected and receiving frames, but the configured field does not exist | Check the source's WS payload shape. The forwarder accepts both top-level (`{"last_line":"..."}`) and wrapped (`{"type":"payload","payload":{"last_line":"..."}}`). |
| `WS field '<json_field>' is empty` | Field exists but is empty/whitespace | Source produces empty values; pick a different `json_field`. |
| `WS JSON parse error: <code>` | Frame is not valid JSON | Source is sending plain text or a different framing. |

The forwarder **forces re-authentication on every disconnect**, so a wrong-password loop will produce one `[http-auth] failed http_code=401` per reconnect attempt (at the current reconnect interval). That is the fastest way to see the credential rejection from logs alone.

**Reconnect backoff (sign-in failure):** When `auth_username` is set but sign-in does not return a token, the firmware **does not** open a WebSocket to the source (avoids useless `begin()` traffic that could exhaust lwIP sockets). It applies **capped exponential backoff** (delay doubles from 5 s): **`last_signin_http < 0`** (TCP/connect failure, usually `-1`) caps at **15 s** so brief reader outages recover quickly; **HTTP-level failures** (e.g. `401`) cap at **60 s** to avoid hammering wrong credentials. After a successful sign-in path and `CONNECTED`, the delay resets to 5 s. UART logs include `[ws-reconnect-backoff] delay_ms=... last_signin_http=...` when backing off.

**Older builds (pitfall):** If you still see `auth=token-failed` immediately followed by `ws-attempt` / `ws-event` spam without backoff, the device may be running firmware that called WebSocket `begin()` even after failed sign-in — upgrade to a build that includes the forwarder reconnect fix.

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

### Node-RED `signIn` monitoring (troubleshooting)

If the **http request** node shows **`JSON parse error`** and your **function** sees **`No access_token in response: ""`**, Node-RED tried to parse the **HTTP response** as JSON but the body was **empty or not JSON**. That is almost always a **bad or missing POST body** to `/rest/signIn` (reader responds **400** with an empty or non-JSON body), not a bug on the ESP writer (the writer can still receive WebSocket `TEXT` frames while your laptop flow fails independently).

For a **full checklist**, **Admin API v2 deploy** notes (flows + `rev` + ETag), and a **next-steps debugging plan** (correlating NR with writer UART), see **[NODE-RED-SERIAL-READER-DEBUG.md](./NODE-RED-SERIAL-READER-DEBUG.md)**. To re-apply the patched **serialReader WS monitor** tab from this repo: `python scripts/patch_nodered_serialreader_flows.py`.

**Fix the flow (checklist):**

1. **Send JSON credentials on the wire:** Use an **inject** node whose payload is a JSON object, e.g. `{"username":"admin","password":"yourActualPassword"}`, and wire it into **http request**.
2. **http request node:** `POST` → `http://<reader-ip>/rest/signIn`. Under the request body options, send **`msg.payload`** as **JSON** (not "ignore" and not an empty editor). Set header **`Content-Type: application/json`** (the node often adds this when sending JSON — verify).
3. **Temporarily** set **Return** to **a UTF-8 string** (or "a string") instead of "a parsed JSON object". Deploy and inject: the **debug** output should show raw body and status. Expect a long JSON string containing `access_token` on **200**. If you see **empty** body, open the node’s status / catch node and log **status code** (400 = malformed request; 401 = wrong password / security off mismatch).
4. Once **200** and body look like JSON, switch **Return** back to **JSON** and pass `msg.payload.access_token` into your WebSocket URL builder.
5. **Cross-check from the same laptop** with PowerShell (same as manual WS section above): `Invoke-RestMethod` to `http://<reader>/rest/signIn` with `-ContentType application/json` and a JSON body. If PowerShell works but Node-RED does not, the bug is strictly in the NR flow wiring.

**Optional:** Add a **catch** node wired to **debug** to log failed `http request` details (status code, response text).

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
[SerialWriterForwarder][ws-reconnect-backoff] delay_ms=10000 last_signin_http=401

# Auth attempt + TCP failure (source down / no route) — http_code=-1, extended log hint
[SerialWriterForwarder][http-auth] failed url=http://10.45.71.5/rest/signIn http_code=-1 (TCP/connect failed; source unreachable?)
[SerialWriterForwarder][ws-attempt] auth=token-failed user=admin (sign-in did not return access_token)
[SerialWriterForwarder][ws-reconnect-backoff] delay_ms=10000 last_signin_http=-1
# next TCP failures cap at 15 s (transport backoff)
[SerialWriterForwarder][ws-reconnect-backoff] delay_ms=15000 last_signin_http=-1

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

### Reading a real disconnect + TCP failure

When the WebSocket drops (`[ws-event] type=DISCONNECTED`), behaviour depends on whether a **data frame was already received** on that session (firmware **0.7.5+**): **mid-session** drops keep the JWT (`[ws-token] kept...`) so the next attempt can open the outbound WebSocket **without** `POST /rest/signIn` first. **Handshake-only** failures still clear the token so sign-in runs again.

If sign-in fails with **`http_code=-1`** and the log hint **`(TCP/connect failed; source unreachable?)`**, the ESP32 never established TCP to the reader on port 80. That is a **Layer 2/3 problem** (reader down, wrong IP, different subnet, AP isolation, transient WiFi/routing loss), not “reconnect is disabled.” The UI / REST field `last_error` is set to **`Source unreachable: TCP to http://<host>...`** in that case (see `SerialWriterForwarderService::fetchHttpAuthToken` / `initWsClient`).

You will then see **`[ws-reconnect] reason=no-client`** on later ticks. That is **expected** when username/password auth is configured but sign-in did not return a token: `initWsClient()` **returns before** allocating `WebSocketsClient`, so there is no client until TCP to the reader succeeds again. It does **not** mean the reconnect loop stopped.

**Bench check:** While the writer logs repeated `http_code=-1`, verify from another machine on the same LAN that the reader answers HTTP (e.g. `POST /rest/signIn` or a simple TCP connect to port 80). If the reader is reachable elsewhere but the writer never recovers until reboot, capture that scenario (WiFi events, reader IP stability) for a possible firmware or network-stack follow-up.

### Reader reboot vs writer reboot

`http_code=-1` is evaluated on the **serialWriter** device: its `HTTPClient` cannot complete TCP to the configured reader IP (e.g. `http://10.45.71.5/rest/signIn`).

- **Rebooting the serialReader** only resets the reader’s firmware and listeners. It does **not** reset the writer’s WiFi association, lwIP socket tables, or ARP cache. If the writer’s UART log **unchanged** after a reader power-cycle (still alternating `http-auth` failed `-1` with `ws-reconnect` / backoff), treat the incident as **writer reachability or writer network stack state**, not “the reader process was hung.”
- **Rebooting the serialWriter** reinitializes WiFi and the whole TCP stack, which is why `http-auth ok` and `ws-event CONNECTED` often return only after a writer reset even when the reader was healthy all along.

Use that split when triaging: confirm the reader from a **third** host on the same subnet while the writer is stuck; if the third host succeeds, focus on the writer (captive portal / `WiFi Got IP` / reason codes, STA vs AP, cable AP firmware).

### Recent firmware reconnect path (maintainers)

Reconnect behaviour on ESP32 **changed in meaning** around commits **`02ec858`** / **`cf01bb1`** on `SerialWriterForwarderService.cpp` (auth-first refactor and transport-specific backoff), compared to earlier builds such as **`4113231`**:

| Area | Older pattern (pre–auth-first) | Current pattern |
|------|-------------------------------|-----------------|
| Order of operations | Allocate `WebSocketsClient`, then fetch token; on token failure the code could still call `begin(host, port, path)` without `access_token` (TCP + WS attempt anyway). | Parse URL, **`POST /rest/signIn` first**; if sign-in fails, **return before** `new WebSocketsClient()` — no `begin()` until a token exists. |
| `reason=no-client` | Same label, but backoff for the no-client path was a fixed 5s interval in the older `checkWsConnection` logic. | Backoff grows with failures (transport `-1` capped shorter than HTTP errors like `401`); see `ws-reconnect-backoff` and `last_signin_http` in logs. |
| Heartbeat | **`enableHeartbeat(15000, 3000, 2)`** on the outbound client (e.g. `4113231`). | Removed in **`02ec858`**; **restored from firmware 0.7.5** on the outbound client. |
| Teardown | Client replaced with `delete` (ordering varied). | **`disconnect()` then `delete`** when replacing the outbound WS client. |

**From firmware 0.7.5** (`SerialWriterForwarderService.cpp`): after a **mid-session** WebSocket drop (`DISCONNECTED` with data already received), the JWT is **kept** so the next reconnect opens the WebSocket **without** `POST /rest/signIn` first (log: `[ws-token] kept after mid-session disconnect`). Handshake-only failures still clear the token. After **six** consecutive transport-level sign-in failures (`http_code=-1`), the writer calls **`WiFi.reconnect()`** once (log: `[wifi-recover]`) to try to clear a wedged STA path.

So “it used to reconnect after a few retries” on older flashes may reflect **different TCP traffic** (HTTP-only retries vs mixed HTTP + unconditional WS `begin`) and different timers — not proof that the reader was at fault. The **underlying** failure in your logs is still **writer TCP `-1`** until something resets the writer’s network path.

### Optional recovery experiments (bisect and mitigations)

If you can reproduce **reader reachable from a PC** while the **writer stays on `http_code=-1`**, capture WiFi reason codes and consider:

1. **Bisect on hardware:** Flash the parent of `02ec858`, then `02ec858`, then `cf01bb1` with the same topology to see which change correlates with “stuck until writer reboot.”
2. **WiFi recovery:** Firmware **0.7.5+** triggers `WiFi.reconnect()` after six consecutive TCP sign-in failures (see `[wifi-recover]`). If problems persist on dual AP+STA builds, capture whether the soft-AP is active and open an issue with WiFi mode and reason codes.
3. **Reconnect policy:** Implemented in **0.7.5+** — JWT kept after mid-session disconnect so WS is retried before re-auth (see `[ws-token]`). If your reader invalidates tokens on every socket close, you may need server-side policy or a follow-up code change.
4. **Heartbeat:** Restored in **0.7.5+** for the outbound client; compare idle stability vs older builds if needed.

### Periodic disconnects (~8 s) and `Client connected` in the same log

**What the samples show**

- **Reconnect without `http-auth`:** Lines like `[ws-attempt] auth=token-ok` after `[ws-token] kept...` mean the writer is **not** hammering `POST /rest/signIn` on every drop. That rules out “writer overloads reader auth” **during** steady streaming; sign-in only returns when the token was cleared (e.g. handshake failure) or on cold start.
- **`DISCONNECTED` with `len=15` or `len=22`:** The WebSockets library passes a **close payload** (often a 2-byte [RFC 6455](https://datatracker.ietf.org/doc/html/rfc6455#section-5.5.1) close code in network byte order, plus optional UTF-8 reason, or a short status string). Today’s log line only prints `len=`; decode requires a **temporary** `Serial.printf` of the first bytes in the `WStype_DISCONNECTED` branch (or a logic analyzer on TLS-off lab traffic). Typical codes: `1000` normal closure, `1001` going away, `1006` abnormal (no close frame), `1008` policy violation, `1011` server error.
- **Roughly 8 s between drops** while only **PONG** / occasional small **TEXT** (`len=33`, `ws-field-missing`) appear is consistent with an **idle or single-client policy on the reader** (or a proxy): many stacks close outbound streams if **no application JSON** is sent for N seconds when the scale line is unchanged. That is **not** a UART buffer length issue on the writer — your `TEXT len=339` frames are delivered and parsed when present.
- **`[WS] Client connected: 1`** comes from [`WebSocketTxRx.h`](../lib/framework/WebSocketTxRx.h) on the **writer** device: a **browser or tool opened a WebSocket to the writer** (e.g. `/ws/serialWriterForwarder` or another secured UI socket). It is **not** the reader accepting a second forwarder. It can still matter: **two tabs** or **diagnostics** on the writer add load and JSON broadcasts; as an experiment, **close every UI tab** on the writer and see whether reader-side disconnect cadence changes.

**When it degrades into `http_code=-1` again**

That is the same **writer TCP to reader** failure class as before: after a bad handshake the token is cleared, sign-in runs, and transport fails. It is **orthogonal** to the periodic close code unless the reader crashes or the AP kicks the writer.

**Debugging checklist (recommended order)**

Full ordered steps, log table, and **serialReader** branch handoff: **[SERIAL-FORWARDER-DISCONNECT-RUNBOOK.md](./SERIAL-FORWARDER-DISCONNECT-RUNBOOK.md)**.

1. Capture **close payload bytes** for every `DISCONNECTED` — from firmware **0.7.6+** the writer logs **`[ws-close]`** (`rfc6455_code` / `no_close_payload` / capped `reason_hex`); map RFC 6455 codes to reader policy or network resets.
2. **Reader UART** (or remote logging) at the same timestamps: does the reader log closing `/ws/serial` (reason, heap, `LIVE_LOCK`, `AsyncWebSocket` client limit)?
3. **A/B:** No browser open on the **writer** vs one tab on forwarder status — compare disconnect interval.
4. **Reader firmware:** Inspect max concurrent WS clients, idle timeout, and whether `/ws/serial` only pushes on **line change** (explains long gaps with only PONG from the writer’s heartbeat).
5. **Network:** Same-LAN PC with `wscat` or browser `WebSocket` to `ws://<reader>/ws/serial?...` left idle — if the PC sees the same ~8 s close, the cause is on the **reader or AP**, not the ESP32 forwarder alone.
6. If `http_code=-1` returns, use the **reader vs writer reboot** and **third-host ping** steps above; confirm whether **`[wifi-recover]`** fired (six TCP sign-in failures).

**Do we need more testing to conclude?** Yes, for the **exact root** of the periodic close: the close code plus reader logs (or reader source for idle timeout / max clients) are required. The writer logs alone strongly suggest **server-side or path idle policy**, not JWT spam or undersized line buffers.

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
- [ ] `usb_only` writes clean lines to the **USB** port (no log mixing); UART port shows only `[SerialWriter]` log lines
- [ ] Forwarder `output_targets`: `serial1_only` / `both` behave as documented (mirror counts match over 2 minutes when using `both`)
- [ ] Pulling the **USB** cable mid-stream logs `USB CDC sink not configured; line dropped` once on UART, no crash; re-plugging resumes data without firmware restart
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
