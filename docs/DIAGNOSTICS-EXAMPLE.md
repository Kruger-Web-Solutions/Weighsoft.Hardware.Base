# UART Diagnostics Example

## Overview

The UART Diagnostics service provides comprehensive hardware testing tools for Serial2 (GPIO17/18 on ESP32-S3, GPIO16/17 on ESP32). It helps troubleshoot serial connectivity issues by testing the hardware functionality, detecting baud rates, and measuring signal quality before debugging scale wiring or software.

This service follows **SOLID principles**, specifically the **Single Responsibility Principle**, by maintaining a clear separation of concerns: SerialService handles external device communication, while DiagnosticsService handles hardware testing and validation.

## Features

- **Loopback Test**: Verifies GPIO17/18 hardware by sending data from TX to RX (requires jumper wire)
- **Baud Rate Detection**: Automatically detects device baud rate by testing common values (1200-115200)
- **Signal Quality Analysis**: Measures connection reliability, latency, jitter, and packet loss
- **Real-Time Results**: WebSocket streaming provides live updates during tests
- **REST API**: GET test results, POST to start/stop tests
- **WebSocket-Only**: Diagnostics uses WebSocket for real-time updates (no MQTT/BLE)

## Hardware Setup

### GPIO Pins

```
ESP32 Pin          Function
---------          --------
GPIO17 (RX2)       Receives data (Serial2)
GPIO18 (TX2)       Transmits data (Serial2)
GND                Common ground
```

### Test-Specific Wiring

**Loopback Test & Signal Quality Test:**

```
GPIO17 (RX) ←→ GPIO18 (TX)  (jumper wire)
```

**Baud Rate Detection:**

```
Scale/Device TX → GPIO17 (RX)
Scale/Device GND → GND
```

## Architecture

The Diagnostics service follows the **single-layer pattern** with WebSocket-only communication:

```
Serial2 Hardware
      |
      v
DiagnosticsService
      |
      +-- HttpEndpoint (REST for control)
      +-- WebSocketTxRx (real-time results)
```

Unlike SerialService, DiagnosticsService does not include MQTT or BLE because diagnostics are interactive tests requiring real-time feedback through the UI.

## Backend Implementation

### File Structure

Reference: @src/examples/diagnostics/

```
src/examples/diagnostics/
├── DiagnosticsState.h       # State structure (loopback, baud_scan, signal_quality)
├── DiagnosticsService.h     # Service header with test methods
└── DiagnosticsService.cpp   # Test implementations (loopback, baud scan, signal test)
```

### Key Components

#### DiagnosticsState

```cpp
class DiagnosticsState {
 public:
  // Loopback Test State
  bool loopbackEnabled;
  String loopbackStatus;        // "idle", "running", "pass", "fail"
  uint32_t loopbackTxCount;
  uint32_t loopbackRxCount;
  uint32_t loopbackErrorCount;
  String loopbackLastTest;
  String loopbackLastReceived;
  unsigned long loopbackStartTime;

  // Baud Rate Detection State
  bool baudScanEnabled;
  String baudScanStatus;        // "idle", "scanning", "found", "not_found"
  uint32_t baudDetected;
  uint8_t baudCurrentIndex;
  uint32_t baudTestPackets;

  // Signal Quality State
  bool signalTestEnabled;
  String signalStatus;          // "idle", "running", "complete"
  uint8_t signalQuality;        // 0-100%
  uint32_t signalTotalPackets;
  uint32_t signalSentPackets;
  uint32_t signalReceivedPackets;
  float signalAvgLatency;
  float signalJitter;
  uint32_t signalErrorCount;
};
```

#### DiagnosticsService

**Protocol Composition**:

- `HttpEndpoint<DiagnosticsState>` - REST API at `/rest/diagnostics`
- `WebSocketTxRx<DiagnosticsState>` - WebSocket at `/ws/diagnostics`

**Test Methods**:

- `runLoopbackTest()` - Sends "TEST:123" packets at 100ms intervals, verifies echo
- `runBaudScan()` - Tests baud rates 1200-115200, waits 500ms per rate for data
- `runSignalQualityTest()` - Sends timestamped packets, calculates latency/jitter/quality

**Hardware Control**:

