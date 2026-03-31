# Growbox Monitor Integration

## Overview

The Growbox Monitor is a **multi-layer integration** that controls a 4-channel relay board (2 lights + 2 fans), reads temperature, humidity, and soil moisture sensors, and exposes everything through REST, WebSocket, MQTT, and BLE.

**Target hardware**: ESP32-S3 DevKitC-1 (8 MB flash, 512 KB SRAM, BLE 5.0)

**Grows**: Herbs and greens (basil, coriander, mint) with automated daily light scheduling and temperature-driven fan control.

---

## Architecture

### Three-Layer Pattern

The Growbox integration uses the **three-service pattern** — hardware configuration, live state, and automation settings are separated into distinct services:

```
GrowboxSettingsService          (hardware config — GPIO pins, calibration)
├── HttpEndpoint<GrowboxSettings>    REST API
└── FSPersistence<GrowboxSettings>   Persisted to /config/growboxSettings.json

GrowboxAutomationService        (automation rules — schedules, thresholds)
├── HttpEndpoint<GrowboxAutomationSettings>  REST API
├── WebSocketTxRx<GrowboxAutomationSettings> Real-time updates
└── FSPersistence<GrowboxAutomationSettings> Persisted to /config/growboxAutomation.json

GrowboxService                  (live state — relays, sensors, alarms)
├── HttpEndpoint<GrowboxState>       REST API
├── WebSocketTxRx<GrowboxState>      Real-time dashboard updates
├── MqttPubSub<GrowboxState>         Lights relay (HA light entity)
├── MqttPubSub<GrowboxState>         Fan In relay (HA switch entity)
├── MqttPubSub<GrowboxState>         Fan Out relay (HA switch entity)
├── MqttPubSub<GrowboxAutomationSettings>  Automation over MQTT
├── MqttPub<GrowboxState>            Full state JSON (sensor topic)
├── BlePubSub<GrowboxState>          Controls characteristic (R/W/Notify)
├── BlePub<GrowboxState>             Sensors characteristic (R/Notify)
└── BlePubSub<GrowboxAutomationSettings>  Automation characteristic (R/W/Notify)
```

---

## Hardware

### Components

| Component | Part | Connection |
|-----------|------|-----------|
| MCU | ESP32-S3 DevKitC-1 | — |
| Relay board | 4-channel opto-isolated, 5 V coil | GPIO (active-low by default) |
| Inside sensor | SHT20 temperature + humidity | I2C (SDA GPIO 8, SCL GPIO 9) |
| Outside sensor | DHT22 temperature + humidity | One-wire (GPIO 10, 10 kΩ pull-up) |
| Soil moisture | Capacitive V1.0 (analog out) | ADC (GPIO 4) |

### Default GPIO Map (ESP32-S3)

| Signal | GPIO | Notes |
|--------|------|-------|
| Light relay A | 15 | Active-low (configurable) |
| Light relay B | 16 | Active-low (configurable) |
| Fan In relay | 17 | Active-low (configurable) |
| Fan Out relay | 18 | Active-low (configurable) |
| I2C SDA (SHT20) | 8 | Internal pull-up |
| I2C SCL (SHT20) | 9 | Internal pull-up |
| DHT22 data | 10 | Needs external 10 kΩ pull-up to 3.3 V |
| Moisture ADC | 4 | 3.3 V analog input, avoid GPIO 5/6/7 |

**Excluded ESP32-S3 pins**: 0 (boot), 3 (JTAG/USB), 19/20 (USB-OTG), 26–32 (flash), 33–37 (PSRAM), 45/46 (strapping).

---

## Relay Polarity

Most opto-isolated relay boards are **active-low**: drive the GPIO LOW to energise the relay coil.

The `relay_active_high` flag in Settings inverts the logic if your board is active-high. All relays default to **off (de-energised) on boot**.

---

## File Structure

### Backend (`src/examples/growbox/`)

