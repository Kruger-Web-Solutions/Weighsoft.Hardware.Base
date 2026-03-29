# Load Cell Integration Guide

> **Status**: ✅ Fully implemented as the **Weighing Board** service.  
> See [`WEIGHING-EXAMPLE.md`](WEIGHING-EXAMPLE.md) for the complete user guide and API reference.

---

## Overview

This guide explains how the **ADS1231** load cell ADC interfaces with the ESP32. The full
implementation lives in `src/examples/weighing/` and the frontend in
`interface/src/examples/weighing/`.

---

## ADS1231 Chip Connection

### Hardware Pinout

**Chip:** ADS1231IDR (Texas Instruments)  
**Package:** SOIC-16  
**Designator:** U1 on PCB  
**Purpose:** 24-bit ADC for Wheatstone bridge load cell amplification

### ESP32 GPIO Connections

| ADS1231 Pin | Signal | ESP32 GPIO | Direction | Function |
|-------------|--------|-----------|-----------|----------|
| PIN 10 (DOUT/DRDY) | DOUT | **GPIO25** | Input | Data Output / Data Ready |
| PIN 11 (SCLK) | SCLK | **GPIO23** | Output | Serial Clock (bit-banged) |
| PIN 12 (PDWN) | PDWN | **GPIO22** | Output | Power Down (HIGH=normal) |
| PIN 13 (SPEED) | SPEED | **GPIO19** | Output | Data Rate (LOW=10SPS, HIGH=80SPS) |
| PIN 1-4 (AINP/AINN) | SIG+/SIG- | Load Cell | — | Differential analog input |
| PIN 7 (REFIN+/REFN-) | EXCSNS+/- | Load Cell | — | Excitation sense (optional) |
| VDD | +5VA | Power | — | 5V analog supply (filtered) |
| GND | GND | GND | — | Common ground |

### Load Cell Connector

**Connector J1** (6-pin Phoenix Contact, 3.81mm pitch) provides load cell connection:

| Pin | Signal | Colour (typical) | Purpose |
|-----|--------|------------------|---------|
| 1 | EXC+ | Red | Excitation voltage positive (+5V) |
| 2 | EXCSNS+ | — | Excitation sense positive (optional) |
| 3 | SIG+ | White | Signal positive (from load cell) |
| 4 | SIG- | Black | Signal negative (from load cell) |
| 5 | EXCSNS- | — | Excitation sense negative (optional) |
| 6 | EXC- | Black | Excitation voltage negative (GND) |

### Communication Protocol

The ADS1231 uses a **two-wire bit-banged serial interface** (not true SPI):

- **DOUT/DRDY**: Dual-purpose pin — HIGH = no data, LOW = data ready
- **SCLK**: Clock pulses from ESP32 (24 pulses per reading + 1 for channel select)
- **Data Rate**: 10 SPS (LOW, default) or 80 SPS (HIGH) configurable via SPEED pin
- **Sign extension**: 24-bit two's complement extended to `int32_t`

Driver implemented in `src/examples/weighing/ADS1231.h/.cpp`.

---

## Programming the ESP32

### No Built-in USB Programmer

The custom PCB (**scale_wifi**) has **no USB-to-UART bridge chip** (no CP2102/CH340/FTDI).
You need an **external USB-UART adapter** (3.3V logic level).

### PCB Header Reference

| Header | Pins | Signals | Purpose |
|--------|------|---------|---------|
| **SK1** | 4-pin, 3.81mm | VIN, NC, NC, GND | Power input only — **not** a programming header |
| **SK2** | 2-pin, 2.54mm | GPIO3/RXD0, GPIO1/TXD0 | **UART0 programming/serial** |
| **SK3** | 2-pin, 2.54mm | GPIO16, GPIO17 | Serial2 (RX/TX) loopback — **not** boot mode |
| **S1** | tactile button | GPIO0 → GND | **Boot mode** — the only way to enter download mode |

**SK3 location:** top edge of PCB at (65.3 mm, 10.5 mm).  
**SK2 location:** top edge of PCB at (74.5 mm, 10.5 mm).

### Connecting a USB-UART Adapter

Connect to **SK2** and a nearby GND point:

| SK2 Pin | Signal | Adapter Connection |
|---------|--------|--------------------|
| 1 (left) | GPIO3 / RXD0 | **TX** on adapter |
| 2 (right) | GPIO1 / TXD0 | **RX** on adapter |
| — | GND | GND (use SK1 pin 4 or any GND test point) |

> TX on adapter → RX on ESP32 (GPIO3). RX on adapter ← TX from ESP32 (GPIO1). They cross over.

### Entering Flash Mode (Boot Mode)

The PCB has **no auto-reset circuit**. Boot mode is entered via **S1** — a tactile button that pulls **GPIO0 LOW**.

**GPIO0** connects only to S1 and a 10 kΩ pull-up to 3.3V. When S1 is pressed it pulls GPIO0 to GND, signalling the ESP32 to boot into download mode at the next power-on.

**Procedure:**

1. **Press and hold S1** (top-right area of PCB, near SK2)
2. **Power cycle the board** — remove and reapply power while still holding S1
3. **Keep holding S1 for ~2 seconds** after power-on, then release
4. **Run the upload command** — see below
5. **Power cycle again** after upload completes to boot into normal firmware