- `startSerial(baud)` - Configures and starts Serial2 at specified baud rate
- `stopSerial()` - Stops Serial2
- `readSerialLine()` - Reads line-based data with overflow protection

## API Reference

### REST Endpoints

#### GET `/rest/diagnostics`

Returns current test state.

**Response:**

```json
{
  "loopback": {
    "enabled": true,
    "status": "pass",
    "tx_count": 142,
    "rx_count": 142,
    "error_count": 0,
    "success_rate": 100.0,
    "last_test": "TEST:142",
    "last_received": "TEST:142",
    "uptime_seconds": 14
  },
  "baud_scan": {
    "enabled": false,
    "status": "found",
    "detected_baud": 9600,
    "current_index": 3,
    "current_baud": 9600,
    "test_packets": 5
  },
  "signal_quality": {
    "enabled": false,
    "status": "complete",
    "quality_percent": 98,
    "total_packets": 1000,
    "sent_packets": 1000,
    "received_packets": 980,
    "avg_latency_ms": 0.52,
    "jitter_ms": 0.08,
    "error_count": 20,
    "progress": 100.0
  }
}
```

#### POST `/rest/diagnostics`

Start/stop tests and configure parameters.

**Start Loopback Test:**

```json
{
  "loopback_enabled": true
}
```

**Stop Loopback Test:**

```json
{
  "loopback_enabled": false
}
```

**Start Baud Scan:**

```json
{
  "baud_scan_enabled": true
}
```

**Start Signal Quality Test:**

```json
{
  "signal_test_enabled": true,
  "signal_total_packets": 1000
}
```

### WebSocket Endpoint

**URL:** `ws://<device-ip>/ws/diagnostics`

Provides real-time test updates. Connect once and receive automatic updates as tests run.

**Message Format:** Same as REST GET response.

**Example Usage:**

```typescript
const ws = new WebSocket('ws://192.168.1.100/ws/diagnostics');
ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log('Loopback:', data.loopback.status);
  console.log('Success Rate:', data.loopback.success_rate + '%');
};
```

## Frontend Implementation

### File Structure

Reference: @interface/src/examples/diagnostics/

```
interface/src/examples/diagnostics/
├── Diagnostics.tsx           # Main router with tabs
├── DiagnosticsInfo.tsx       # Information and use cases
├── LoopbackTest.tsx          # Loopback test UI with live results
├── BaudDetector.tsx          # Baud rate scanner UI
└── SignalQuality.tsx         # Signal quality test UI
```

### Component Design

#### DiagnosticsInfo

- Overview of all available tests
- Hardware setup instructions
- Common use cases and troubleshooting

#### LoopbackTest

**UX Features:**

- Large START/STOP button
- Real-time success rate display (large, color-coded)
- Sent/Received/Errors metrics in cards
- Last test string comparison with match/mismatch indicator
- Troubleshooting alerts when test fails

**User Flow:**

1. Read hardware setup instructions (prominent alert)
2. Connect GPIO17 to GPIO18 with jumper wire
3. Click "Start Loopback Test"
4. Watch real-time metrics update
5. Verify 100% success rate (green) or troubleshoot failures (red)

#### BaudDetector

**UX Features:**

- Setup instructions for two modes (loopback OR scale)
- Progress bar with percentage
- Horizontal stepper showing current baud being tested
- Large "DETECTED: 9600 baud" success message
- "Apply to Serial Config" button for quick navigation
- Troubleshooting alerts when not found

**User Flow:**

1. Connect scale (must be transmitting) OR jumper wire
2. Click "Start Scan"
3. Watch progress stepper (1200 → 2400 → 4800 → ...)
4. Get detection result or troubleshooting tips

#### SignalQuality

**UX Features:**

- Packet count selector (100, 1000, 10000)
- Circular progress meter for quality percentage
- Color-coded quality ratings (EXCELLENT/GOOD/FAIR/POOR)
- Grid layout for metrics (latency, jitter, packet loss, errors)
- Test summary with detailed statistics
- Recommendations for poor quality

**User Flow:**

1. Select packet count (more = longer test = more accurate)
2. Connect GPIO17 to GPIO18
3. Click "Run Quality Test"
4. Watch progress bar and metrics update
5. Review quality rating and detailed metrics