| File | Purpose |
|------|---------|
| `GrowboxSettingsService.h/.cpp` | Hardware config service (GPIO pins, calibration, sensor enable flags) |
| `GrowboxAutomationService.h/.cpp` | Automation rules service (schedules, thresholds, alarms) |
| `GrowboxService.h/.cpp` | Live state service (relays, sensors, MQTT, BLE) |

### Frontend (`interface/src/examples/growbox/`)

| File | Purpose |
|------|---------|
| `types.ts` | TypeScript interfaces for all three services |
| `api.ts` | Axios API client functions |
| `Growbox.tsx` | Tab router (Dashboard / Automation / BLE / Settings) |
| `GrowboxDashboard.tsx` | Live status, relay controls, sensor display via WebSocket |
| `GrowboxAutomationForm.tsx` | Light schedule, fan thresholds, moisture alarm settings |
| `GrowboxBleInfo.tsx` | BLE characteristic reference and usage examples |
| `GrowboxSettingsForm.tsx` | GPIO configuration, relay test mode, moisture calibration |

---

## REST API

### Live State — `GET/POST /rest/growbox`

Returns or updates the live `GrowboxState` object.

**Writable fields only** (POST):

```json
{
  "lights_on": true,
  "light_a_on": true,
  "light_b_on": true,
  "fan_in_on": false,
  "fan_out_on": false,
  "moisture_alarm_acknowledged": false
}
```

All other fields (sensor readings, modes, reason strings, diagnostics) are read-only.

**Full response example**:

```json
{
  "lights_on": true,
  "light_a_on": true,
  "light_b_on": true,
  "fan_in_on": false,
  "fan_out_on": false,
  "lights_mode": "schedule",
  "fan_in_mode": "auto",
  "fan_out_mode": "auto",
  "inside_temperature": 24.1,
  "inside_humidity": 68.3,
  "outside_temperature": 22.5,
  "outside_humidity": 55.0,
  "moisture_raw": 2100,
  "moisture_percent": 62.5,
  "moisture_calibrated": true,
  "moisture_alarm_active": false,
  "moisture_alarm_acknowledged": false,
  "inside_sensor_fault": false,
  "outside_sensor_fault": false,
  "moisture_sensor_fault": false,
  "lights_reason": "schedule",
  "fan_in_reason": "temperature",
  "fan_out_reason": "temperature",
  "moisture_alarm_reason": "",
  "schedule_active": true,
  "manual_override_active": false,
  "uptime_ms": 3600000,
  "boot_count": 5,
  "ntp_synced": true,
  "wifi_connected": true
}
```

### Hardware Settings — `GET/POST /rest/growboxSettings`

```json
{
  "light_relay_a_pin": 15,
  "light_relay_b_pin": 16,
  "fan_in_pin": 17,
  "fan_out_pin": 18,
  "i2c_sda_pin": 8,
  "i2c_scl_pin": 9,
  "dht_pin": 10,
  "moisture_adc_pin": 4,
  "moisture_dry_value": 2800,
  "moisture_wet_value": 1200,
  "relay_active_high": false,
  "poll_interval_ms": 5000,
  "inside_sensor_enabled": true,
  "outside_sensor_enabled": true,
  "moisture_sensor_enabled": true
}
```

### Automation Settings — `GET/POST /rest/growboxAutomation`

```json
{
  "light_schedule_enabled": true,
  "light_on_time": "06:00",
  "light_off_time": "22:00",
  "fan_control_enabled": true,
  "fan_temperature_threshold": 28.0,
  "fan_hysteresis": 2.0,
  "moisture_alarm_enabled": true,
  "moisture_alarm_threshold": 20,
  "moisture_alarm_dwell_ms": 300000
}
```

---

## WebSocket

| Endpoint | Data |
|----------|------|
| `/ws/growbox` | Live `GrowboxState` — updates every poll interval and on relay state changes |
| `/ws/growboxAutomation` | `GrowboxAutomationSettings` — updates when automation config changes |

---