> If S1 is not populated on your board, short the two S1 footprint pads that connect to GPIO0 to the adjacent GND pads using tweezers or a wire while power cycling.

> **"No serial data received"** means GPIO0 was not LOW at boot. Redo from step 1.

### Upload Commands

```bash
# Flash firmware (initial or update)
python -m platformio run --target upload

# Flash filesystem (first time only — web UI, config structure)
python -m platformio run --target uploadfs
```

### OTA Updates (After First Flash)

Once the ESP32 has WiFi and initial firmware:

1. Navigate to `http://<esp32-ip>/` → **System** → **Upload**
2. Upload `.pio/build/node32s/firmware.bin`

Or configure in `platformio.ini`:
```ini
upload_protocol = espota
upload_port = 192.168.0.100
upload_flags = --port=8266 --auth=esp-react
```

### Build Environments

| Environment | Platform | Flash | BLE | Use Case |
|-------------|----------|-------|-----|----------|
| `node32s` | ESP32 | 4MB | ✅ | **Default — full features** |
| `esp32dev` | ESP32 | 4MB | ✅ | Debug mode (verbose output) |
| `esp12e` | ESP8266 | 4MB | ❌ | Legacy (no weighing service) |

---

## Load Cell Calibration

### Calibration Theory

Weight calculation formula:

```
gross_weight = (raw_adc - zero_offset) × calibration_factor
```

- `zero_offset`: raw ADC reading with empty scale
- `calibration_factor`: converts raw counts to weight units (e.g. kg/count)

### Calibration Procedure (Step-by-Step)

#### Step 1 — Zero Calibration

1. Ensure scale platform is empty
2. Open **Live Stream** tab → wait for **STABLE** indicator
3. Click **Zero** (or the scale will auto-zero on power-on if within ±10% of max)
4. `zero_offset` is updated, saved to flash, and logged to the audit trail

#### Step 2 — Span Calibration

1. Place a certified reference weight on the scale
2. Wait for **STABLE**
3. Open **Calibration** tab → enter known weight value → click **Calibrate**
4. System calculates: `calibration_factor = known_weight / (raw - zero_offset)`
5. Factor is saved to flash and logged to the audit trail

#### Step 3 — Seal

1. Verify displayed weight matches known weight ±1 division
2. Click **Seal Instrument** — requires factory PIN to unseal
3. All calibration and configuration changes are now blocked

### Recommended Calibration Weights

| Scale Capacity | Recommended Test Weight |
|----------------|------------------------|
| 0–50 kg | 20 kg (40% capacity) |
| 0–100 kg | 50 kg (50% capacity) |
| 0–500 kg | 200 kg (40% capacity) |
| 0–1000 kg | 500 kg (50% capacity) |

Use **OIML Class F2** or better certified weights. Calibrate in the actual operating
environment (same temperature and mounting conditions as production use).

---

## Implementation Summary

All phases from the original roadmap are complete:

| Phase | Description | Status |
|-------|-------------|--------|
| ADS1231 driver | Bit-bang protocol, averaging, power control | ✅ `ADS1231.h/.cpp` |
| WeighingService | StatefulService with all 4 channels | ✅ `WeighingService.h/.cpp` |
| State management | Gross/net/tare, stability, AZT, overload | ✅ `WeighingState.h` |
| Audit trail | OIML 6.2.3.6 append-only log | ✅ `AuditTrail.h/.cpp` |
| Electronic seal | Factory PIN (SHA-256), brute-force lockout | ✅ `WeighingState.h` |
| Frontend UI | 7-tab interface (Info, Config, Cal, REST, Live, Audit, BLE) | ✅ `interface/src/examples/weighing/` |
| Version service | OIML 6.2.1 software identification | ✅ `VersionService.h/.cpp` |
| OIML compliance | Event counter, audit trail, seal, defect detection | ✅ See compliance matrix |

---

## GPIO Pin Summary

| GPIO | Signal | Direction | Used By |
|------|--------|-----------|---------|
| GPIO19 | SPEED | Output | ADS1231 data rate selection |
| GPIO22 | PDWN | Output | ADS1231 power down control |
| GPIO23 | SCLK | Output | ADS1231 serial clock |
| GPIO25 | DOUT | Input | ADS1231 data / data ready |
| GPIO16 | Serial2 RX | Input | Available (serial branch) |
| GPIO17 | Serial2 TX | Output | Available (serial branch) |

No pin conflicts. All ADS1231 pins are dedicated on the PCB.

---

## Related Documentation

- **[WEIGHING-EXAMPLE.md](WEIGHING-EXAMPLE.md)** — Full user guide, API reference, type approval guide
- **[SERIAL-EXAMPLE.md](SERIAL-EXAMPLE.md)** — UART monitoring approach (alternative to direct ADC)
- **[API-REFERENCE.md](API-REFERENCE.md)** — All REST, WebSocket, MQTT, BLE endpoints
- **[PIN-CONFIGURATION.md](PIN-CONFIGURATION.md)** — Complete GPIO pin reference
- **ADS1231 Datasheet** — [TI Official](https://www.ti.com/lit/ds/symlink/ads1231.pdf)
