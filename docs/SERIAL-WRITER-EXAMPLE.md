# Serial Writer Integration

## Overview

The Serial Writer integration sends data **to** the Serial1 TX pin (GPIO17). It is the direct reverse of the Serial Monitor (`serialReader` branch), which reads from RX (GPIO18).

Two new services are added:

- **SerialWriterService** — receives data via REST, WebSocket, MQTT, or BLE and writes each line to Serial1.
- **SerialWriterForwarderService** — pulls data from a remote HTTP or WebSocket source and feeds it into SerialWriterService.

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
| Destination | Remote HTTP / WS / MQTT / BLE | SerialWriterService → Serial1 TX |
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

The forwarder pulls data from a remote source and feeds each line to Serial1 via SerialWriterService.

### HTTP Polling Mode

Every `poll_interval_ms` milliseconds, the forwarder GETs `source_url`, parses the JSON response, extracts `json_field`, and writes it to serial if non-empty.

Example: to mirror a remote Serial Monitor in real-time:
```json
{
  "enabled": true,
  "protocol": 0,
  "source_url": "http://192.168.1.100/rest/serial",
  "json_field": "last_line",
  "poll_interval_ms": 500
}
```

### WebSocket Subscribe Mode

Connects to `source_url` as a WebSocket client. Each incoming JSON message is parsed and `json_field` is extracted and written to serial.

Example: subscribe to a remote device's serial WebSocket:
```json
{
  "enabled": true,
  "protocol": 1,
  "source_url": "ws://192.168.1.100/ws/serial",
  "json_field": "last_line"
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
| `SerialWriterService.h/.cpp` | Writer service: receives from all protocols, writes to Serial1 |
| `SerialWriterForwarderService.h/.cpp` | Forwarder: pulls from remote HTTP/WS, feeds SerialWriterService |

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
- [ ] Config changes persist across reboot (`baud_rate`, `line_terminator`)
- [ ] WebSocket `/ws/serialWriter` notifies on each send (check `sent_count` / `last_sent_line`)
- [ ] MQTT send topic delivers line to serial
- [ ] BLE write `{"pending_line":"ble test"}` triggers serial write
- [ ] Forwarder HTTP mode: configure source URL, enable → verify `last_received_line` in UI
- [ ] Forwarder WS mode: connect to WS source → verify streaming
- [ ] Forwarder auth: configure credentials → verify 401 retry and successful auth
- [ ] SerialService (live mode) still functions after switching back from writer mode
- [ ] Firmware builds without errors: `platformio run -e esp32s3`
- [ ] Frontend builds without errors: `cd interface && npm run build`