## MQTT Topics

All topics are prefixed with `growbox/<unique_id>/`.

### Relay Control (Home Assistant compatible)

| Topic | Direction | Format |
|-------|-----------|--------|
| `growbox/<id>/lights/state` | Publish | `{"state":"ON"}` or `{"state":"OFF"}` |
| `growbox/<id>/lights/set` | Subscribe | `{"state":"ON"}` or `{"state":"OFF"}` |
| `growbox/<id>/fan_in/state` | Publish | `{"state":"ON"}` |
| `growbox/<id>/fan_in/set` | Subscribe | `{"state":"ON"}` |
| `growbox/<id>/fan_out/state` | Publish | `{"state":"ON"}` |
| `growbox/<id>/fan_out/set` | Subscribe | `{"state":"ON"}` |

### Full State (Sensor Data)

| Topic | Direction | Format |
|-------|-----------|--------|
| `growbox/<id>/state` | Publish | Full `GrowboxState` JSON |

### Automation Settings

| Topic | Direction | Format |
|-------|-----------|--------|
| `growbox/<id>/automation/state` | Publish | Full `GrowboxAutomationSettings` JSON |
| `growbox/<id>/automation/set` | Subscribe | Partial or full `GrowboxAutomationSettings` JSON |

### Home Assistant Auto-Discovery

On every MQTT connect, the device publishes discovery payloads to `homeassistant/<component>/<uid>-<id>/config`. The following entities are auto-discovered:

| Entity | HA Component | HA Class |
|--------|-------------|----------|
| Lights | `light` | — |
| Fan In | `switch` | — |
| Fan Out | `switch` | — |
| Inside Temperature | `sensor` | `temperature` |
| Inside Humidity | `sensor` | `humidity` |
| Outside Temperature | `sensor` | `temperature` |
| Outside Humidity | `sensor` | `humidity` |
| Soil Moisture | `sensor` | — |
| Moisture Alarm | `binary_sensor` | `moisture` |

---

## BLE Interface

**BLE must be enabled** in the BLE Settings page. Enable it, then connect with a generic BLE scanner (nRF Connect, LightBlue) or your own app.

**Service UUID**: `4b6e5d7f-bf21-4e9a-9c87-6e5d1042a3b1`

### Characteristics

#### Controls — `a3b4c5d6-bf21-4e9a-9c87-6e5d1042a3b2` (R/W/Notify)

Relay outputs and alarm. Compact abbreviated keys:

| Key | Type | Description | Writable |
|-----|------|-------------|----------|
| `la` | bool | Light A on | ✓ |
| `lb` | bool | Light B on | ✓ |
| `fi` | bool | Fan In on | ✓ |
| `fo` | bool | Fan Out on | ✓ |
| `ma` | bool | Moisture alarm active | — |
| `mak` | bool | Alarm acknowledged | ✓ |

#### Sensors — `b5c6d7e8-bf21-4e9a-9c87-6e5d1042a3b3` (R/Notify)

Live sensor readings:

| Key | Type | Description |
|-----|------|-------------|
| `it` | float | Inside temperature (°C) |
| `ih` | float | Inside humidity (%) |
| `ot` | float | Outside temperature (°C) |
| `oh` | float | Outside humidity (%) |
| `mp` | float | Soil moisture (%) |
| `isf` | bool | Inside sensor fault |
| `osf` | bool | Outside sensor fault |
| `msf` | bool | Moisture sensor fault |
| `ntp` | bool | NTP sync OK |
| `wifi` | bool | WiFi connected |

#### Automation — `c7d8e9f0-bf21-4e9a-9c87-6e5d1042a3b4` (R/W/Notify)

Same key names as REST API (full names, not abbreviated). Write partial or full JSON to update automation settings.

---

## Automation Logic

### Light Scheduling

When `light_schedule_enabled` is true and NTP is synced, lights are turned on/off at the configured local times. Scheduling requires a configured NTP timezone (set in NTP Settings).

