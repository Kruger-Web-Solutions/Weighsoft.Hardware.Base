# Serial Monitor Example

## Overview

The Serial Monitor service provides real-time monitoring and streaming of serial data from Serial2 (GPIO17/18 on ESP32-S3, GPIO16/17 on ESP32). It demonstrates the single-layer architecture pattern by composing REST, WebSocket, MQTT, and BLE communication channels directly within the service.

## Features

- **Line-Based Reading**: Reads complete lines from Serial2 (up to 512 characters)
- **Configurable Serial Port**: Baud rate, data bits (7/8), stop bits, parity (None/Even/Odd) via UI or REST
- **Weight Extraction**: Optional regex pattern with one capture group extracts a value (e.g. weight) from each line; full line and extracted value are both transmitted
- **Multi-Channel Broadcasting**: Automatically streams data (last_line and weight) across all enabled channels
- **REST API**: GET last line and weight; POST to update serial and regex configuration
- **WebSocket**: Real-time streaming of all incoming lines and extracted weights
- **MQTT Publishing**: Publishes each line and weight to a configurable topic
- **BLE Notifications**: Broadcasts data via Bluetooth Low Energy

## Hardware Setup

### Wiring Diagram

```
ESP32 Pin          Serial Device (Scale)
---------          ---------------------
GPIO17 (RX2)  <--  TX
GPIO18 (TX2)  -->  RX (not currently used)
GND           ---  GND
```

### Important Notes

1. **Serial2 vs Serial**: This service uses Serial2 (hardware UART1) to avoid conflicts with the debug Serial (UART0)
2. **Voltage Levels**: ESP32 operates at 3.3V - use a level shifter if connecting to 5V devices
3. **Baud Rate**: Default is 115200, but **many scales use 9600 baud**. Use the Configuration tab or REST API to change if no data appears
4. **Common Scale Formats**: Most scales output lines like `WN0001.68kg` or `Weight: 12.5 kg` with newline endings

## Architecture

The Serial service follows the **single-layer pattern**:

```
Serial2 Hardware
      |
      v
 SerialService
      |
      +-- HttpEndpoint (REST)
      +-- WebSocketTxRx (WS)
      +-- MqttPubSub (MQTT)
      +-- BlePubSub (BLE)
```

All protocol handlers are composed directly within the service, with inline configuration. No separate settings services are needed.

## Backend Implementation

### File Structure

Reference: @src/examples/serial/

```
src/examples/serial/
├── SerialState.h         # State structure (last_line, weight, timestamp, config)
├── SerialService.h       # Service header with protocol composition
└── SerialService.cpp     # Implementation with Serial2 reading and regex extraction
```

### Key Components

#### SerialState

```cpp
class SerialState {
 public:
  String lastLine;      // Full line from serial
  String weight;        // Extracted value (first capture group) or empty
  unsigned long timestamp;
  uint32_t baudrate;    // 9600, 115200, etc.
  uint8_t databits;     // 7 or 8
  uint8_t stopbits;     // 1 or 2
  uint8_t parity;       // 0=None, 1=Even, 2=Odd
  String regexPattern; // e.g. "(\\d+\\.?\\d*)"
  static void read(SerialState& state, JsonObject& root);
  static StateUpdateResult update(JsonObject& root, SerialState& state);
};
```

- **lastLine** / **weight**: Last received line and extracted weight (empty if regex does not match)
- **timestamp**: millis() when the line was received
- **Configuration**: baud_rate, data_bits, stop_bits, parity, regex_pattern can be updated via REST POST; changes trigger Serial2 reconfiguration

#### SerialService

**Protocol Composition**:

- `HttpEndpoint<SerialState>` - REST API at `/rest/serial`
- `WebSocketTxRx<SerialState>` - WebSocket at `/ws/serial`
- `MqttPubSub<SerialState>` - MQTT topic: `weighsoft/serial/{unique_id}/data`
- `BlePubSub<SerialState>` - BLE service/characteristic UUIDs defined inline

**Line Reading Logic**:

