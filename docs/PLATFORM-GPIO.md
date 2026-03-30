# Platform-Specific GPIO Reference

## Overview

Different ESP32 platforms have different GPIO mappings for built-in peripherals like LEDs and UARTs. This document provides a reference for the most common GPIO assignments across platforms used in this project.

## Built-in LED Pins

| Platform | Board | LED GPIO | Type | Notes |
|----------|-------|----------|------|-------|
| ESP32-S3 | ESP32-S3-DevKitC-1 | **GPIO48** | RGB NeoPixel (WS2812) | Requires `neopixelWrite()` function |
| ESP32 | ESP32-DevKitC, NodeMCU-32S | **GPIO2** | Simple LED | Blue LED (active HIGH) |
| ESP8266 | NodeMCU v2/v3 | **GPIO2** | Simple LED | Blue LED (active LOW) |

### LED Control Methods

**ESP32-S3 (RGB NeoPixel):**

```cpp
// Use neopixelWrite() for RGB control
neopixelWrite(48, red, green, blue);  // red, green, blue = 0-255

// Turn off
neopixelWrite(48, 0, 0, 0);

// White
neopixelWrite(48, 255, 255, 255);

// Red
neopixelWrite(48, 255, 0, 0);
```

**ESP32 (Simple LED):**

```cpp
// Use digitalWrite() for on/off control
digitalWrite(2, HIGH);  // LED ON
digitalWrite(2, LOW);   // LED OFF
```

**ESP8266 (Simple LED - Inverted):**

```cpp
// Inverted logic - LOW = ON, HIGH = OFF
digitalWrite(2, LOW);   // LED ON
digitalWrite(2, HIGH);  // LED OFF
```

### LED Logic Levels

**ESP32 / ESP32-S3:**

- `HIGH` (1) = LED ON
- `LOW` (0) = LED OFF

**ESP8266:**

- `HIGH` (1) = LED OFF (inverted)
- `LOW` (0) = LED ON

## UART Pin Mappings

### Serial0 (USB/Debug)

**All Platforms:**

- TX: GPIO1 (usually reserved for USB serial)
- RX: GPIO3 (usually reserved for USB serial)

**Note:** Serial0 is typically used for debugging and should not be used for external devices.

### Serial1 (External UART)

| Platform | RX Pin | TX Pin | Notes |
|----------|--------|--------|-------|
| ESP32-S3 | **GPIO18** | **GPIO17** | Configurable, used for scale communication |
| ESP32 | **GPIO16** | **GPIO17** | Configurable, used for scale communication |
| ESP8266 | TX only (GPIO2) | No RX | Limited UART support |

## Platform Detection in Code

Use preprocessor directives to conditionally compile platform-specific code:

### ESP32-S3 Detection (RGB NeoPixel)

```cpp
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  // ESP32-S3 specific code - RGB NeoPixel
  #define LED_PIN 48
  #define HAS_RGB_LED true
  
  // Control RGB LED
  void setLED(bool on, uint8_t r, uint8_t g, uint8_t b) {
    if (on) {
      neopixelWrite(LED_PIN, r, g, b);
    } else {
      neopixelWrite(LED_PIN, 0, 0, 0);
    }
  }
#endif
```

### ESP32 Detection

```cpp
#if defined(ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S3)
  // ESP32 (non-S3) specific code
  #define LED_PIN 2
  #define UART_RX_PIN 16
  #define UART_TX_PIN 17
#endif
```

### ESP8266 Detection

```cpp
#if defined(ESP8266)
  // ESP8266 specific code
  #define LED_PIN 2
  #define LED_ON 0x0  // Inverted logic
  #define LED_OFF 0x1
#endif
```

## Example: Platform-Agnostic LED Control with RGB Support

