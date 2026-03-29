# Weighing Board Service

A complete industrial load cell interface for the Weighsoft Hardware Base framework. Reads weight from an ADS1231 24-bit ADC and streams live data via REST, WebSocket, MQTT, and BLE. Designed to meet OIML D31:2023 type approval requirements.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Getting Started](#2-getting-started)
3. [Hardware Setup](#3-hardware-setup)
4. [Architecture](#4-architecture)
5. [Calibration Procedure (User Guide)](#5-calibration-procedure-user-guide)
6. [Weighing Operations](#6-weighing-operations)
7. [Seal and Type Approval Guide](#7-seal-and-type-approval-guide)
8. [REST API Reference](#8-rest-api-reference)
9. [WebSocket API Reference](#9-websocket-api-reference)
10. [MQTT API Reference](#10-mqtt-api-reference)
11. [BLE API Reference](#11-ble-api-reference)
12. [Configuration Reference](#12-configuration-reference)
13. [Frontend Tabs](#13-frontend-tabs)
14. [Testing Guide](#14-testing-guide)
15. [Troubleshooting](#15-troubleshooting)
16. [Performance](#16-performance)
17. [Type Approval Compliance Summary (OIML D31:2023)](#17-type-approval-compliance-summary)
18. [Related Documentation](#18-related-documentation)

---

## 1. Overview

The **Weighing Board** service (`WeighingService`) reads live weight from an **ADS1231IDR** 24-bit sigma-delta ADC via bit-banged GPIO, computes gross/net/tare values, and exposes them on four communication channels simultaneously.

**Key capabilities:**

| Feature | Description |
|---------|-------------|
| ADC | ADS1231 24-bit, 10 or 80 SPS, bit-banged GPIO |
| Weighing | Gross, net, tare, preset tare, manual zero, auto zero tracking |
| Stability | Rolling buffer, configurable division |
| Protection | Electronic seal + factory PIN (SHA-256, compile-time, immutable) |
| Compliance | OIML D31:2023 event counter + audit trail |
| Channels | REST, WebSocket, MQTT (bidirectional), BLE (READ/WRITE/NOTIFY) |

---

## 2. Getting Started

### Prerequisites

- ESP32 development board
- ADS1231 24-bit load cell ADC
- Load cell compatible with ADS1231 (Wheatstone bridge, typical 2mV/V output)
- PlatformIO build environment
- `FACTORY_CALIBRATION_PIN` set in `factory_settings.ini` (default: `123456`)

### Quick Setup

```bash
# 1. Build and flash firmware
pio run --target upload

# 2. Build and upload frontend
pio run --target buildfs && pio run --target uploadfs

# 3. Access web interface at device IP
# Navigate to Weighing → Information
```

### First Use

1. Open the **Configuration** tab — set max capacity, min division, and unit for your scale.
2. Open the **Calibration** tab — with nothing on the scale, verify that gross weight reads near 0.
3. Place a known reference weight on the scale.
4. Enter the known weight value and click **Calibrate**.
5. Verify the displayed weight matches the known weight.
6. Once satisfied, click **Seal Instrument** to lock the calibration.

---

## 3. Hardware Setup

### ADS1231 GPIO Connections

| ADS1231 Pin | ESP32 GPIO | Direction | Description |
|-------------|-----------|-----------|-------------|
| DOUT | GPIO 25 | Input | Serial data output (MISO equivalent) |
| SCLK | GPIO 23 | Output | Serial clock (bit-banged) |
| PDWN | GPIO 22 | Output | Power down (HIGH = normal operation) |
| SPEED | GPIO 19 | Output | Output rate (LOW = 10 SPS, HIGH = 80 SPS) |
| VCC | 3.3V or 5V | — | Power supply |
| GND | GND | — | Common ground |

### Wiring Notes

- Connect load cell excitation to ADS1231 AVDD and AVSS pins.
- Connect load cell signal to ADS1231 VINP/VINN differential inputs.
- Use shielded cable for the load cell wiring to minimise noise.
- Add a 100nF decoupling capacitor close to ADS1231 VCC pin.
- SPEED LOW (10 SPS) is recommended for best accuracy; use HIGH (80 SPS) only if fast response is needed.

### Supported Load Cells

Any Wheatstone bridge load cell compatible with differential input, typically:
- Rated output: 1 mV/V to 3 mV/V
- Excitation voltage: 3.3V or 5V
- Capacity range: determined by your `max_capacity` setting

---

## 4. Architecture

### Backend Architecture

```
ADS1231 ADC (GPIO 25/23/22/19)
    │
    ▼
ADS1231 driver (bit-bang)
    │  readRawAverage(N samples)
    ▼
WeighingService::loop()
    │  compute gross/net/tare/stability
    ▼
StatefulService<WeighingState>
    │
    ├── HttpEndpoint     → GET/POST /rest/weighing    (IS_AUTHENTICATED)
    ├── WebSocketTxRx    → /ws/weighing               (IS_AUTHENTICATED)
    ├── MqttPubSub       → weighsoft/weighing/{id}/data|set
    ├── BlePubSub        → UUID 56780001-...          (READ|WRITE|NOTIFY)
    └── FSPersistence    → /config/weighing.json
```

### State Architecture

The `WeighingState` has three logical groups:

| Group | Persisted | Description |
|-------|-----------|-------------|
| Live measurement | No | `weight`, `gross_weight`, `net_weight`, `tare_value`, `stable`, etc. |
| Configuration | Yes | `max_capacity`, `min_division`, `unit`, `calibration_factor`, etc. |
| Seal / Audit | Yes | `seal_active`, `event_counter`, `last_calibration_time` |

`read()` serialises all fields for channel broadcast.  
`readConfig()` serialises config + seal fields only (used by FSPersistence, avoids flash wear from high-frequency ADC updates).

### Audit Trail

All legally relevant events are appended to `/audit/weighing.json` (LittleFS). This partition is separate from the application partition and **survives OTA firmware updates**.

---

## 5. Calibration Procedure (User Guide)

Calibration requires the instrument to be **unsealed**. The recommended procedure:

### Step 1 — Empty Scale (Zero Calibration)

1. Ensure the scale platform is empty.
2. Open the **Live Stream** tab — wait for the **STABLE** indicator.
3. Verify gross weight reads near 0. If not (e.g. the scale was moved), click **Zero**.
4. The zero offset is saved to flash and logged to the audit trail.

### Step 2 — Span Calibration

1. Place a certified reference weight on the scale (e.g. 20 kg OIML F2 weight).
2. Wait for the **STABLE** indicator.
3. Open the **Calibration** tab.
4. Enter the known weight value in the "Known Weight" field.
5. Click **Calibrate**.
6. The calibration factor is calculated as: `factor = known_weight / (raw_reading - zero_offset)`
7. The new factor is saved to flash and logged to the audit trail.

### Step 3 — Verification

1. Remove the reference weight, wait for stable reading near 0.
2. Place the reference weight again — the display should show the correct value ±1 division.
3. If correct, proceed to Step 4.

### Step 4 — Sealing

1. Open the **Calibration** tab.
2. Click **Seal Instrument** → confirm in the dialog.
3. The event counter increments and the seal is logged to the audit trail.
4. The instrument is now locked — configuration and calibration changes are blocked.

---

## 6. Weighing Operations

All operations are accessible via REST, WebSocket, MQTT, and BLE (JSON).

### Tare Operations

| Operation | JSON Command | Blocked by Seal | Requires Stable |
|-----------|-------------|-----------------|-----------------|
| Semi-automatic tare | `{"tare": true}` | No | Yes |
| Preset tare | `{"preset_tare": true, "preset_tare_value": 0.5}` | No | No |
| Clear tare | `{"clear_tare": true}` | No | No |

**Semi-automatic tare**: stores current gross weight as tare value, switches to "net" mode.  
**Preset tare**: sets a known container weight without requiring a load on the scale.  
**Clear tare**: resets tare to 0, switches back to "gross" mode.

### Zero Operations

| Operation | Blocked by Seal | Range Limit |
|-----------|-----------------|-------------|
| Manual zero | Yes | ±2% of max capacity (configurable) |
| Power-on zero | N/A (automatic) | ±10% of max capacity (configurable) |
| Auto zero tracking | No (if AZT enabled) | ±`zero_tracking_range` per interval |

**Manual zero**: `{"zero": true}` — adjusts `zero_offset` so the current gross weight becomes 0. Logged to audit trail.  
**Power-on zero**: performed once after startup if the scale is within the configured power-on zero range.  
**Auto zero tracking (AZT)**: continuously corrects small drift. Only active when `center_of_zero` is true, `stable` is true, and no tare is active.

### Stability and Indicators

| Indicator | Condition |
|-----------|-----------|
| `stable` | Max − min of last 8 readings < `min_division` |
| `center_of_zero` | Gross weight within ±0.25d of zero AND no tare |
| `overload` | Gross weight > max_capacity + 9d |
| `underload` | Gross weight < −20d |
| `adc_connected` | False if DOUT never goes LOW within 500 ms |

---

## 7. Seal and Type Approval Guide

### Electronic Seal

The electronic seal prevents all calibration and legally relevant parameter changes until the factory PIN is verified.

**Sealing**: `{"seal": true}` (requires IS_AUTHENTICATED, logs to audit trail, increments event counter)  
**Unsealing**: `{"unseal": true, "pin": "XXXXXX"}` (factory PIN verified via SHA-256 comparison)

### Factory PIN

The calibration PIN is defined at **compile time** in `factory_settings.ini`:

```ini
-D FACTORY_CALIBRATION_PIN=\"123456\"
```

- The PIN is **hashed** (SHA-256) once at startup and compared against incoming PIN submissions.
- The actual PIN value is **never exposed** via any API, log, or channel.
- The PIN **cannot be changed** at runtime — requires a firmware rebuild.
- After 5 consecutive wrong attempts, the unseal function is **locked for 5 minutes** (brute-force protection).
- Remaining lockout time is reported in `pin_lockout_seconds` field.

### Event Counter (OIML 6.2.3.5)

A non-resettable `uint32_t` counter (`event_counter`) is incremented on every legally relevant event:
- Manual zero (changes `zero_offset`)
- Calibration (changes `calibration_factor`)
- Seal activated
- Seal deactivated (unseal)
- Parameter changes (unit, max_capacity, min_division, decimal_places, zero tracking settings)

The counter is persisted to flash and **never decreases** — if a corrupted value smaller than the in-memory value is loaded from flash, it is ignored.

### Audit Trail (OIML 6.2.3.6)

Location: `/audit/weighing.json` (LittleFS, survives OTA)

Each entry contains:

```json
{
  "seq": 12,
  "timestamp": "2026-02-09T14:30:00Z",
  "event": "calibrate",
  "parameter": "calibration_factor",
  "old_value": "0.00123456",
  "new_value": "0.00124500",
  "event_counter": 5
}
```

- **Capacity**: 100 entries. When full, oldest entry is removed (OIML-permitted FIFO).
- **No delete API**: the log cannot be cleared via any endpoint.
- **Timestamps**: ISO 8601 from NTP. If NTP is not synced: `NO_NTP_<millis>`.
- **Boot logging**: every firmware boot is logged as an `"event": "boot"` entry.

Access the audit log:
- REST: `GET /rest/weighingAudit` (IS_ADMIN)
- Frontend: **Audit Trail** tab

### For Type Approval Examination

The examiner verification workflow:

1. `GET /rest/version` — confirm software identification (OIML 6.2.1)
2. `GET /rest/weighing` — confirm `event_counter` is present and non-zero
3. Attempt calibration while sealed — confirm HTTP 400 response
4. `GET /rest/weighingAudit` — review all events with timestamps and counter values
5. Attempt unseal with wrong PIN 5 times — confirm lockout activates
6. Verify lockout duration in `pin_lockout_seconds`
7. Unseal with correct PIN — confirm counter increments in audit trail

---

## 8. REST API Reference

### `GET /rest/weighing`

**Auth**: IS_AUTHENTICATED  
Returns current state (live measurement + configuration + seal info):

```json
{
  "weight": 12.34,
  "gross_weight": 12.34,
  "net_weight": 11.84,
  "tare_value": 0.50,
  "weight_mode": "net",
  "raw_value": 498765,
  "stable": true,
  "center_of_zero": false,
  "overload": false,
  "underload": false,
  "adc_connected": true,
  "unit": "kg",
  "zero_offset": 123456,
  "calibration_factor": 0.00001234,
  "max_capacity": 100.0,
  "min_division": 0.02,
  "decimal_places": 2,
  "zero_tracking_enabled": true,
  "zero_tracking_range": 0.01,
  "power_on_zero_range": 10.0,
  "manual_zero_range": 2.0,
  "speed_mode": 0,
  "samples_to_average": 8,
  "seal_active": false,
  "event_counter": 7,
  "last_calibration_time": "2026-02-09T14:30:00Z",
  "pin_lockout_seconds": 0
}
```

### `POST /rest/weighing`

**Auth**: IS_AUTHENTICATED  
Send a command or configuration update. All config changes are blocked when `seal_active == true`.

**Tare commands** (not blocked by seal):
```json
{ "tare": true }
{ "preset_tare": true, "preset_tare_value": 0.5 }
{ "clear_tare": true }
```

**Calibration commands** (blocked when sealed):
```json
{ "zero": true }
{ "calibrate": true, "known_weight": 20.0 }
```

**Seal management**:
```json
{ "seal": true }
{ "unseal": true, "pin": "123456" }
```

**Configuration** (blocked when sealed):
```json
{
  "unit": "kg",
  "max_capacity": 100.0,
  "min_division": 0.02,
  "decimal_places": 2,
  "zero_tracking_enabled": true
}
```

**Responses**:
- `200 OK`: state JSON (including updated values)
- `400 Bad Request`: command rejected (e.g. `seal_active == true` for calibration)
- `401 Unauthorized`: not authenticated

### `GET /rest/weighingAudit`

**Auth**: IS_ADMIN  
Returns the full audit trail as a JSON array. See [Section 7](#7-seal-and-type-approval-guide) for entry format.

### `GET /rest/version`

**Auth**: NONE_REQUIRED (public)  
Returns software identification for OIML 6.2.1:

```json
{
  "version": "0.1.0",
  "api_version": "v1",
  "build_date": "Feb  9 2026",
  "build_time": "14:30:00",
  "project_name": "Weighsoft Weighing Board",
  "project_url": "https://github.com/henzard/Weighsoft.Hardware.Base"
}
```

---

## 9. WebSocket API Reference

**Endpoint**: `ws://<device-ip>/ws/weighing`  
**Auth**: IS_AUTHENTICATED (send Bearer token as first message or via query param)

The WebSocket streams the full state JSON every loop cycle (typically 100–200 ms). No special subscribe message is needed — connection triggers immediate data flow.

**Incoming (server → client)**: full state JSON (same as `GET /rest/weighing`)

**Outgoing (client → server)**: same command JSON as `POST /rest/weighing`

Example (browser):
```javascript
const ws = new WebSocket('ws://192.168.1.100/ws/weighing');
ws.onmessage = (e) => {
  const state = JSON.parse(e.data);
  console.log(`Weight: ${state.weight} ${state.unit}`);
};
// Send tare command
ws.send(JSON.stringify({ tare: true }));
```

---

## 10. MQTT API Reference

**Publish topic** (device → broker): `weighsoft/weighing/{unique_id}/data`  
**Subscribe topic** (broker → device): `weighsoft/weighing/{unique_id}/set`

The `{unique_id}` is the device's unique ID (configured in framework settings).

**Published payload**: full state JSON (same as REST response)

**Subscribe payload**: command JSON (same as REST POST body)

Example subscriptions (mosquitto):
```bash
# Monitor weight stream
mosquitto_sub -h broker.local -t "weighsoft/weighing/+/data"

# Send tare command
mosquitto_pub -h broker.local -t "weighsoft/weighing/MY_DEVICE/set" -m '{"tare":true}'

# Calibrate
mosquitto_pub -h broker.local -t "weighsoft/weighing/MY_DEVICE/set" -m '{"calibrate":true,"known_weight":20.0}'
```

---

## 11. BLE API Reference

**Service UUID**: `56780000-e8f2-537e-4f6c-d104768a5678`  
**Characteristic UUID**: `56780001-e8f2-537e-4f6c-d104768a5678`  
**Properties**: READ, WRITE, NOTIFY

**Read / Notify**: returns current state JSON (may be truncated to 512 bytes for BLE MTU)  
**Write**: accepts command JSON (same format as REST POST)

Enable BLE in the BLE Settings page. The weighing service is configured automatically when the BLE server starts.

---

## 12. Configuration Reference

All configuration is persisted to `/config/weighing.json`.

| Field | Key | Default | Legally Relevant | Description |
|-------|-----|---------|-----------------|-------------|
| Unit | `unit` | `"kg"` | Yes | `"kg"`, `"g"`, or `"lb"` |
| Max Capacity | `max_capacity` | `100.0` | Yes | Maximum weighing range |
| Min Division | `min_division` | `0.02` | Yes | Scale division `d` (sets overload/stability thresholds) |
| Decimal Places | `decimal_places` | `2` | Yes | Display resolution (0–6) |
| Zero Tracking | `zero_tracking_enabled` | `true` | Yes | Enable automatic zero tracking |
| AZT Range | `zero_tracking_range` | `0.01` | Yes | Max AZT correction per interval (typically 0.5d) |
| Power-On Zero Range | `power_on_zero_range` | `10.0` | Yes | Auto-zero at startup if within N% of max capacity |
| Manual Zero Range | `manual_zero_range` | `2.0` | Yes | Max weight to allow manual zero (% of max capacity) |
| Speed Mode | `speed_mode` | `0` | No | `0` = 10 SPS (low noise), `1` = 80 SPS (fast) |
| Samples to Average | `samples_to_average` | `8` | No | ADC readings averaged per update (1–32) |

---

## 13. Frontend Tabs

| Tab | Path | Description |
|-----|------|-------------|
| Information | `.../weighing/information` | Hardware setup, features, type approval summary |
| Configuration | `.../weighing/configuration` | Scale parameters (blocked when sealed) |
| Calibration | `.../weighing/calibration` | Span calibration + seal/unseal controls |
| REST View | `.../weighing/rest` | Polls REST API every 2 seconds |
| Live Stream | `.../weighing/stream` | WebSocket real-time weight, tare/zero buttons |
| Audit Trail | `.../weighing/audit` | Read-only audit log table (admin only) |
| BLE | `.../weighing/ble` | BLE UUIDs and command documentation |

---

## 14. Testing Guide

### Backend Unit Tests

Test calibration algorithm:
```
gross = (raw - zeroOffset) * calibrationFactor
Expected: gross ≈ knownWeight (within ±1 division)
```

Test stability detection:
- Fill buffer with identical readings → `stable = true`
- Add reading differing by > `minDivision` → `stable = false`

Test seal enforcement:
```bash
# Seal first
curl -X POST http://device/rest/weighing -d '{"seal":true}' -H "Authorization: Bearer TOKEN"
# Attempt calibration (should return 400)
curl -X POST http://device/rest/weighing -d '{"calibrate":true,"known_weight":20}' -H "Authorization: Bearer TOKEN"
```

### REST API Tests

```bash
# Read current state
curl http://device/rest/weighing -H "Authorization: Bearer TOKEN"

# Tare
curl -X POST http://device/rest/weighing -d '{"tare":true}' -H "Authorization: Bearer TOKEN"

# Check audit trail (admin)
curl http://device/rest/weighingAudit -H "Authorization: Bearer ADMIN_TOKEN"

# Check version (public)
curl http://device/rest/version
```

### WebSocket Test

```javascript
const ws = new WebSocket('ws://DEVICE_IP/ws/weighing');
ws.onopen = () => ws.send(JSON.stringify({ tare: true }));
ws.onmessage = (e) => console.log(JSON.parse(e.data).weight);
```

---

## 15. Troubleshooting

### ADC Not Connected (`adc_connected: false`)

- Verify DOUT=GPIO25, SCLK=GPIO23, PDWN=GPIO22, SPEED=GPIO19 connections.
- Check ADS1231 power supply (VCC and GND).
- Ensure PDWN is HIGH (should be driven HIGH by the driver at startup).
- Check for short circuits on DOUT/SCLK lines.

### Weight Reads 0 or Constant Value

- Verify load cell wiring to ADS1231 VINP/VINN differential inputs.
- Check `zero_offset` — if it equals the raw ADC value, the calibration factor may be incorrect.
- Reset to factory defaults (wipe `/config/weighing.json`) and recalibrate.

### Weight Drifts Over Time

- Enable auto zero tracking (AZT) in Configuration.
- Check for temperature effects on the load cell.
- Ensure stable power supply to ADS1231.

### Seal Cannot Be Activated

- Ensure `sealActive == false` (instrument is currently unsealed).
- Require IS_AUTHENTICATED access token.

### Cannot Unseal — Wrong PIN

- The PIN is set in `factory_settings.ini` as `FACTORY_CALIBRATION_PIN`.
- Default: `"123456"`.
- If 5 wrong attempts are made, the lockout will last 5 minutes.

### Audit Trail Shows `NO_NTP_XXXXX` Timestamps

- NTP is not synced. Check NTP server settings in the Time Settings page.
- Timestamps will update to ISO 8601 format once NTP syncs.

---

## 16. Performance

| Metric | Value |
|--------|-------|
| ADC sample rate (low noise) | 10 SPS |
| ADC sample rate (fast) | 80 SPS |
| Averaging (default) | 8 samples → ~1.25 updates/second at 10 SPS |
| WebSocket update rate | Matches `loop()` execution rate (~100–200 ms) |
| Stability detection | 8-sample rolling buffer |
| Audit file size | ~100 entries × ~200 bytes = ~20 KB |
| Config file size | ~512 bytes |

---

## 17. Type Approval Compliance Summary

Compliance with **OIML D31 Edition 2023** for software-controlled measuring instruments:

| OIML Requirement | Implementation | Status |
|-----------------|----------------|--------|
| **6.2.1 Software Identification** | `GET /rest/version` returns version, build date, project name. Logged to audit trail on boot. | ✅ Covered |
| **6.2.2 Algorithm Correctness** | `weight = (raw - zeroOffset) * calibrationFactor`. Deterministic, documented. | ✅ Covered |
| **6.2.3.4 Parameter Protection** | `seal_active` flag blocks all legally relevant parameter changes when true. | ✅ Covered |
| **6.2.3.5 Event Counter** | `event_counter` (uint32_t), persisted, non-resettable, never decreases on load. | ✅ Covered |
| **6.2.3.6 Audit Trail** | `/audit/weighing.json`: timestamp, event, parameter, old/new values, counter. Append-only. | ✅ Covered |
| **6.2.5 Demands on User** | Step-by-step calibration UI, stability required before calibrate/zero, clear error messages. | ✅ Covered |
| **6.2.6.1 Defect Detection** | `adc_connected`, `overload`, `underload` reported on all channels. | ✅ Covered |
| **6.2.7 Timestamps** | NTP-synced ISO 8601, fallback to `NO_NTP_<millis>` if NTP unavailable. | ✅ Covered |
| **6.3.2.1.2 Protective Interface** | All commands validated in `WeighingState::update()`. Unknown keys ignored. Sealed params rejected. | ✅ Covered |
| **6.3.4 Data Storage** | Config on LittleFS (`/config/weighing.json`), audit on LittleFS (`/audit/weighing.json`). | ✅ Covered |
| **6.3.5 Transmission Integrity** | JWT auth on REST/WS, MQTT credentials, BLE pairing. Full state in every message. | ✅ Covered |
| **6.3.8.4 Traced Updates** | Audit trail on LittleFS survives OTA. Boot event logged with firmware version. | ✅ Covered |
| **Factory PIN protection** | SHA-256 hashed at runtime, immutable (compile-time), brute-force lockout after 5 attempts. | ✅ Covered |

---

## 18. Related Documentation

- [`docs/ARCHITECTURE.md`](ARCHITECTURE.md) — Framework architecture overview
- [`docs/DESIGN-PATTERNS.md`](DESIGN-PATTERNS.md) — StatefulService and composition patterns
- [`docs/EXTENSION-GUIDE.md`](EXTENSION-GUIDE.md) — How to extend the framework
- [`docs/API-REFERENCE.md`](API-REFERENCE.md) — All REST, WebSocket, MQTT, BLE endpoints
- [`docs/LED-EXAMPLE.md`](LED-EXAMPLE.md) — LED example (base pattern used for this service)
- [`docs/SERIAL-EXAMPLE.md`](SERIAL-EXAMPLE.md) — Serial example (split-state pattern reference)
- [`docs/LOADCELL-INTEGRATION.md`](LOADCELL-INTEGRATION.md) — Load cell integration notes
- `factory_settings.ini` — Factory default settings including `FACTORY_CALIBRATION_PIN`
- OIML D31 Edition 2023 — `docs/type approval/d031-e23.pdf`