```cpp
void SerialService::readSerial() {
  while (Serial2.available()) {
    char c = Serial2.read();
    
    if (c == '\n') {
      // Process complete line
      update([&](SerialState& state) {
        state.lastLine = _lineBuffer;
        state.timestamp = millis();
        return StateUpdateResult::CHANGED;
      }, "serial_hw");
      _lineBuffer = "";
    } else if (c != '\r') {
      _lineBuffer += c;
      if (_lineBuffer.length() > 512) {
        _lineBuffer = "";  // Safety: discard oversized lines
      }
    }
  }
}
```

**Safety Features**:

- Maximum line length: 512 characters
- Ignores carriage returns (`\r`)
- Origin tracking prevents feedback loops
- Non-blocking read in main loop

## Frontend Implementation

### File Structure

Reference: @interface/src/examples/serial/

```
interface/src/examples/serial/
├── SerialMonitor.tsx      # Main router with tabs
├── SerialInfo.tsx         # Information/documentation page
├── SerialConfig.tsx       # Serial port and regex configuration form
├── SerialRest.tsx         # REST polling view
├── SerialWebSocket.tsx    # Real-time streaming view
└── SerialBle.tsx          # BLE connection instructions
```

### UI Components

#### SerialInfo

- Overview of the service
- Feature list
- Hardware wiring instructions
- Communication channel details

#### SerialRest

- Polls REST API every 2 seconds
- Displays last received line with timestamp
- Useful for low-frequency updates or testing

#### SerialWebSocket

- Real-time streaming display
- Keeps last 100 lines in memory
- Auto-scrolls to bottom
- Clear button to reset display
- Timestamped entries

#### SerialBle

- BLE connection instructions
- Service UUID: `12340000-e8f2-537e-4f6c-d104768a1234`
- Characteristic UUID: `12340001-e8f2-537e-4f6c-d104768a1234`
- Step-by-step guide for nRF Connect

## API Reference

### REST Endpoint

**GET** `/rest/serial`

Returns the last received serial line, extracted weight, and current configuration.

**Response**:

```json
{
  "last_line": "Weight: 12.5 kg",
  "weight": "12.5",
  "timestamp": 12345678,
  "baud_rate": 115200,
  "data_bits": 8,
  "stop_bits": 1,
  "parity": 0,
  "regex_pattern": "(\\d+\\.\\d+)"
}
```

**Fields**:

- `last_line` (string): Most recently received complete line
- `weight` (string): Extracted value from first regex capture group, or empty
- `timestamp` (number): ESP32 millis() when line was received
- `baud_rate`, `data_bits`, `stop_bits`, `parity`, `regex_pattern`: Serial and extraction configuration

**POST** `/rest/serial`

Update serial port and regex configuration. Request body can include any of: `baud_rate`, `data_bits`, `stop_bits`, `parity`, `regex_pattern`. Serial2 is reconfigured immediately.

### WebSocket Endpoint

**Path**: `/ws/serial`

Streams real-time data as lines arrive.

**Message Format**:

```json
{
  "last_line": "Weight: 12.5 kg",
  "weight": "12.5",
  "timestamp": 12345678
}
```

**Connection**:

```typescript
const ws = new WebSocket('ws://192.168.x.x/ws/serial');
ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log(data.last_line, data.timestamp);
};
```

### MQTT Topic

**Publish Topic**: `weighsoft/serial/{unique_id}/data`

Where `{unique_id}` is the device's unique identifier (MAC-based).

**Payload**:

```json
{
  "last_line": "Weight: 12.5 kg",
  "weight": "12.5",
  "timestamp": 12345678
}
```

**Subscribe Example** (mosquitto_sub):

```bash
mosquitto_sub -h broker.example.com -t "weighsoft/serial/+/data"
```

### BLE Service

**Service UUID**: `12340000-e8f2-537e-4f6c-d104768a1234`  
**Characteristic UUID**: `12340001-e8f2-537e-4f6c-d104768a1234`

**Properties**: READ, NOTIFY

**Value Format**: UTF-8 JSON string

