#ifndef DiagnosticsState_h
#define DiagnosticsState_h

#include <StatefulService.h>

// Common baud rates for auto-detection
#define DIAG_BAUD_RATES_COUNT 8
static const uint32_t DIAG_BAUD_RATES[DIAG_BAUD_RATES_COUNT] = {
  1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200
};

class DiagnosticsState {
 public:
  // === Loopback Test State ===
  bool loopbackEnabled;         // True when loopback test is active
  String loopbackStatus;        // "idle", "running", "pass", "fail"
  uint32_t loopbackTxCount;     // Total packets sent
  uint32_t loopbackRxCount;     // Total packets received
  uint32_t loopbackErrorCount;  // Mismatched packets
  String loopbackLastTest;      // Last test string sent
  String loopbackLastReceived;  // Last string received
  unsigned long loopbackStartTime;  // millis() when test started

  // === Baud Rate Detection State ===
  bool baudScanEnabled;         // True when auto-baud scan is active
  String baudScanStatus;        // "idle", "scanning", "found", "not_found"
  uint32_t baudDetected;        // Detected baud rate (0 if not found)
  uint8_t baudCurrentIndex;     // Current baud rate being tested (0-7)
  uint32_t baudTestPackets;     // Packets received at current baud

  // === Signal Quality State ===
  bool signalTestEnabled;       // True when signal quality test is active
  String signalStatus;          // "idle", "running", "complete"
  uint8_t signalQuality;        // 0-100 percentage
  uint32_t signalTotalPackets;  // Total packets to send for test
  uint32_t signalSentPackets;   // Packets sent so far
  uint32_t signalReceivedPackets;  // Packets received
  float signalAvgLatency;       // Average round-trip latency (ms)
  float signalJitter;           // Latency variance (ms)
  uint32_t signalErrorCount;    // Corrupted/mismatched packets

  static void read(DiagnosticsState& state, JsonObject& root) {
    // Loopback
    JsonObject loopback = root["loopback"].to<JsonObject>();
    loopback["enabled"] = state.loopbackEnabled;
    loopback["status"] = state.loopbackStatus;
    loopback["tx_count"] = state.loopbackTxCount;
    loopback["rx_count"] = state.loopbackRxCount;
    loopback["error_count"] = state.loopbackErrorCount;
    loopback["success_rate"] = state.loopbackTxCount > 0 
      ? (float)(state.loopbackRxCount - state.loopbackErrorCount) / state.loopbackTxCount * 100.0f 
      : 0.0f;
    loopback["last_test"] = state.loopbackLastTest;
    loopback["last_received"] = state.loopbackLastReceived;
    loopback["uptime_seconds"] = state.loopbackStartTime > 0 
      ? (millis() - state.loopbackStartTime) / 1000 
      : 0;

    // Baud detection
    JsonObject baud = root["baud_scan"].to<JsonObject>();
    baud["enabled"] = state.baudScanEnabled;
    baud["status"] = state.baudScanStatus;
    baud["detected_baud"] = state.baudDetected;
    baud["current_index"] = state.baudCurrentIndex;
    baud["test_packets"] = state.baudTestPackets;
    if (state.baudCurrentIndex < DIAG_BAUD_RATES_COUNT) {
      baud["current_baud"] = DIAG_BAUD_RATES[state.baudCurrentIndex];
    }

    // Signal quality
    JsonObject signal = root["signal_quality"].to<JsonObject>();
    signal["enabled"] = state.signalTestEnabled;
    signal["status"] = state.signalStatus;
    signal["quality_percent"] = state.signalQuality;
    signal["total_packets"] = state.signalTotalPackets;
    signal["sent_packets"] = state.signalSentPackets;
    signal["received_packets"] = state.signalReceivedPackets;
    signal["avg_latency_ms"] = state.signalAvgLatency;
    signal["jitter_ms"] = state.signalJitter;
    signal["error_count"] = state.signalErrorCount;
    signal["progress"] = state.signalTotalPackets > 0 
      ? (float)state.signalSentPackets / state.signalTotalPackets * 100.0f 
      : 0.0f;
  }

  static StateUpdateResult update(JsonObject& root, DiagnosticsState& state) {
    StateUpdateResult result = StateUpdateResult::UNCHANGED;

    // Loopback control
    if (root.containsKey("loopback_enabled")) {
      bool v = root["loopback_enabled"];
      if (v != state.loopbackEnabled) {
        state.loopbackEnabled = v;
        if (v) {
          // Starting test - reset counters
          state.loopbackStatus = "running";
          state.loopbackTxCount = 0;
          state.loopbackRxCount = 0;
          state.loopbackErrorCount = 0;
          state.loopbackLastTest = "";
          state.loopbackLastReceived = "";
          state.loopbackStartTime = millis();
        } else {
          // Stopping test - mark result as "stopped" instead of "idle"
          if (state.loopbackTxCount > 0) {
            // Keep status as pass/fail based on results
          } else {
            state.loopbackStatus = "idle";
          }
        }
        result = StateUpdateResult::CHANGED;
      }
    }

    // Baud scan control
    if (root.containsKey("baud_scan_enabled")) {
      bool v = root["baud_scan_enabled"];
      if (v != state.baudScanEnabled) {
        state.baudScanEnabled = v;
        if (v) {
          // Starting scan - reset state
          state.baudScanStatus = "scanning";
          state.baudDetected = 0;
          state.baudCurrentIndex = 0;
          state.baudTestPackets = 0;
        } else {
          // Stopping scan
          state.baudScanStatus = "idle";
        }
        result = StateUpdateResult::CHANGED;
      }
    }

    // Signal quality test control
    if (root.containsKey("signal_test_enabled")) {
      bool v = root["signal_test_enabled"];
      if (v != state.signalTestEnabled) {
        state.signalTestEnabled = v;
        if (v) {
          // Starting test - reset state
          uint32_t packets = root["signal_total_packets"] | 1000;
          state.signalStatus = "running";
          state.signalTotalPackets = packets;
          state.signalSentPackets = 0;
          state.signalReceivedPackets = 0;
          state.signalQuality = 0;
          state.signalAvgLatency = 0.0f;
          state.signalJitter = 0.0f;
          state.signalErrorCount = 0;
        } else {
          // Stopping test
          state.signalStatus = "idle";
        }
        result = StateUpdateResult::CHANGED;
      }
    }

    return result;
  }
};

#endif