```cpp
// LedExampleService.h
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define LED_PIN 48
  #define HAS_RGB_LED true
#else
  #define LED_PIN 2
  #define HAS_RGB_LED false
#endif

// LedExampleService.cpp
void LedExampleService::onConfigUpdated() {
  #if defined(CONFIG_IDF_TARGET_ESP32S3)
  // ESP32-S3: Use neopixelWrite for RGB control
  if (_state.ledOn) {
    neopixelWrite(LED_PIN, _state.red, _state.green, _state.blue);
  } else {
    neopixelWrite(LED_PIN, 0, 0, 0);  // Off
  }
  #else
  // ESP32/ESP8266: Use digitalWrite for simple LED
  #ifdef ESP32
  digitalWrite(LED_PIN, _state.ledOn ? HIGH : LOW);
  #elif defined(ESP8266)
  digitalWrite(LED_PIN, _state.ledOn ? LOW : HIGH);  // Inverted
  #endif
  #endif
}
```

## Hardware Capabilities Comparison

| Feature | ESP8266 | ESP32 | ESP32-S3 |
|---------|---------|-------|----------|
| Flash | 4MB | 4-16MB | 4-16MB |
| RAM | 80KB | 520KB | 512KB |
| PSRAM | No | Optional | Optional (8MB on-board) |
| CPU Cores | 1 | 2 | 2 |
| WiFi | 2.4GHz | 2.4GHz | 2.4GHz |
| Bluetooth | No | Classic + BLE | BLE 5.0 |
| USB | No | No | **Native USB** |
| UART | 1.5 (TX only on Serial1) | 3 | 3 |
| GPIO Pins | ~17 usable | ~34 usable | ~45 usable |
| ADC | 1x 10-bit | 2x 12-bit | 2x 12-bit |

## Common Pitfalls

### 1. LED Not Working on ESP32-S3

**Problem:** Code compiled for ESP32 uses GPIO2 and `digitalWrite()`, but ESP32-S3 has RGB NeoPixel on GPIO48 requiring `neopixelWrite()`.

**Solution:** Use platform detection and appropriate control method:

```cpp
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define LED_PIN 48
  #define HAS_RGB_LED true
  
  // RGB NeoPixel control
  void setLED(bool on, uint8_t r, uint8_t g, uint8_t b) {
    if (on) {
      neopixelWrite(LED_PIN, r, g, b);
    } else {
      neopixelWrite(LED_PIN, 0, 0, 0);
    }
  }
#else
  #define LED_PIN 2
  #define HAS_RGB_LED false
  
  // Simple LED control
  void setLED(bool on) {
    digitalWrite(LED_PIN, on ? HIGH : LOW);
  }
#endif
```

### 2. UART Not Working on ESP32-S3

**Problem:** Code uses GPIO16/17 (ESP32), but ESP32-S3 needs GPIO17/18.

**Solution:** Update SerialService to use correct pins:

```cpp
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define UART_RX_PIN 18
  #define UART_TX_PIN 17
#else
  #define UART_RX_PIN 16
  #define UART_TX_PIN 17
#endif
```

### 3. Inverted LED Logic on ESP8266

**Problem:** LED is ON when it should be OFF and vice versa.

**Solution:** Use platform-specific logic levels:

```cpp
#ifdef ESP32
  digitalWrite(LED_PIN, state ? HIGH : LOW);
#elif defined(ESP8266)
  digitalWrite(LED_PIN, state ? LOW : HIGH);  // Inverted
#endif
```

## USB Differences

### ESP32-S3 Native USB

**Advantage:** No USB-to-UART chip needed for programming/debugging

**Configuration:**

```ini
; platformio.ini
build_flags =
  ; Use hardware UART0 for serial, not USB CDC
  -DARDUINO_USB_CDC_ON_BOOT=0
  -DARDUINO_USB_MODE=0
```

**Why disable USB CDC?**

- Serial monitor over USB CDC can cause conflicts with Serial0 debugging
- Hardware UART0 is more reliable for serial debugging
- USB is still available for programming via esptool

## Partition Scheme Differences