### TypeScript Types

Reference: @interface/src/types/diagnostics.ts

```typescript
export interface LoopbackState {
  enabled: boolean;
  status: 'idle' | 'running' | 'pass' | 'fail';
  tx_count: number;
  rx_count: number;
  error_count: number;
  success_rate: number;
  last_test: string;
  last_received: string;
  uptime_seconds: number;
}

export interface BaudScanState {
  enabled: boolean;
  status: 'idle' | 'scanning' | 'found' | 'not_found';
  detected_baud: number;
  current_index: number;
  current_baud?: number;
  test_packets: number;
}

export interface SignalQualityState {
  enabled: boolean;
  status: 'idle' | 'running' | 'complete';
  quality_percent: number;
  total_packets: number;
  sent_packets: number;
  received_packets: number;
  avg_latency_ms: number;
  jitter_ms: number;
  error_count: number;
  progress: number;
}

export interface DiagnosticsData {
  loopback: LoopbackState;
  baud_scan: BaudScanState;
  signal_quality: SignalQualityState;
}
```

## Usage Guide

### Common Troubleshooting Workflows

#### Problem: Scale Not Sending Data

**Solution Steps:**

1. **Loopback Test First**: Verify ESP32 hardware is functional
   - Navigate to Diagnostics → Loopback Test
   - Connect GPIO17-18 jumper wire
   - Start test, expect 100% success rate
   - If failing: check GPIO pins for damage

2. **Baud Rate Detection**: Find correct scale baud rate
   - Remove jumper wire
   - Connect scale to GPIO17
   - Navigate to Diagnostics → Baud Detector
   - Start scan, wait for detection
   - Apply detected baud to Serial Config

3. **Signal Quality Check**: Verify connection reliability
   - Reconnect GPIO17-18 jumper
   - Navigate to Diagnostics → Signal Quality
   - Run test with 1000 packets
   - Expect 95%+ quality for reliable operation

#### Problem: Intermittent Data Loss

**Solution:**

1. Run Signal Quality Test
2. Check quality percentage:
   - **95-100%**: Excellent, no action needed
   - **80-94%**: Good, but consider checking wiring
   - **60-79%**: Fair, inspect connections and cables
   - **<60%**: Poor, replace cables or check for interference

3. Review detailed metrics:
   - High jitter (>1ms): Electrical interference nearby
   - High error count: Poor connection or damaged wire
   - High latency (>2ms): Unusual, may indicate hardware issue

#### Problem: Unknown Device Baud Rate

**Solution:**

1. Ensure device is actively transmitting data
2. Connect device TX to GPIO17, GND to GND
3. Navigate to Diagnostics → Baud Detector
4. Click "Start Scan"
5. Wait for detection (tests 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200)
6. Click "Apply to Serial Config" when found

### Test Interpretation

#### Loopback Test Results

- **Status: PASS (100% success)**: Hardware is working correctly
- **Status: PASS (95-99% success)**: Hardware mostly working, minor issues
- **Status: FAIL (<95% success)**: Hardware problem or wiring issue

**Troubleshooting:**

- Verify jumper wire is firmly connected to both pins
- Try a different jumper wire (some wires have poor contact)
- Inspect GPIO pins for bent pins or corrosion
- Check for other devices connected to GPIO17/18

#### Baud Rate Detection Results

- **Status: FOUND**: Baud rate successfully detected
- **Status: NOT_FOUND**: No data received at any tested rate

**Troubleshooting (NOT_FOUND):**

- Verify device is powered on
- Confirm device is actively transmitting (not in sleep mode)
- Check wiring: device TX → ESP32 RX (GPIO17)
- Ensure common ground connection
- Try manually setting baud in Serial Config if device uses non-standard rate

#### Signal Quality Results

**Quality Ratings:**

- **95-100% (EXCELLENT)**: Production-ready, reliable connection
- **80-94% (GOOD)**: Acceptable for most uses, minor improvements possible
- **60-79% (FAIR)**: May work but expect occasional errors
- **<60% (POOR)**: Unreliable, must fix before production use

**Metric Interpretation:**