```json
{"last_line":"Weight: 12.5 kg","weight":"12.5","timestamp":12345678}
```

## Testing

### 1. Backend Testing

#### Test Serial2 Input

Connect a USB-to-TTL adapter:

```
Adapter TX --> ESP32 GPIO17 (RX2)
Adapter GND --> ESP32 GND
```

Send test data:

```bash
# Using screen (Linux/Mac)
screen /dev/ttyUSB0 115200

# Using PuTTY (Windows)
# Connect to COM port at 115200 baud
```

Type lines and press Enter - they should appear in all channels.

#### Verify REST API

```bash
curl http://192.168.x.x/rest/serial
```

Expected response:

```json
{"last_line":"test data","timestamp":12345}
```

#### Verify WebSocket

Use browser console or wscat:

```bash
wscat -c ws://192.168.x.x/ws/serial
```

#### Verify MQTT

```bash
mosquitto_sub -h your-broker -t "weighsoft/serial/+/data" -v
```

#### Verify BLE

1. Open nRF Connect app
2. Scan for ESP32 device
3. Connect and find service `12340000-e8f2-537e-4f6c-d104768a1234`
4. Enable notifications on characteristic
5. Send serial data, verify notifications appear

### 2. Frontend Testing

1. **Navigation**: Verify "Serial Monitor" appears in Project menu
2. **Information Tab**: Check documentation renders correctly
3. **REST View**: Verify 2-second polling shows last line
4. **Live Stream**:
   - Verify WebSocket connection establishes
   - Lines appear in real-time
   - Timestamps are correct
   - Clear button works
   - Auto-scroll works
5. **BLE Tab**: Verify instructions are clear and UUIDs match backend

### 3. Integration Testing

Send a continuous stream of test data:

```bash
# Linux/Mac
while true; do echo "Test line $(date +%s)"; sleep 1; done > /dev/ttyUSB0
```

Verify:

- REST API returns most recent line
- WebSocket streams all lines
- MQTT publishes all lines
- BLE notifies all lines
- No memory leaks (check free heap over time)
- No line corruption or truncation

## Use Cases

### GPS Module Integration

**Hardware**: GPS module with NMEA output (TX only)

**Wiring**:

```
GPS Module TX --> ESP32 GPIO17 (RX2)
GPS Module GND --> ESP32 GND
GPS Module VCC --> ESP32 3.3V
```

**Expected Data**:

```
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
```

Each NMEA sentence will be streamed across all channels.

### Sensor Logger

**Hardware**: Arduino/Sensor board sending CSV data

**Example Data**:

```
timestamp,temp,humidity,pressure
1234567890,23.5,65.2,1013.25
1234567891,23.6,65.1,1013.24
```

Parse lines on client side (WebSocket/MQTT subscriber).

### Debug Monitor

Monitor debug output from another microcontroller:

```
[DEBUG] Sensor initialized
[INFO] Reading value: 42
[WARN] Temperature high: 75°C
[ERROR] Communication timeout
```

View real-time logs via WebSocket in browser.

## Troubleshooting

### No Data Received

1. **Check Wiring**: Verify TX->RX connection and common ground
2. **Baud Rate Mismatch**: Most scales use **9600 baud**, not 115200. Change in Configuration tab
3. **Check Scale Output**: Connect scale to PC/Raspberry Pi with `cat /dev/ttyUSB0` to verify it's sending data
4. **Voltage Levels**: Use level shifter for 5V devices
5. **Serial Monitor**: Close Arduino IDE Serial Monitor (conflicts with Serial2)
6. **Check Debug Logs**: Monitor Serial output for `[Serial] Received line:` messages

### Lines Truncated

- Default limit is 512 characters per line
- Increase in `SerialService.cpp`:

  ```cpp
  if (_lineBuffer.length() > 1024) {  // Increase from 512
  ```

### High CPU Usage

If receiving data at very high rates (>1000 lines/sec):

- Add rate limiting in `readSerial()`
- Increase buffer size
- Consider hardware flow control

### BLE Not Visible