### Fan Control

When `fan_control_enabled` is true, fans are driven by inside temperature:

- Fans **ON** when inside temperature ≥ `fan_temperature_threshold`
- Fans **OFF** when inside temperature < `fan_temperature_threshold - fan_hysteresis`

Hysteresis prevents rapid on/off cycling near the threshold.

**Offline behaviour**: Automation continues running without WiFi. The last automation state is preserved through power cycles.

### Moisture Alarm

When `moisture_alarm_enabled` is true and `moisture_percent` stays below `moisture_alarm_threshold` for longer than `moisture_alarm_dwell_ms`, `moisture_alarm_active` is set true. The alarm is cleared by either:

1. Acknowledging via REST/WS/MQTT/BLE (`moisture_alarm_acknowledged = true`)
2. Moisture returning above threshold (alarm auto-clears after 30 s above threshold)

---

## Moisture Sensor Calibration

The capacitive moisture sensor outputs a raw ADC value — lower values mean wetter soil. To calibrate:

1. Navigate to **Settings** tab in the Growbox UI
2. With sensor in **dry air**, click **Set as Dry** to record the dry ADC value
3. Submerge sensor in water to the max-fill line, click **Set as Wet** to record the wet ADC value
4. Save settings

The firmware maps raw ADC to `moisture_percent` using linear interpolation between the two calibration points.

---

## Verification Checklist

### Hardware

- [ ] All four relays switch correctly (use relay test buttons in Settings tab)
- [ ] Relay polarity correct — loads de-energised on boot
- [ ] SHT20 readings appear on dashboard (inside temp/humidity non-zero)
- [ ] DHT22 readings appear (outside temp/humidity non-zero)
- [ ] Moisture sensor reads a non-zero value (outside_sensor_fault = false)
- [ ] Moisture calibration applied (moisture_calibrated = true)

### Automation

- [ ] Light schedule triggers at configured on/off times
- [ ] Fans turn on when temperature exceeds threshold
- [ ] Fans turn off when temperature drops below (threshold - hysteresis)
- [ ] Moisture alarm fires after dwell time at low moisture
- [ ] Alarm clears after acknowledgement
- [ ] Automation continues working without WiFi

### Protocols

- [ ] REST: `GET /rest/growbox` returns full state
- [ ] REST: `POST /rest/growbox` with `{"lights_on":true}` turns lights on
- [ ] WebSocket `/ws/growbox` streams live state
- [ ] MQTT: Lights state published to `growbox/<id>/lights/state`
- [ ] MQTT: Writing `{"state":"ON"}` to `growbox/<id>/lights/set` turns lights on
- [ ] MQTT: Sensor data published to `growbox/<id>/state`
- [ ] Home Assistant auto-discovery works (entities appear in HA)
- [ ] BLE: Controls characteristic readable and writable
- [ ] BLE: Sensor notifications received on state change

---

## Extending the Integration

### Adding a Second Moisture Sensor

1. Add a second ADC pin field in `GrowboxSettings`
2. Add second moisture fields to `GrowboxState`
3. Read the second sensor in `readSensors()`
4. Add the field to the dashboard component

### Adding a CO₂ Sensor (e.g. MH-Z19)

1. Add to `GrowboxSettings`: `co2_rx_pin`, `co2_tx_pin`
2. Add `co2_ppm` to `GrowboxState`
3. Use `SoftwareSerial` in `readSensors()` to poll MH-Z19
4. Add HA discovery for a `sensor` with `device_class: carbon_dioxide`
5. Add BLE sensor field and dashboard display

### Complex Automation with Home Assistant

For multi-variable or time-of-week automation (beyond what the ESP32 handles), use MQTT with Node-RED or Home Assistant automations:

- Subscribe to `growbox/<id>/state` for live sensor data
- Publish to `growbox/<id>/lights/set`, `fan_in/set`, `fan_out/set` to control relays
- Publish to `growbox/<id>/automation/set` to update thresholds remotely