- **Avg Latency**: Normal range 0.1-2ms (loopback), higher indicates issues
- **Jitter**: Low jitter (<0.5ms) is good, high jitter indicates noise/interference
- **Packet Loss**: Should be <1%, higher indicates connection problems
- **Errors**: Corrupted packets, should be 0 for loopback tests

## Integration with Serial Service

### Workflow

1. **First Time Setup:**
   - Run Diagnostics → Loopback Test (verify hardware)
   - Run Diagnostics → Baud Detector (find scale baud rate)
   - Apply detected baud to Serial → Configuration
   - Test scale data in Serial → Live Stream

2. **Troubleshooting Data Issues:**
   - If Serial shows no data: Run Diagnostics → Loopback + Baud Detector
   - If Serial shows garbled data: Verify baud rate, check Signal Quality
   - If Serial has intermittent errors: Run Signal Quality test

### Separation of Concerns

**DiagnosticsService:**

- Hardware testing and validation
- Interactive tests with immediate feedback
- WebSocket-only for real-time UI updates
- Does NOT communicate with external devices during normal operation

**SerialService:**

- External device communication (scales, sensors)
- Continuous monitoring and data streaming
- Multi-channel broadcasting (REST, WS, MQTT, BLE)
- Configured based on diagnostics results

## Technical Details

### Loopback Test Implementation

**Test Packet Format:** `TEST:123` (counter increments)

**Process:**

1. Start Serial2 at 115200 baud
2. Every 100ms: Send "TEST:{counter}" via TX (GPIO18)
3. Read incoming data from RX (GPIO17)
4. Compare received with sent, increment error count on mismatch
5. Calculate success rate: (rx_count - error_count) / tx_count * 100
6. Status = "pass" if success rate >= 95%, else "fail"

### Baud Rate Detection Implementation

**Tested Rates:** 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200

**Process:**

1. Start at first baud rate (1200)
2. Wait 500ms for incoming data
3. If 3+ packets received: FOUND (return detected baud)
4. If timeout: Move to next baud rate
5. Repeat until rate found or all rates tested
6. If no rates matched: NOT_FOUND

### Signal Quality Test Implementation

**Test Packet Format:** `SIG:123:1234567890` (seq:timestamp)

**Process:**

1. Start Serial2 at 115200 baud
2. Send N packets (user configurable: 100-10000)
3. Each packet includes sequence number and timestamp
4. Calculate metrics:
   - **Latency**: Round-trip time per packet (TX → RX)
   - **Jitter**: Standard deviation of latency
   - **Quality**: (received - errors) / sent * 100
   - **Packet Loss**: (sent - received) / sent * 100
5. Status = "complete" when all packets sent and 1s timeout elapsed

## Notes

### Hardware Considerations

1. **ESP32 Serial2 Pins:** GPIO17 (RX), GPIO18 (TX) on ESP32-S3 or GPIO16 (RX), GPIO17 (TX) on ESP32 are fixed hardware pins
2. **Voltage Levels:** ESP32 operates at 3.3V - use level shifters for 5V devices
3. **Jumper Wire Quality:** Poor quality wires can cause intermittent failures in loopback/signal tests
4. **GPIO Pin Health:** Bent or corroded pins will cause test failures

### Software Considerations

1. **Test Isolation:** Only one test runs at a time (loopback XOR baud scan XOR signal test)
2. **Serial2 Conflict:** DiagnosticsService takes control of Serial2 during tests - SerialService should not be reading simultaneously
3. **Baud Rate:** Tests use 115200 for loopback/signal, variable for baud scan
4. **Line Endings:** Accepts both `\r` and `\n` as line terminators

### Best Practices

1. **Run Loopback First:** Always verify hardware before testing with external devices
2. **Disconnect Scale for Tests:** Loopback and Signal Quality require GPIO17-18 jumper
3. **Let Scans Complete:** Don't interrupt baud scans early, may miss correct rate
4. **More Packets = Better Accuracy:** Use 10,000 packet signal tests for production validation
5. **Regular Health Checks:** Run signal quality tests periodically to detect hardware degradation

## See Also

- [Serial Example Documentation](SERIAL-EXAMPLE.md) - External device communication
- [API Reference](API-REFERENCE.md) - Complete API documentation
- [File Reference](FILE-REFERENCE.md) - Source code organization