1. Verify BLE is enabled: Settings -> BLE Settings -> Enabled
2. Check device name in WiFi Status
3. Ensure BLE server started (check serial monitor logs)
4. Restart BLE advertising if needed

## Configuration

### Serial port and regex (UI or REST)

Use the **Configuration** tab in the Serial project, or **POST** to `/rest/serial` with JSON body:

- **baud_rate**: 300–2000000 (e.g. 9600, 115200)
- **data_bits**: 7 or 8 (5 and 6 are mapped to 7 on ESP32)
- **stop_bits**: 1 or 2
- **parity**: 0 = None, 1 = Even, 2 = Odd
- **regex_pattern**: Optional. First capture group is used as `weight`. Examples:
  - `(\d+\.?\d*)` – first decimal or integer number
  - `Weight:\s*(\d+\.?\d*)` – value after "Weight: "
  - `(\d+\.\d+)\s*kg` – number before " kg"

If the pattern does not match a line, `last_line` is still sent and `weight` is empty.

### Change GPIO Pins

Serial2 uses fixed pins on ESP32 (16/17). To use different pins:

1. Use `Serial1` instead (GPIO43/44) - hardware UART0 on ESP32-S3
2. Use SoftwareSerial library (not recommended for high speeds)
3. Remap UART pins (advanced, requires IDF configuration)

### Change Line Ending

Default is newline (`\n`). To handle different endings:

```cpp
void SerialService::readSerial() {
  while (Serial2.available()) {
    char c = Serial2.read();
    
    if (c == '\n' || c == '\r') {  // Accept either
      if (_lineBuffer.length() > 0) {
        // Process line
      }
      _lineBuffer = "";
    } else {
      _lineBuffer += c;
    }
  }
}
```

## Performance

### Throughput

- **Max Baud Rate**: 115200 (default), up to 2 Mbps possible
- **Common Scale Baud**: Most industrial scales use **9600 baud**
- **Max Line Rate**: ~1000 lines/sec (100 char average)
- **Latency**: <10ms from serial RX to WebSocket TX

### Memory Usage

- **Line Buffer**: 512 bytes max
- **State**: ~550 bytes (String + timestamp + overhead)
- **Per Channel**: ~200-500 bytes (varies by protocol)
- **Total Impact**: ~2-3 KB

## Common Scale Formats

### Weighing Scale Output Examples

**Format: `WN0001.68kg`** (common in industrial scales)

- Regex pattern: `(\d+\.\d+)` → extracts `0001.68`
- Regex pattern: `WN(\d+\.\d+)` → extracts `0001.68`
- Typical baud: 9600

**Format: `Weight: 12.5 kg`**

- Regex pattern: `(\d+\.\d+)` → extracts `12.5`
- Regex pattern: `Weight:\s*(\d+\.\d+)` → extracts `12.5`
- Typical baud: 9600 or 115200

**Format: `ST,GS,+00012.5kg`** (A&D scales)

- Regex pattern: `([+-]?\d+\.\d+)` → extracts `+00012.5`
- Typical baud: 9600

**Testing your scale:**

```bash
# On Raspberry Pi or Linux PC
cat /dev/ttyUSB0

# Should show continuous output like:
WN0001.68kg
WN0001.68kg
```

## Related Documentation

- [LED Example](LED-EXAMPLE.md) - Reference implementation
- [Design Patterns](DESIGN-PATTERNS.md) - Single-layer architecture
- [Extension Guide](EXTENSION-GUIDE.md) - Creating new services
- [Architecture](ARCHITECTURE.md) - Framework overview

## Future Enhancements

Possible additions (not yet implemented):

1. **Bidirectional Communication**: Send commands via TX (GPIO18)
2. **Line Filtering**: Regex patterns to filter which lines are broadcast
3. **Statistics**: Line count, bytes received, error count
4. **Buffering**: Store last N lines for late-joining clients
5. **Binary Mode**: Support binary protocols (not just text lines)
6. **Multi-Port**: Monitor Serial, Serial1, and Serial2 simultaneously
