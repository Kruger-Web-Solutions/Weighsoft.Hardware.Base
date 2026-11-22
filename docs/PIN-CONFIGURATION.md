# GPIO Pin Configuration

## Overview

This document explains the UART Serial1 GPIO pin assignments for different ESP32 platforms and how to change them if needed.

## Current Pin Assignments

### ESP32-S3 (esp32s3 environment)

| Signal | GPIO | Purpose |
|--------|------|---------|
| RX | **GPIO18** | Receives data from external device TX (U1RXD) |
| TX | **GPIO17** | Transmits data to external device RX (U1TXD) |
| GND | GND | Common ground (required) |

**Why GPIO17/18?**

- Hardware UART1 pins (U1RXD/U1TXD) on ESP32-S3
- Won't conflict with USB Serial monitor on GPIO43/44

### ESP32 (node32s / esp32dev environments)

| Signal | GPIO | Purpose |
|--------|------|---------|
| RX | **GPIO18** | Receives data from external device TX (same product wiring as ESP32-S3) |
| TX | **GPIO17** | Transmits data to external device RX |
| GND | GND | Common ground (required) |

**Why GPIO17/18 on classic ESP32 too?**

- **Product standard** in this repository: one bench harness and one firmware pin map for **all** ESP32 targets (`SerialService.h` / `DiagnosticsService.h`).
- Wire **scale TX → GPIO18**, **scale RX → GPIO17**, **GND** common on **node32s**, **esp32dev**, and **esp32s3**.

## How to Change Serial1 Pins

If you need to use different GPIO pins (e.g., hardware conflict, PCB design), update **two header files**:

### 1. SerialService Pin Definitions

**File:** `src/examples/serial/SerialService.h`

```cpp
// ESP32-S3: GPIO18=U1RXD (RX), GPIO17=U1TXD (TX) - must match hardware UART1 defaults
#define SERIAL_PORT Serial1
#define SERIAL2_RX_PIN 18  // Change this
#define SERIAL2_TX_PIN 17  // Change this
```

### 2. DiagnosticsService Pin Definitions

**File:** `src/examples/diagnostics/DiagnosticsService.h`

```cpp
// ESP32-S3: GPIO18=U1RXD (RX), GPIO17=U1TXD (TX) - must match hardware UART1 defaults
#define DIAG_SERIAL_PORT Serial1
#define DIAG_RX_PIN 18  // Must match SERIAL2_RX_PIN
#define DIAG_TX_PIN 17  // Must match SERIAL2_TX_PIN
```

**IMPORTANT:** Both files MUST use the same pins - SerialService and DiagnosticsService share the Serial1 hardware.

### 3. Update All Hardcoded References

After changing the header files, update **hardcoded GPIO numbers** in:

#### Backend C++ Files

- `src/examples/diagnostics/DiagnosticsService.cpp`
  - Line 74: `Serial.println(F("[Diagnostics] Ready. GPIO17 (RX) / GPIO18 (TX)"));`
  - Line 157: `Serial.printf("[Diagnostics] Serial2 started: %lu baud, GPIO17 (RX), GPIO18 (TX)...`
  - Line 239: `Serial.println(F("[Diagnostics]   Device TX -> ESP32 GPIO17 (RX2)"));`
  
- `src/UartMode.h`
  - Line 6: Comment explaining GPIO pins

#### Frontend TypeScript Files

- `interface/src/components/UartModeSwitcher.tsx`
  - Line 45: Info message about Serial2 pins
  
- `interface/src/examples/serial/SerialInfo.tsx`
  - Line 9: Service description
  - Lines 74, 80: GPIO pin labels in Hardware Setup list
  
- `interface/src/examples/diagnostics/DiagnosticsInfo.tsx`
  - Line 12: Service description
  - Lines 27, 31, 47, 63: Test setup instructions
  - Lines 103, 109: GPIO pin labels in Hardware Pins list
  
- `interface/src/examples/diagnostics/LoopbackTest.tsx`
  - Lines 67-74: Hardware setup instructions
  - Lines 211-213: Troubleshooting steps
  - Line 227: Initial state message
  
- `interface/src/examples/diagnostics/BaudDetector.tsx`
  - Lines 64-65: Setup options
  - Line 176: Wiring troubleshooting
  
- `interface/src/examples/diagnostics/SignalQuality.tsx`
  - Line 68: Setup instructions
  - Line 291: Initial state message

#### Documentation Markdown Files

- `docs/DIAGNOSTICS-EXAMPLE.md`
  - Line 5: Service overview
  - Lines 11, 25-26: Hardware specs
  - Lines 34, 39, 261, 294, 354, 360, 366, 390, 408, 418: Setup/wiring instructions
  - Lines 473-474, 510, 525: Technical details
  
- `docs/SERIAL-EXAMPLE.md`
  - Line 5: Service overview
  - Lines 25-26: Wiring diagram
  - Lines 267, 347, 519: Setup examples

### 4. Rebuild and Deploy

After updating all references:

```bash
# For USB upload
python -m platformio run -t upload -e esp32s3 --upload-port COM10

# For OTA upload (if already flashed once)
python -m platformio run -t upload -e esp32s3
```

## GPIO Pin Selection Guidelines

### Safe Pins on ESP32-S3

✅ **Recommended for UART:**

- GPIO17, GPIO18 (used in this project - hardware UART1)
- GPIO19, GPIO20
- GPIO21, GPIO47

⚠️ **Avoid These:**

- GPIO0, GPIO3, GPIO45, GPIO46 (strapping pins - affect boot mode)
- GPIO26-GPIO32 (connected to SPI flash/PSRAM on some modules)
- GPIO33-GPIO37 (used by Octal flash/PSRAM if present)
- GPIO19, GPIO20 (USB if USB CDC is enabled)

### Safe Pins on ESP32 (Classic)

✅ **Recommended for UART:**

- GPIO17, GPIO18 (used in this project for Serial1 scale RX/TX)
- GPIO16, GPIO17 (common Serial2 default if you fork to different pins)
- GPIO25, GPIO26
- GPIO32, GPIO33

⚠️ **Avoid These:**

- GPIO0, GPIO2, GPIO5, GPIO12, GPIO15 (strapping pins)
- GPIO6-GPIO11 (connected to SPI flash)
- GPIO34-GPIO39 (input only, can't be used for TX)

## Troubleshooting Pin Changes

If Serial1 doesn't work after changing pins:

1. **Check Pin Capability:**
   - Input-only pins (ESP32: GPIO34-39) cannot be used for TX
   - Strapping pins may cause boot issues if pulled high/low at boot

2. **Verify Wiring:**
   - External device TX → ESP32 RX pin
   - External device RX → ESP32 TX pin  
   - Common GND (critical!)

3. **Check for Conflicts:**
   - No other peripherals (SPI, I2C, LED) using the same GPIOs
   - No pull-up/pull-down resistors interfering with signal

4. **Test with Loopback:**
   - Connect RX to TX directly (jumper wire)
   - Run loopback diagnostic test
   - If this fails, the pin selection or hardware has issues

## Next Steps

- @docs/DIAGNOSTICS-EXAMPLE.md - Diagnostic testing guide
- @docs/SERIAL-EXAMPLE.md - Serial monitoring guide
- @docs/ARCHITECTURE.md - System architecture