| Platform | Flash Size | Partition Scheme | OTA Support | Notes |
|----------|------------|------------------|-------------|-------|
| ESP8266 | 4MB | `min_spiffs.csv` | ✅ Yes | 2x 1.9MB app slots |
| ESP32 (node32s) | 4MB | `partitions_ble.csv` | ❌ No | Single 3.3MB app (BLE enabled) |
| ESP32-S3 | 8MB | `partitions_ble_ota.csv` | ✅ Yes | 2x 2.5MB app slots (BLE + OTA) |

**Why no OTA on 4MB ESP32 with BLE?**

- Firmware with BLE = ~2.2MB
- OTA needs 2 app slots = 4.4MB minimum
- 4MB flash can't fit dual slots

**Solution:** Use 8MB ESP32-S3 for BLE + OTA support.

## Pin Strapping Warnings

Some GPIO pins have special boot functions and should be used carefully:

| Platform | Strapping Pins | Warning |
|----------|----------------|---------|
| ESP32 | GPIO0, GPIO2, GPIO5, GPIO12, GPIO15 | Avoid for critical functions |
| ESP32-S3 | GPIO0, GPIO3, GPIO45, GPIO46 | Avoid for critical functions |
| ESP8266 | GPIO0, GPIO2, GPIO15 | Boot mode pins |

**Safe GPIO for General Use:**

- ESP32: GPIO4, GPIO16-23, GPIO25-27, GPIO32-33
- ESP32-S3: GPIO1-14, GPIO17-21, GPIO38-48

## Quick Reference Table

| Feature | GPIO Pin(s) | ESP8266 | ESP32 | ESP32-S3 |
|---------|-------------|---------|-------|----------|
| Built-in LED | | GPIO2 | GPIO2 | **GPIO48** |
| Serial Monitor | RX/TX | GPIO3/1 | GPIO3/1 | GPIO3/1 |
| External UART | RX/TX | N/A | GPIO16/17 | **GPIO18/17** |
| I2C (SDA/SCL) | | GPIO4/5 | GPIO21/22 | GPIO8/9 |
| SPI (MOSI/MISO/SCK/CS) | | GPIO13/12/14/15 | GPIO23/19/18/5 | GPIO11/13/12/10 |

## Testing Platform-Specific Code

### 1. Test RGB NeoPixel on ESP32-S3

```cpp
void testRgbLed() {
  #if defined(CONFIG_IDF_TARGET_ESP32S3)
  Serial.println("Testing RGB NeoPixel on GPIO48");
  
  // Red
  neopixelWrite(48, 255, 0, 0);
  delay(1000);
  
  // Green
  neopixelWrite(48, 0, 255, 0);
  delay(1000);
  
  // Blue
  neopixelWrite(48, 0, 0, 255);
  delay(1000);
  
  // White
  neopixelWrite(48, 255, 255, 255);
  delay(1000);
  
  // Off
  neopixelWrite(48, 0, 0, 0);
  
  Serial.println("RGB NeoPixel test complete");
  #else
  Serial.println("RGB NeoPixel not available on this platform");
  #endif
}
```

### 2. Test Simple LED on All Platforms

```cpp
void testLed() {
  Serial.printf("Testing LED on GPIO%d\n", LED_PIN);
  
  digitalWrite(LED_PIN, LED_ON);
  delay(1000);
  
  digitalWrite(LED_PIN, LED_OFF);
  delay(1000);
  
  Serial.println("LED test complete");
}
```### 3. Test UART on All Platforms

```cpp
void testUart() {
  Serial.printf("Testing UART: RX=GPIO%d, TX=GPIO%d\n", UART_RX_PIN, UART_TX_PIN);
  
  Serial1.begin(9600, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  Serial1.println("UART test message");
  
  Serial.println("UART test complete");
}
```

## Version History

- **2026-02-17**: Initial documentation created after fixing ESP32-S3 LED issue
