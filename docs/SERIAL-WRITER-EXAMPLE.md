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
- [ ] SerialService (live mode) still functions after switching back from writer mode
- [ ] Firmware builds without errors: `platformio run -e esp32s3`
- [ ] Forwarder JSON mapping tests build: `pio test -c platformio_test.ini -e esp32s3_test_mapping --without-uploading --without-testing`
- [ ] Frontend builds without errors: `cd interface && npm run build`
