# Multi-Mode Serial — Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a Writer mode (alongside Reader and Diagnostics) to the existing single-firmware ESP32 project. Mode is exclusively owned by one service at a time. Writer accepts data from four local channels (Manual, REST, WebSocket, MQTT) and from a remote source Reader. The Serial section in the web UI swaps between Reader and Writer screens based on the active mode. Adds a "NEW" first-run state with friendly default hotspot SSIDs.

**Architecture:** Extend `UartModeType` enum to three states (`READER`, `WRITER`, `DIAGNOSTICS`). Persisted as a string with empty meaning "NEW". `UartModeService` already coordinates two-way exclusivity — extend to three. New backend services `SerialWriterService` (local input handler) and `SerialWriterForwarderService` (WebSocket client subscribing to a remote Reader's `/ws/serial`) live in `src/examples/serialwriter/`. Frontend gains mode-aware tabs in `SerialMonitor.tsx` and six new Writer screens.

**Tech Stack:** ESP32-S3 Arduino-PlatformIO firmware (C++), React/TypeScript frontend with MUI, ESPAsyncWebServer, AsyncMqttClient, ArduinoJson, FSPersistence (built-in framework).

**Reference spec:** [`docs/superpowers/specs/2026-04-30-multi-mode-serial-reader-writer-design.md`](../specs/2026-04-30-multi-mode-serial-reader-writer-design.md).

---

## File map

### Backend — created

- `src/examples/serialwriter/SerialWriterState.h` — state class for the local Writer service.
- `src/examples/serialwriter/SerialWriterService.h`/`.cpp` — local Writer (REST/WS/MQTT inbound + serial port output).
- `src/examples/serialwriter/SerialWriterForwarderState.h` — state class for the source-Reader subscription.
- `src/examples/serialwriter/SerialWriterForwarderService.h`/`.cpp` — WebSocket client that pulls from a remote Reader.

### Backend — modified

- `src/UartMode.h` — add `WRITER` to enum, rename `LIVE_MONITORING` → `READER`, persist as string with `""` meaning NEW.
- `src/UartModeService.h`/`.cpp` — register Writer services, three-way `applyMode()`.
- `src/main.cpp` — instantiate and wire the new Writer services.

### Frontend — created

- `interface/src/types/serialWriter.ts` — TypeScript types mirroring `SerialWriterState`.
- `interface/src/api/serialWriter.ts` — REST + WS client functions.
- `interface/src/examples/serialwriter/SerialWriter.tsx` — the writer-mode container with tab routing.
- `interface/src/examples/serialwriter/WriterStatus.tsx` — Status tab.
- `interface/src/examples/serialwriter/WriterSettings.tsx` — Settings tab.
- `interface/src/examples/serialwriter/SendMessage.tsx` — Send Message tab.
- `interface/src/examples/serialwriter/SendViaWeb.tsx` — Send via Web tab.
- `interface/src/examples/serialwriter/WriterLiveStream.tsx` — Live Stream tab.
- `interface/src/examples/serialwriter/WriterMqtt.tsx` — MQTT tab.

### Frontend — modified

- `interface/src/types/uartMode.ts` — extend mode type to include `'writer'` and `''` (NEW).
- `interface/src/components/UartModeSwitcher.tsx` — 3-way toggle + confirmation dialog.
- `interface/src/examples/serial/SerialMonitor.tsx` — read mode and route to reader-or-writer container.
- `interface/src/project/ProjectRouting.tsx` — no change to the `serial/*` mount; new container handles internal routing.

### Tests / verification

- This codebase has no automated test framework. Each task ends with a **build + flash + manual verify** step (or just build for frontend-only changes). Manual verification is described per task.

---

## Conventions used in this plan

- All file paths are relative to `C:\Projects\Weighsoft.Hardware.Base`.
- Build command: `pio run -e esp32s3` (build only, fast); upload command: `pio run -e esp32s3 -t upload --upload-port COM2` (replace `COM2` with whatever Windows currently shows the device on; if device is on WiFi, configure OTA in `platformio.ini`).
- pio binary lives at `C:\Users\User\AppData\Roaming\Python\Python313\Scripts\pio.exe` per session findings.
- Commit cadence: after each task, commit with the message shown in that task's last step.
- Plain language convention applies to **user-visible UI strings** — code identifiers stay descriptive.

---

## Task 1 — Define the three-way mode enum

**Files:**
- Modify: `src/UartMode.h`

- [ ] **Step 1: Replace the enum and state class with a three-way version.**

Open `src/UartMode.h` and replace lines 8–46 with:

```cpp
// UART Mode: which service owns Serial1 (GPIO18 RX / GPIO17 TX) on ESP32 targets.
// Only one service can use Serial1 at a time.
enum class UartModeType {
  READER,        // SerialService — reads from a physical scale (was LIVE_MONITORING)
  WRITER,        // SerialWriterService — writes to a physical serial port
  DIAGNOSTICS    // DiagnosticsService — hardware tests
};

class UartModeState {
 public:
  // Empty modeStr ("") represents the NEW state — device hasn't been told what it is yet.
  // When configured, modeStr is one of: "reader", "writer", "diagnostics".
  String modeStr;

  UartModeState() : modeStr("") {}

  bool isConfigured() const { return modeStr.length() > 0; }
  bool isReader() const { return modeStr == "reader"; }
  bool isWriter() const { return modeStr == "writer"; }
  bool isDiagnostics() const { return modeStr == "diagnostics"; }
  UartModeType type() const {
    if (modeStr == "writer") return UartModeType::WRITER;
    if (modeStr == "diagnostics") return UartModeType::DIAGNOSTICS;
    return UartModeType::READER;  // default for empty/NEW or "reader"
  }

  static void read(UartModeState& state, JsonObject& root) {
    root["mode"] = state.modeStr;  // "" means NEW
  }

  static StateUpdateResult update(JsonObject& root, UartModeState& state) {
    if (root.containsKey("mode")) {
      String incoming = root["mode"].as<String>();

      // Migration: legacy "live" → "reader"
      if (incoming == "live") incoming = "reader";

      // Validate
      if (incoming != "" && incoming != "reader" && incoming != "writer" && incoming != "diagnostics") {
        return StateUpdateResult::ERROR;
      }

      if (incoming != state.modeStr) {
        state.modeStr = incoming;
        return StateUpdateResult::CHANGED;
      }
    }
    return StateUpdateResult::UNCHANGED;
  }
};
```

- [ ] **Step 2: Build to confirm the header compiles.**

Run: `pio run -e esp32s3`
Expected: build will fail downstream because `UartModeService` still uses the old `mode` field; that's fine — we fix it in Task 2. The header itself should compile cleanly when included.

- [ ] **Step 3: Commit (no commit yet — wait for Task 2 to compile cleanly together).**

No commit at this step — proceed to Task 2.

---

## Task 2 — Update UartModeService for the three-way enum

**Files:**
- Modify: `src/UartModeService.h`
- Modify: `src/UartModeService.cpp`

- [ ] **Step 1: Update `UartModeService.h` — add Writer service registration and three-way helpers.**

Replace the contents of `src/UartModeService.h` with:

```cpp
#ifndef UartModeService_h
#define UartModeService_h

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <WebSocketTxRx.h>
#include "UartMode.h"

// Forward declarations
class SerialService;
class SerialWriterService;
class SerialWriterForwarderService;
class DiagnosticsService;

#define UART_MODE_ENDPOINT_PATH "/rest/uartMode"
#define UART_MODE_SOCKET_PATH "/ws/uartMode"
#define UART_MODE_CONFIG_FILE "/config/uartMode.json"

class UartModeService : public StatefulService<UartModeState> {
 public:
  UartModeService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager);
  void begin();

  void setSerialService(SerialService* serialService);
  void setWriterService(SerialWriterService* writerService);
  void setForwarderService(SerialWriterForwarderService* forwarderService);
  void setDiagnosticsService(DiagnosticsService* diagnosticsService);

  void applyMode();

  bool isReader() const { return _state.isReader(); }
  bool isWriter() const { return _state.isWriter(); }
  bool isDiagnostics() const { return _state.isDiagnostics(); }
  bool isConfigured() const { return _state.isConfigured(); }

 private:
  HttpEndpoint<UartModeState> _httpEndpoint;
  FSPersistence<UartModeState> _fsPersistence;
  WebSocketTxRx<UartModeState> _webSocket;

  SerialService* _serialService = nullptr;
  SerialWriterService* _writerService = nullptr;
  SerialWriterForwarderService* _forwarderService = nullptr;
  DiagnosticsService* _diagnosticsService = nullptr;

  void onModeChanged();
};

#endif
```

- [ ] **Step 2: Update `UartModeService.cpp` — three-way `applyMode()`.**

Replace the body of `applyMode()` (and the `setSerialService` / `setDiagnosticsService` registrations) with logic that:

1. Suspends ALL three services (Reader, Writer, Forwarder, Diagnostics) first.
2. Resumes only the active mode's owner.

Use this implementation skeleton (adapt to existing patterns in the .cpp — preserve the `onUpdate` registration, `begin()`, `_fsPersistence.readFromFS()`, etc. that already exist):

```cpp
void UartModeService::setSerialService(SerialService* s) { _serialService = s; }
void UartModeService::setWriterService(SerialWriterService* s) { _writerService = s; }
void UartModeService::setForwarderService(SerialWriterForwarderService* s) { _forwarderService = s; }
void UartModeService::setDiagnosticsService(DiagnosticsService* s) { _diagnosticsService = s; }

void UartModeService::applyMode() {
  // Forward declarations don't have suspend/resume yet — these calls
  // require including the actual headers in the .cpp:
  //   #include <examples/serial/SerialService.h>
  //   #include <examples/serialwriter/SerialWriterService.h>
  //   #include <examples/serialwriter/SerialWriterForwarderService.h>
  //   #include <examples/diagnostics/DiagnosticsService.h>  (if it exists)

  // Suspend everyone first.
  if (_serialService)        _serialService->suspendSerial();
  if (_writerService)        _writerService->suspendWriter();
  if (_forwarderService)     _forwarderService->stop();
  // (Diagnostics service: keep existing suspend pattern from the prior 2-way code.)

  // Resume the active owner only.
  if (_state.isWriter()) {
    if (_writerService)    _writerService->resumeWriter();
    if (_forwarderService) _forwarderService->start();
  } else if (_state.isDiagnostics()) {
    // Existing pattern for diagnostics: it owns the UART when active.
    // (No-op here unless DiagnosticsService has a resume — preserve current behavior.)
  } else {
    // READER (default). Also covers NEW state: keep the UART quiet but bias toward Reader behavior.
    if (_serialService) _serialService->resumeSerial();
  }
}

void UartModeService::onModeChanged() {
  applyMode();
  _fsPersistence.writeToFS();
}
```

- [ ] **Step 3: Build the firmware to confirm it compiles up to (but not including) the new Writer services.**

Run: `pio run -e esp32s3`
Expected: build will fail with link errors mentioning `SerialWriterService` / `SerialWriterForwarderService` — those don't exist yet. We add them in subsequent tasks.

- [ ] **Step 4: Commit Task 1 + Task 2 together.**

```bash
git add src/UartMode.h src/UartModeService.h src/UartModeService.cpp
git commit -m "feat(uart-mode): three-way enum (Reader/Writer/Diagnostics) with NEW state migration"
```

---

## Task 3 — SerialWriterState header

**Files:**
- Create: `src/examples/serialwriter/SerialWriterState.h`

- [ ] **Step 1: Create the directory and state header.**

```bash
mkdir -p src/examples/serialwriter
```

- [ ] **Step 2: Write `SerialWriterState.h`.**

```cpp
#ifndef SerialWriterState_h
#define SerialWriterState_h

#include <StatefulService.h>
#include <SettingValue.h>

#define SERIALW_DEFAULT_BAUDRATE 9600
#define SERIALW_MIN_BAUDRATE     300
#define SERIALW_MAX_BAUDRATE     2000000

#define SERIALW_LE_NONE 0
#define SERIALW_LE_CR   1
#define SERIALW_LE_LF   2
#define SERIALW_LE_CRLF 3

class SerialWriterState {
 public:
  // Persisted configuration
  uint32_t baudrate;       // 9600, 19200, etc.
  uint8_t  databits;       // 5..8
  uint8_t  stopbits;       // 1 or 2
  uint8_t  parity;         // 0=None,1=Even,2=Odd
  uint8_t  lineEnding;     // 0=None,1=CR,2=LF,3=CRLF
  String   friendlyName;   // user-set device name shown on Reader's Writers tab
  String   mqttSubscribeTopic;
  String   mqttStatusTopic;
  bool     mqttEnabled;

  // Runtime stats (not persisted)
  String        lastSent;
  unsigned long lastSentAt = 0;
  uint8_t       lastSentSource = 0;  // 0=NONE,1=MANUAL,2=REST,3=WS,4=MQTT,5=READER
  unsigned long bytesSentTotal = 0;
  uint8_t       suspended = 0;
  uint8_t       serialStarted = 0;

  SerialWriterState()
      : baudrate(SERIALW_DEFAULT_BAUDRATE),
        databits(8),
        stopbits(1),
        parity(0),
        lineEnding(SERIALW_LE_CRLF),
        friendlyName(""),
        mqttSubscribeTopic(""),
        mqttStatusTopic(""),
        mqttEnabled(false) {}

  static void read(SerialWriterState& state, JsonObject& root) {
    root["baud_rate"]            = state.baudrate;
    root["data_bits"]            = state.databits;
    root["stop_bits"]            = state.stopbits;
    root["parity"]               = state.parity;
    root["line_ending"]          = state.lineEnding;
    root["friendly_name"]        = state.friendlyName;
    root["mqtt_subscribe_topic"] = state.mqttSubscribeTopic;
    root["mqtt_status_topic"]    = state.mqttStatusTopic;
    root["mqtt_enabled"]         = state.mqttEnabled;
    root["device_id"]            = SettingValue::getUniqueId();

    root["last_sent"]            = state.lastSent;
    root["last_sent_at"]         = state.lastSentAt;
    root["last_sent_source"]     = state.lastSentSource;
    root["bytes_sent_total"]     = state.bytesSentTotal;
    root["suspended"]            = state.suspended;
    root["serial_started"]       = state.serialStarted;
  }

  static void readConfig(SerialWriterState& state, JsonObject& root) {
    root["baud_rate"]            = state.baudrate;
    root["data_bits"]            = state.databits;
    root["stop_bits"]            = state.stopbits;
    root["parity"]               = state.parity;
    root["line_ending"]          = state.lineEnding;
    root["friendly_name"]        = state.friendlyName;
    root["mqtt_subscribe_topic"] = state.mqttSubscribeTopic;
    root["mqtt_status_topic"]    = state.mqttStatusTopic;
    root["mqtt_enabled"]         = state.mqttEnabled;
  }

  static StateUpdateResult updateConfig(JsonObject& root, SerialWriterState& state) {
    state.baudrate           = root["baud_rate"]            | (uint32_t)SERIALW_DEFAULT_BAUDRATE;
    state.databits           = root["data_bits"]            | (uint8_t)8;
    state.stopbits           = root["stop_bits"]            | (uint8_t)1;
    state.parity             = root["parity"]               | (uint8_t)0;
    state.lineEnding         = root["line_ending"]          | (uint8_t)SERIALW_LE_CRLF;
    state.friendlyName       = root["friendly_name"]        | "";
    state.mqttSubscribeTopic = root["mqtt_subscribe_topic"] | "";
    state.mqttStatusTopic    = root["mqtt_status_topic"]    | "";
    state.mqttEnabled        = root["mqtt_enabled"]         | false;

    if (state.baudrate < SERIALW_MIN_BAUDRATE || state.baudrate > SERIALW_MAX_BAUDRATE) state.baudrate = SERIALW_DEFAULT_BAUDRATE;
    if (state.databits < 5 || state.databits > 8) state.databits = 8;
    if (state.stopbits < 1 || state.stopbits > 2) state.stopbits = 1;
    if (state.parity > 2) state.parity = 0;
    if (state.lineEnding > SERIALW_LE_CRLF) state.lineEnding = SERIALW_LE_CRLF;
    return StateUpdateResult::CHANGED;
  }

  static StateUpdateResult update(JsonObject& root, SerialWriterState& state) {
    StateUpdateResult result = StateUpdateResult::UNCHANGED;
    auto changed = [&result]() { result = StateUpdateResult::CHANGED; };

    if (root.containsKey("baud_rate")) {
      uint32_t v = root["baud_rate"];
      if (v >= SERIALW_MIN_BAUDRATE && v <= SERIALW_MAX_BAUDRATE && v != state.baudrate) { state.baudrate = v; changed(); }
    }
    if (root.containsKey("data_bits")) {
      uint8_t v = root["data_bits"];
      if (v >= 5 && v <= 8 && v != state.databits) { state.databits = v; changed(); }
    }
    if (root.containsKey("stop_bits")) {
      uint8_t v = root["stop_bits"];
      if ((v == 1 || v == 2) && v != state.stopbits) { state.stopbits = v; changed(); }
    }
    if (root.containsKey("parity")) {
      uint8_t v = root["parity"];
      if (v <= 2 && v != state.parity) { state.parity = v; changed(); }
    }
    if (root.containsKey("line_ending")) {
      uint8_t v = root["line_ending"];
      if (v <= SERIALW_LE_CRLF && v != state.lineEnding) { state.lineEnding = v; changed(); }
    }
    if (root.containsKey("friendly_name")) {
      String v = root["friendly_name"].as<String>();
      if (v != state.friendlyName) { state.friendlyName = v; changed(); }
    }
    if (root.containsKey("mqtt_subscribe_topic")) {
      String v = root["mqtt_subscribe_topic"].as<String>();
      if (v != state.mqttSubscribeTopic) { state.mqttSubscribeTopic = v; changed(); }
    }
    if (root.containsKey("mqtt_status_topic")) {
      String v = root["mqtt_status_topic"].as<String>();
      if (v != state.mqttStatusTopic) { state.mqttStatusTopic = v; changed(); }
    }
    if (root.containsKey("mqtt_enabled")) {
      bool v = root["mqtt_enabled"];
      if (v != state.mqttEnabled) { state.mqttEnabled = v; changed(); }
    }
    return result;
  }
};

#endif
```

- [ ] **Step 3: Build to confirm header compiles in isolation.**

Run: `pio run -e esp32s3`
Expected: still fails with link errors for `SerialWriterService` (we haven't created the .h/.cpp yet). The state header itself must compile silently.

- [ ] **Step 4: Commit.**

```bash
git add src/examples/serialwriter/SerialWriterState.h
git commit -m "feat(serialwriter): add SerialWriterState"
```

---

## Task 4 — SerialWriterService header

**Files:**
- Create: `src/examples/serialwriter/SerialWriterService.h`

- [ ] **Step 1: Write the header.**

```cpp
#ifndef SerialWriterService_h
#define SerialWriterService_h

#include <deque>

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <MqttPubSub.h>
#include <WebSocketTxRx.h>
#include <SettingValue.h>
#include <examples/serialwriter/SerialWriterState.h>

#define SERIALW_ENDPOINT_PATH    "/rest/serialWriter"
#define SERIALW_SOCKET_PATH      "/ws/serialWriter"
#define SERIALW_CONFIG_FILE      "/config/serialWriterConfig.json"

#define SERIALW_PORT             Serial1
#define SERIALW_RX_PIN           18
#define SERIALW_TX_PIN           17

// Source identifiers for transmitted lines (also surfaced in WS broadcast events)
enum class TxSource : uint8_t {
  NONE   = 0,
  MANUAL = 1,
  REST   = 2,
  WS     = 3,
  MQTT   = 4,
  READER = 5
};

class SerialWriterService : public StatefulService<SerialWriterState> {
 public:
  SerialWriterService(AsyncWebServer* server,
                      FS* fs,
                      SecurityManager* securityManager,
                      AsyncMqttClient* mqttClient);
  void begin();
  void loop();

  // Coordination with UartModeService — only the active mode owns Serial1.
  void suspendWriter();
  void resumeWriter();
  bool isSuspended() const { return _suspended; }

  // Single funnel for ALL outbound paths. Returns number of bytes written (0 on failure).
  size_t transmit(const String& data, TxSource source);

 private:
  HttpEndpoint<SerialWriterState>   _httpEndpoint;
  FSPersistence<SerialWriterState>  _fsPersistence;
  MqttPubSub<SerialWriterState>     _mqttPubSub;
  WebSocketTxRx<SerialWriterState>  _webSocket;
  AsyncMqttClient*                  _mqttClient;

  AsyncWebServer*  _server;
  SecurityManager* _securityManager;

  bool _serialStarted = false;
  bool _suspended     = true;  // start suspended; UartModeService activates us only when in Writer mode

  // Inbound REST endpoint for { "data": "..." }
  void registerSendEndpoint();
  void onSendRequest(AsyncWebServerRequest* request, JsonVariant& json);

  // Inbound WS handler for plain text or JSON {"data":"..."} frames
  void registerWsSink();

  // Inbound MQTT handler
  void onMqttMessage(const String& topic, const String& payload);
  void configureMqttSubscription();

  // Helpers
  void applySerialConfig();
  uint32_t serialMode() const;       // databits/parity/stopbits → ESP32 SERIAL_* constant
  String   lineEndingChars() const;  // returns "" / "\r" / "\n" / "\r\n"
  void     onConfigUpdated();
  void     broadcastTxEvent(const String& data, TxSource source);
};

#endif
```

- [ ] **Step 2: Build — header should compile inclusion-wise; .cpp doesn't exist yet so link will fail.**

Run: `pio run -e esp32s3`
Expected: link errors for `SerialWriterService::*` symbols. Header parsing should be clean.

- [ ] **Step 3: Commit.**

```bash
git add src/examples/serialwriter/SerialWriterService.h
git commit -m "feat(serialwriter): add SerialWriterService header"
```

---

## Task 5 — SerialWriterService implementation

**Files:**
- Create: `src/examples/serialwriter/SerialWriterService.cpp`

- [ ] **Step 1: Write the implementation.**

Mirror the pattern in `src/examples/serial/SerialService.cpp` for HttpEndpoint/MqttPubSub/WebSocketTxRx wiring. The skeleton:

```cpp
#include <examples/serialwriter/SerialWriterService.h>

SerialWriterService::SerialWriterService(AsyncWebServer* server,
                                         FS* fs,
                                         SecurityManager* securityManager,
                                         AsyncMqttClient* mqttClient) :
    _httpEndpoint(SerialWriterState::read,
                  SerialWriterState::update,
                  this,
                  server,
                  SERIALW_ENDPOINT_PATH,
                  securityManager),
    _fsPersistence(SerialWriterState::readConfig,
                   SerialWriterState::updateConfig,
                   this,
                   fs,
                   SERIALW_CONFIG_FILE),
    _mqttPubSub(SerialWriterState::read,
                SerialWriterState::update,
                this,
                mqttClient),
    _webSocket(SerialWriterState::read,
               SerialWriterState::update,
               this,
               server,
               SERIALW_SOCKET_PATH,
               securityManager),
    _mqttClient(mqttClient),
    _server(server),
    _securityManager(securityManager) {
  addUpdateHandler([this](const String& origin) { onConfigUpdated(); }, false);
}

void SerialWriterService::begin() {
  _fsPersistence.readFromFS();
  registerSendEndpoint();
  registerWsSink();
  configureMqttSubscription();
  // Do NOT start the serial port here — UartModeService::applyMode() calls resumeWriter()
  // exactly once when the mode becomes WRITER.
}

void SerialWriterService::loop() {
  // Reserved for future periodic work (status heartbeat, etc.). No-op for v1.
}

void SerialWriterService::suspendWriter() {
  if (_suspended) return;
  if (_serialStarted) {
    SERIALW_PORT.end();
    _serialStarted = false;
  }
  _suspended = true;
  update([&](SerialWriterState& s) {
    s.suspended = 1;
    s.serialStarted = 0;
    return StateUpdateResult::CHANGED;
  }, "suspend");
}

void SerialWriterService::resumeWriter() {
  if (!_suspended) return;
  applySerialConfig();
  _suspended = false;
  update([&](SerialWriterState& s) {
    s.suspended = 0;
    s.serialStarted = _serialStarted ? 1 : 0;
    return StateUpdateResult::CHANGED;
  }, "resume");
}

size_t SerialWriterService::transmit(const String& data, TxSource source) {
  if (_suspended || !_serialStarted) return 0;
  if (data.length() == 0) return 0;

  size_t written = SERIALW_PORT.print(data);
  String le = lineEndingChars();
  if (le.length() > 0) written += SERIALW_PORT.print(le);

  // Update stats and broadcast
  String composed = data + le;
  update([&](SerialWriterState& s) {
    s.lastSent = composed;
    s.lastSentAt = millis();
    s.lastSentSource = (uint8_t)source;
    s.bytesSentTotal += written;
    return StateUpdateResult::CHANGED;
  }, "tx");
  broadcastTxEvent(composed, source);
  return written;
}

void SerialWriterService::applySerialConfig() {
  if (_serialStarted) { SERIALW_PORT.end(); _serialStarted = false; }
  read([&](const SerialWriterState& s) {
    SERIALW_PORT.begin(s.baudrate, serialMode(), SERIALW_RX_PIN, SERIALW_TX_PIN);
  });
  _serialStarted = true;
}

uint32_t SerialWriterService::serialMode() const {
  uint32_t mode = SERIAL_8N1;
  // Compose from databits/parity/stopbits (mirror SerialService::getSerialConfig pattern)
  read([&](const SerialWriterState& s) {
    uint8_t bits   = s.databits;
    uint8_t par    = s.parity;
    uint8_t stop   = s.stopbits;
    uint32_t parityBits = par == 1 ? 0x2 : par == 2 ? 0x3 : 0x0;
    uint32_t stopBits   = stop == 2 ? 0x30 : 0x10;
    uint32_t dataBits   = ((bits - 5) & 0x3) << 2;
    mode = 0x800001c | dataBits | parityBits | stopBits;
  });
  return mode;
}

String SerialWriterService::lineEndingChars() const {
  String result = "";
  read([&](const SerialWriterState& s) {
    switch (s.lineEnding) {
      case SERIALW_LE_CR:   result = "\r";   break;
      case SERIALW_LE_LF:   result = "\n";   break;
      case SERIALW_LE_CRLF: result = "\r\n"; break;
      default:              result = "";     break;
    }
  });
  return result;
}

void SerialWriterService::onConfigUpdated() {
  _fsPersistence.writeToFS();
  if (!_suspended) applySerialConfig();
  configureMqttSubscription();
}

void SerialWriterService::registerSendEndpoint() {
  _server->on((String(SERIALW_ENDPOINT_PATH) + "/send").c_str(),
              HTTP_POST,
              [](AsyncWebServerRequest* req) { /* no-op for body handler */ },
              nullptr,
              [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
                StaticJsonDocument<512> doc;
                if (deserializeJson(doc, data, len)) {
                  req->send(400, "application/json", "{\"error\":\"invalid json\"}");
                  return;
                }
                String text = doc["data"] | "";
                if (text.length() == 0) {
                  req->send(400, "application/json", "{\"error\":\"data required\"}");
                  return;
                }
                size_t n = transmit(text, TxSource::REST);
                if (n == 0) {
                  req->send(503, "application/json", "{\"error\":\"writer suspended or serial not started\"}");
                } else {
                  req->send(200, "application/json", "{\"sent\":true}");
                }
              });
}

void SerialWriterService::registerWsSink() {
  // Hook a separate raw WS endpoint for inbound frames + outbound tx events.
  // Implementation: extend WebSocketTxRx pattern, OR add a raw AsyncWebSocket here.
  // Mirror SerialService's existing WS pattern; tag inbound frames as TxSource::WS.
  // Placeholder: see SerialService for the pattern; copy-adapt.
}

void SerialWriterService::configureMqttSubscription() {
  read([&](const SerialWriterState& s) {
    if (!s.mqttEnabled || s.mqttSubscribeTopic.length() == 0 || _mqttClient == nullptr) return;
    _mqttClient->subscribe(s.mqttSubscribeTopic.c_str(), 0);
  });
}

void SerialWriterService::onMqttMessage(const String& topic, const String& payload) {
  read([&](const SerialWriterState& s) {
    if (s.mqttEnabled && s.mqttSubscribeTopic.length() > 0 && topic == s.mqttSubscribeTopic) {
      transmit(payload, TxSource::MQTT);
    }
  });
}

void SerialWriterService::broadcastTxEvent(const String& data, TxSource source) {
  // Mirror SerialService broadcast pattern: build a small JSON doc and send to WS clients.
  // Actual code follows the same _webSocket emission used in SerialService::publishLine.
}
```

> **Note for the implementer:** Two methods (`registerWsSink`, `broadcastTxEvent`) are stubs in this draft because they need to mirror `SerialService`'s exact framework patterns (use the existing `WebSocketTxRx` instance methods or a dedicated `AsyncWebSocket`). Read `src/examples/serial/SerialService.cpp` for the established pattern, then port. Do **not** invent a new pattern.

- [ ] **Step 2: Build.**

Run: `pio run -e esp32s3`
Expected: build should succeed because SerialWriterService now has both header and implementation. UartModeService still references `_writerService->...` and `_forwarderService->...` — the latter doesn't exist yet, so build will still fail on `SerialWriterForwarderService`. Forwarder is added in Task 7.

- [ ] **Step 3: Commit.**

```bash
git add src/examples/serialwriter/SerialWriterService.cpp
git commit -m "feat(serialwriter): implement SerialWriterService — local input channels and transmit funnel"
```

---

## Task 6 — SerialWriterForwarderState header

**Files:**
- Create: `src/examples/serialwriter/SerialWriterForwarderState.h`

- [ ] **Step 1: Write the state header.**

```cpp
#ifndef SerialWriterForwarderState_h
#define SerialWriterForwarderState_h

#include <StatefulService.h>

#define SERIALW_FWD_METHOD_WS    0
#define SERIALW_FWD_METHOD_HTTP  1

class SerialWriterForwarderState {
 public:
  // Persisted
  String  sourceUrl;          // e.g. "ws://192.168.2.50/ws/serial" or "http://192.168.2.50/rest/serial"
  uint8_t connectionMethod;   // 0=WS (default), 1=HTTP polling
  bool    autoReconnect;      // default true
  bool    enabled;            // default false until user sets a source

  // Runtime (not persisted)
  bool          connected = false;
  unsigned long lastReceivedAt = 0;
  String        lastReceived;
  uint16_t      reconnectAttempts = 0;
  String        lastError;

  SerialWriterForwarderState()
      : sourceUrl(""),
        connectionMethod(SERIALW_FWD_METHOD_WS),
        autoReconnect(true),
        enabled(false) {}

  static void read(SerialWriterForwarderState& state, JsonObject& root) {
    root["source_url"]         = state.sourceUrl;
    root["connection_method"]  = state.connectionMethod;
    root["auto_reconnect"]     = state.autoReconnect;
    root["enabled"]            = state.enabled;
    root["connected"]          = state.connected;
    root["last_received_at"]   = state.lastReceivedAt;
    root["last_received"]      = state.lastReceived;
    root["reconnect_attempts"] = state.reconnectAttempts;
    root["last_error"]         = state.lastError;
  }

  static void readConfig(SerialWriterForwarderState& state, JsonObject& root) {
    root["source_url"]         = state.sourceUrl;
    root["connection_method"]  = state.connectionMethod;
    root["auto_reconnect"]     = state.autoReconnect;
    root["enabled"]            = state.enabled;
  }

  static StateUpdateResult updateConfig(JsonObject& root, SerialWriterForwarderState& state) {
    state.sourceUrl        = root["source_url"]        | "";
    state.connectionMethod = root["connection_method"] | (uint8_t)SERIALW_FWD_METHOD_WS;
    state.autoReconnect    = root["auto_reconnect"]    | true;
    state.enabled          = root["enabled"]           | false;
    if (state.connectionMethod > SERIALW_FWD_METHOD_HTTP) state.connectionMethod = SERIALW_FWD_METHOD_WS;
    return StateUpdateResult::CHANGED;
  }

  static StateUpdateResult update(JsonObject& root, SerialWriterForwarderState& state) {
    StateUpdateResult result = StateUpdateResult::UNCHANGED;
    if (root.containsKey("source_url")) {
      String v = root["source_url"].as<String>();
      if (v != state.sourceUrl) { state.sourceUrl = v; result = StateUpdateResult::CHANGED; }
    }
    if (root.containsKey("connection_method")) {
      uint8_t v = root["connection_method"];
      if (v <= SERIALW_FWD_METHOD_HTTP && v != state.connectionMethod) { state.connectionMethod = v; result = StateUpdateResult::CHANGED; }
    }
    if (root.containsKey("auto_reconnect")) {
      bool v = root["auto_reconnect"];
      if (v != state.autoReconnect) { state.autoReconnect = v; result = StateUpdateResult::CHANGED; }
    }
    if (root.containsKey("enabled")) {
      bool v = root["enabled"];
      if (v != state.enabled) { state.enabled = v; result = StateUpdateResult::CHANGED; }
    }
    return result;
  }
};

#endif
```

- [ ] **Step 2: Build.**

Run: `pio run -e esp32s3`
Expected: still fails at link for `SerialWriterForwarderService` symbols. State header alone parses fine.

- [ ] **Step 3: Commit.**

```bash
git add src/examples/serialwriter/SerialWriterForwarderState.h
git commit -m "feat(serialwriter): add SerialWriterForwarderState"
```

---

## Task 7 — SerialWriterForwarderService header

**Files:**
- Create: `src/examples/serialwriter/SerialWriterForwarderService.h`

- [ ] **Step 1: Write the header.**

```cpp
#ifndef SerialWriterForwarderService_h
#define SerialWriterForwarderService_h

#include <WebSocketsClient.h>
#include <HTTPClient.h>

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <SettingValue.h>
#include <examples/serialwriter/SerialWriterForwarderState.h>

class SerialWriterService;

#define SERIALW_FWD_ENDPOINT_PATH "/rest/serialWriterSource"
#define SERIALW_FWD_CONFIG_FILE   "/config/serialWriterSource.json"

class SerialWriterForwarderService : public StatefulService<SerialWriterForwarderState> {
 public:
  SerialWriterForwarderService(AsyncWebServer* server,
                               FS* fs,
                               SecurityManager* securityManager,
                               SerialWriterService* writerService);
  void begin();
  void loop();

  // Called by UartModeService when entering / leaving Writer mode.
  void start();
  void stop();

 private:
  HttpEndpoint<SerialWriterForwarderState>  _httpEndpoint;
  FSPersistence<SerialWriterForwarderState> _fsPersistence;

  SerialWriterService* _writerService;
  WebSocketsClient     _wsClient;

  bool          _running = false;
  unsigned long _nextRetryMs = 0;
  uint16_t      _backoffMs   = 1000;  // 1s, 2s, 4s, ..., capped at 30s

  void onConfigChanged();
  void connectWs();
  void disconnectWs();
  void onWsEvent(WStype_t type, uint8_t* payload, size_t length);
  void scheduleRetry();
};

#endif
```

- [ ] **Step 2: Build.**

Run: `pio run -e esp32s3`
Expected: link will still fail for `.cpp` symbols. Header parses cleanly.

- [ ] **Step 3: Commit.**

```bash
git add src/examples/serialwriter/SerialWriterForwarderService.h
git commit -m "feat(serialwriter): add SerialWriterForwarderService header"
```

---

## Task 8 — SerialWriterForwarderService implementation

**Files:**
- Create: `src/examples/serialwriter/SerialWriterForwarderService.cpp`

- [ ] **Step 1: Write the implementation.**

```cpp
#include <examples/serialwriter/SerialWriterForwarderService.h>
#include <examples/serialwriter/SerialWriterService.h>

SerialWriterForwarderService::SerialWriterForwarderService(AsyncWebServer* server,
                                                           FS* fs,
                                                           SecurityManager* securityManager,
                                                           SerialWriterService* writerService) :
    _httpEndpoint(SerialWriterForwarderState::read,
                  SerialWriterForwarderState::update,
                  this,
                  server,
                  SERIALW_FWD_ENDPOINT_PATH,
                  securityManager),
    _fsPersistence(SerialWriterForwarderState::readConfig,
                   SerialWriterForwarderState::updateConfig,
                   this,
                   fs,
                   SERIALW_FWD_CONFIG_FILE),
    _writerService(writerService) {
  addUpdateHandler([this](const String& origin) { onConfigChanged(); }, false);
}

void SerialWriterForwarderService::begin() {
  _fsPersistence.readFromFS();
  // Do NOT start automatically — UartModeService::applyMode() calls start() when entering Writer mode.
}

void SerialWriterForwarderService::loop() {
  if (!_running) return;
  _wsClient.loop();

  // Reconnect backoff
  if (!_wsClient.isConnected() && millis() >= _nextRetryMs) {
    connectWs();
  }
}

void SerialWriterForwarderService::start() {
  bool shouldRun = false;
  read([&](const SerialWriterForwarderState& s) {
    shouldRun = s.enabled && s.sourceUrl.length() > 0;
  });
  if (!shouldRun) {
    _running = false;
    return;
  }
  _running = true;
  _backoffMs = 1000;
  connectWs();
}

void SerialWriterForwarderService::stop() {
  _running = false;
  disconnectWs();
}

void SerialWriterForwarderService::onConfigChanged() {
  _fsPersistence.writeToFS();
  if (_running) {
    disconnectWs();
    start();
  }
}

void SerialWriterForwarderService::connectWs() {
  // Parse URL: only WS supported in v1; HTTP polling stub for later.
  String url;
  uint8_t method = SERIALW_FWD_METHOD_WS;
  read([&](const SerialWriterForwarderState& s) { url = s.sourceUrl; method = s.connectionMethod; });
  if (method != SERIALW_FWD_METHOD_WS) {
    update([&](SerialWriterForwarderState& s) {
      s.lastError = "HTTP polling not implemented in v1";
      return StateUpdateResult::CHANGED;
    }, "fwd-error");
    return;
  }

  // Strip "ws://" prefix and split host:port + path.
  String host;
  uint16_t port = 80;
  String path = "/ws/serial";
  if (url.startsWith("ws://")) url = url.substring(5);
  int slash = url.indexOf('/');
  String hostPort = slash >= 0 ? url.substring(0, slash) : url;
  if (slash >= 0) path = url.substring(slash);
  int colon = hostPort.indexOf(':');
  if (colon >= 0) {
    host = hostPort.substring(0, colon);
    port = hostPort.substring(colon + 1).toInt();
  } else {
    host = hostPort;
  }

  // Add identity query so the Reader's Writers tab can show this device by name.
  String fullPath = path + "?role=writer&id=" + SettingValue::getUniqueId();

  _wsClient.begin(host, port, fullPath);
  _wsClient.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
    onWsEvent(type, payload, length);
  });
  _wsClient.setReconnectInterval(0);  // we manage backoff ourselves
}

void SerialWriterForwarderService::disconnectWs() {
  _wsClient.disconnect();
  update([](SerialWriterForwarderState& s) {
    s.connected = false;
    return StateUpdateResult::CHANGED;
  }, "fwd-disconnect");
}

void SerialWriterForwarderService::onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      update([](SerialWriterForwarderState& s) {
        s.connected = true;
        s.lastError = "";
        s.reconnectAttempts = 0;
        return StateUpdateResult::CHANGED;
      }, "fwd-connect");
      _backoffMs = 1000;
      break;

    case WStype_DISCONNECTED:
      update([](SerialWriterForwarderState& s) {
        s.connected = false;
        return StateUpdateResult::CHANGED;
      }, "fwd-disconnect");
      scheduleRetry();
      break;

    case WStype_TEXT: {
      String line((const char*)payload, length);
      // Extract last_line from JSON envelope (Reader's WebSocketTxRx broadcasts the full state).
      // Minimal parse: look for "last_line":"..." substring. Robust parse uses ArduinoJson.
      StaticJsonDocument<512> doc;
      if (deserializeJson(doc, line) == DeserializationError::Ok) {
        String lastLine = doc["last_line"] | "";
        if (lastLine.length() > 0 && _writerService) {
          _writerService->transmit(lastLine, TxSource::READER);
          update([&](SerialWriterForwarderState& s) {
            s.lastReceived = lastLine;
            s.lastReceivedAt = millis();
            return StateUpdateResult::CHANGED;
          }, "fwd-rx");
        }
      }
      break;
    }

    default:
      break;
  }
}

void SerialWriterForwarderService::scheduleRetry() {
  _nextRetryMs = millis() + _backoffMs;
  _backoffMs = _backoffMs >= 30000 ? 30000 : _backoffMs * 2;
  update([](SerialWriterForwarderState& s) {
    s.reconnectAttempts++;
    return StateUpdateResult::CHANGED;
  }, "fwd-retry");
}
```

- [ ] **Step 2: Build.**

Run: `pio run -e esp32s3`
Expected: should succeed end-to-end now (assuming `main.cpp` registrations are missing — that's Task 9). If main.cpp doesn't yet reference these classes, you'll get unused-class warnings; not errors.

- [ ] **Step 3: Commit.**

```bash
git add src/examples/serialwriter/SerialWriterForwarderService.cpp
git commit -m "feat(serialwriter): implement SerialWriterForwarderService — WS pull from source Reader with backoff"
```

---

## Task 9 — Wire Writer services in main.cpp

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Add includes near the top of main.cpp.**

Locate the existing `#include <examples/serial/SerialService.h>` and add below it:

```cpp
#include <examples/serialwriter/SerialWriterService.h>
#include <examples/serialwriter/SerialWriterForwarderService.h>
```

- [ ] **Step 2: Add service instantiations.**

Locate the existing `SerialService serialService(...)` declaration and add (right after):

```cpp
SerialWriterService serialWriterService(&server, &ESPFS, esp32sveltekit.getSecurityManager(), esp32sveltekit.getMqttClient());
SerialWriterForwarderService serialWriterForwarderService(&server, &ESPFS, esp32sveltekit.getSecurityManager(), &serialWriterService);
```

(Adjust to match this codebase's actual framework accessor names — check existing `serialService` instantiation for the pattern; replace `esp32sveltekit.getSecurityManager()` with whatever lookup the existing code uses, e.g. `esp32sveltekit.getSecurityManager()` or `&securityManager`.)

- [ ] **Step 3: Wire UartModeService to know about the Writer services.**

Locate `uartModeService.setSerialService(...)` and add below:

```cpp
uartModeService.setWriterService(&serialWriterService);
uartModeService.setForwarderService(&serialWriterForwarderService);
```

- [ ] **Step 4: Wire `begin()` and `loop()` calls.**

In `setup()`, after `serialService.begin();`:

```cpp
serialWriterService.begin();
serialWriterForwarderService.begin();
```

In `loop()`, after `serialService.loop();`:

```cpp
serialWriterService.loop();
serialWriterForwarderService.loop();
```

- [ ] **Step 5: Build.**

Run: `pio run -e esp32s3`
Expected: clean build. RAM/Flash usage should not have grown by more than ~20KB.

- [ ] **Step 6: Flash and smoke-test mode switching via REST.**

Flash: `pio run -e esp32s3 -t upload --upload-port <CURRENT-COM>` (or via OTA if WiFi is up).

After boot, with the device on a known IP:

```bash
# Read current mode (should be "" on a NEW unit, or "reader" after migration)
curl http://<device-ip>/rest/uartMode

# Switch to Writer
curl -X POST http://<device-ip>/rest/uartMode -H "Content-Type: application/json" -d '{"mode":"writer"}'

# Verify
curl http://<device-ip>/rest/uartMode
# Expected: {"mode":"writer"}

# Switch back to Reader
curl -X POST http://<device-ip>/rest/uartMode -H "Content-Type: application/json" -d '{"mode":"reader"}'
```

- [ ] **Step 7: Commit.**

```bash
git add src/main.cpp
git commit -m "feat(main): wire SerialWriterService and SerialWriterForwarderService"
```

---

## Task 10 — Frontend types: uartMode + serialWriter

**Files:**
- Modify: `interface/src/types/uartMode.ts`
- Create: `interface/src/types/serialWriter.ts`

- [ ] **Step 1: Update `interface/src/types/uartMode.ts`.**

Replace contents with:

```ts
export type UartMode = 'reader' | 'writer' | 'diagnostics' | '';

export interface UartModeData {
  mode: UartMode;  // '' = NEW (not yet configured)
}
```

- [ ] **Step 2: Create `interface/src/types/serialWriter.ts`.**

```ts
export type LineEnding = 0 | 1 | 2 | 3;  // None, CR, LF, CRLF

export type TxSource = 0 | 1 | 2 | 3 | 4 | 5;
// NONE, MANUAL, REST, WS, MQTT, READER

export interface SerialWriterData {
  baud_rate: number;
  data_bits: number;
  stop_bits: number;
  parity: number;
  line_ending: LineEnding;
  friendly_name: string;
  mqtt_subscribe_topic: string;
  mqtt_status_topic: string;
  mqtt_enabled: boolean;
  device_id: string;

  last_sent: string;
  last_sent_at: number;
  last_sent_source: TxSource;
  bytes_sent_total: number;
  suspended: number;
  serial_started: number;
}

export interface SerialWriterForwarderData {
  source_url: string;
  connection_method: 0 | 1;  // 0=WS, 1=HTTP
  auto_reconnect: boolean;
  enabled: boolean;

  connected: boolean;
  last_received_at: number;
  last_received: string;
  reconnect_attempts: number;
  last_error: string;
}
```

- [ ] **Step 3: Build the React app to verify types compile.**

```bash
cd interface
npm run build
```

Expected: build completes (eslint warnings OK). No TypeScript errors.

- [ ] **Step 4: Commit.**

```bash
git add interface/src/types/uartMode.ts interface/src/types/serialWriter.ts
git commit -m "feat(interface): add types for 3-way UART mode and SerialWriter"
```

---

## Task 11 — Frontend API clients

**Files:**
- Modify: `interface/src/api/uartMode.ts`
- Create: `interface/src/api/serialWriter.ts`

- [ ] **Step 1: Verify `interface/src/api/uartMode.ts` works with the new type.**

Open the file and confirm the function signatures use `UartModeData` from the types file. If the existing read/update functions accept any string, no change needed. If they restrict to `'live' | 'diagnostics'` literal strings, update them to use the new `UartMode` type.

- [ ] **Step 2: Create `interface/src/api/serialWriter.ts`.**

```ts
import { AxiosPromise } from 'axios';
import { AXIOS } from './endpoints';
import { SerialWriterData, SerialWriterForwarderData } from '../types/serialWriter';

export const SERIALW_REST_PATH = '/rest/serialWriter';
export const SERIALW_SEND_PATH = '/rest/serialWriter/send';
export const SERIALW_FWD_PATH  = '/rest/serialWriterSource';

export const readSerialWriter = (): AxiosPromise<SerialWriterData> =>
  AXIOS.get(SERIALW_REST_PATH);

export const updateSerialWriter = (data: Partial<SerialWriterData>): AxiosPromise<SerialWriterData> =>
  AXIOS.post(SERIALW_REST_PATH, data);

export const sendWriterMessage = (data: string): AxiosPromise<{ sent: boolean }> =>
  AXIOS.post(SERIALW_SEND_PATH, { data });

export const readWriterSource = (): AxiosPromise<SerialWriterForwarderData> =>
  AXIOS.get(SERIALW_FWD_PATH);

export const updateWriterSource = (data: Partial<SerialWriterForwarderData>): AxiosPromise<SerialWriterForwarderData> =>
  AXIOS.post(SERIALW_FWD_PATH, data);
```

- [ ] **Step 3: Build.**

```bash
cd interface
npm run build
```

Expected: clean.

- [ ] **Step 4: Commit.**

```bash
git add interface/src/api/serialWriter.ts interface/src/api/uartMode.ts
git commit -m "feat(interface): add API client for SerialWriter and SerialWriterForwarder"
```

---

## Task 12 — UartModeSwitcher: 3-way toggle with confirmation

**Files:**
- Modify: `interface/src/components/UartModeSwitcher.tsx`

- [ ] **Step 1: Replace the component to support three modes and show a confirmation dialog before switching.**

Key changes:
- Three `ToggleButton`s: Reader (Monitor icon), Writer (Send icon), Diagnostics (Build icon).
- When user clicks a different mode, open a Material `Dialog` with: "Switch to {Reader/Writer/Diagnostics}? Reading will pause." Confirm + Cancel.
- The component now derives "active" from the `data.mode` field directly; the `currentMode` prop becomes optional and is used only for the cross-mode warning banner.

Add a `useState<UartMode | null>(pendingMode)`; on click, set `pendingMode`; render a `Dialog` open when `pendingMode !== null`; on confirm, call `updateUartMode({mode: pendingMode})`; close dialog.

```tsx
import React, { useState } from 'react';
import { Alert, ToggleButtonGroup, ToggleButton, Box, Typography, CircularProgress, Dialog, DialogTitle, DialogContent, DialogActions, Button } from '@mui/material';
import MonitorIcon from '@mui/icons-material/Monitor';
import SendIcon from '@mui/icons-material/Send';
import BuildIcon from '@mui/icons-material/Build';
import { useRest } from '../utils';
import { readUartMode, updateUartMode } from '../api/uartMode';
import { UartMode, UartModeData } from '../types/uartMode';

interface UartModeSwitcherProps {
  currentMode?: UartMode;  // shown for cross-mode mismatch banner; optional
}

const labels: Record<Exclude<UartMode, ''>, string> = {
  reader: 'Reader',
  writer: 'Writer',
  diagnostics: 'Diagnostics'
};

const UartModeSwitcher: React.FC<UartModeSwitcherProps> = ({ currentMode }) => {
  const { loadData, saving, data, errorMessage } = useRest<UartModeData>({ read: readUartMode, update: updateUartMode });
  const [pending, setPending] = useState<Exclude<UartMode, ''> | null>(null);

  React.useEffect(() => { loadData(); }, [loadData]);

  const active = (data?.mode || 'reader') as Exclude<UartMode, ''>;

  const onChange = (_: any, newMode: Exclude<UartMode, ''> | null) => {
    if (newMode && newMode !== active) setPending(newMode);
  };

  const confirm = async () => {
    if (!pending) return;
    try {
      await updateUartMode({ mode: pending });
      await loadData();
    } finally {
      setPending(null);
    }
  };

  return (
    <Box mb={3}>
      {!data?.mode && (
        <Alert severity="info" sx={{ mb: 2 }}>
          <Typography variant="body2"><strong>First-time setup:</strong> pick a mode below to get started.</Typography>
        </Alert>
      )}

      <Box display="flex" alignItems="center" gap={2}>
        <ToggleButtonGroup value={active} exclusive onChange={onChange} aria-label="UART mode" disabled={saving} fullWidth>
          <ToggleButton value="reader" aria-label="reader"><MonitorIcon sx={{ mr: 1 }} />Reader</ToggleButton>
          <ToggleButton value="writer" aria-label="writer"><SendIcon sx={{ mr: 1 }} />Writer</ToggleButton>
          <ToggleButton value="diagnostics" aria-label="diagnostics"><BuildIcon sx={{ mr: 1 }} />Diagnostics</ToggleButton>
        </ToggleButtonGroup>
        {saving && <CircularProgress size={24} />}
      </Box>

      {errorMessage && <Alert severity="error" sx={{ mt: 2 }}>{errorMessage}</Alert>}

      {currentMode && data?.mode && currentMode !== data.mode && (
        <Alert severity="warning" sx={{ mt: 2 }}>
          Mode has changed. Current screen is for <strong>{currentMode}</strong>, but device is in <strong>{data.mode}</strong>.
        </Alert>
      )}

      <Dialog open={pending !== null} onClose={() => setPending(null)}>
        <DialogTitle>Switch to {pending && labels[pending]}?</DialogTitle>
        <DialogContent>
          <Typography>The current mode will pause and the new mode will take over the serial port. Saved settings are kept.</Typography>
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setPending(null)}>Cancel</Button>
          <Button onClick={confirm} variant="contained">Switch</Button>
        </DialogActions>
      </Dialog>
    </Box>
  );
};

export default UartModeSwitcher;
```

- [ ] **Step 2: Build.**

```bash
cd interface && npm run build
```

Expected: clean.

- [ ] **Step 3: Commit.**

```bash
git add interface/src/components/UartModeSwitcher.tsx
git commit -m "feat(interface): 3-way UART mode switcher with confirmation dialog"
```

---

## Task 13 — Mode-aware container in SerialMonitor.tsx

**Files:**
- Modify: `interface/src/examples/serial/SerialMonitor.tsx`

- [ ] **Step 1: Replace SerialMonitor to render Reader OR Writer container based on current mode.**

```tsx
import React, { FC, useEffect, useState } from 'react';
import { Box, CircularProgress } from '@mui/material';
import { useLayoutTitle } from '../../components';
import { readUartMode } from '../../api/uartMode';
import { UartMode } from '../../types/uartMode';
import SerialReader from './SerialReader';        // existing 5 reader tabs (renamed component)
import SerialWriter from '../serialwriter/SerialWriter';  // created in Task 14

const SerialMonitor: FC = () => {
  useLayoutTitle('Serial');
  const [mode, setMode] = useState<UartMode | null>(null);

  useEffect(() => {
    readUartMode().then((res) => setMode(res.data.mode)).catch(() => setMode('reader'));
  }, []);

  if (mode === null) return <Box p={3}><CircularProgress size={24} /></Box>;

  if (mode === 'writer')      return <SerialWriter />;
  if (mode === 'diagnostics') return <Box p={3}>Diagnostics mode is active. Open the Diagnostics top-level menu to run hardware tests.</Box>;
  // reader OR '' (NEW) → render reader screens; the mode switcher is inside, surfacing the NEW prompt
  return <SerialReader />;
};

export default SerialMonitor;
```

- [ ] **Step 2: Extract the existing SerialMonitor body into `SerialReader.tsx`.**

Create `interface/src/examples/serial/SerialReader.tsx`:

```tsx
import React, { FC } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';
import { Tab } from '@mui/material';
import { RouterTabs, useRouterTab } from '../../components';

import SerialInfoWithMode from './SerialInfoWithMode';
import SerialConfig from './SerialConfig';
import SerialRest from './SerialRest';
import SerialWebSocket from './SerialWebSocket';
import SerialBle from './SerialBle';

const SerialReader: FC = () => {
  const { routerTab } = useRouterTab();
  return (
    <>
      <RouterTabs value={routerTab}>
        <Tab value="information" label="Information" />
        <Tab value="configuration" label="Configuration" />
        <Tab value="rest" label="REST View" />
        <Tab value="stream" label="Live Stream" />
        <Tab value="ble" label="BLE Stream" />
      </RouterTabs>
      <Routes>
        <Route path="information" element={<SerialInfoWithMode />} />
        <Route path="configuration" element={<SerialConfig />} />
        <Route path="rest" element={<SerialRest />} />
        <Route path="stream" element={<SerialWebSocket />} />
        <Route path="ble" element={<SerialBle />} />
        <Route path="/*" element={<Navigate replace to="information" />} />
      </Routes>
    </>
  );
};

export default SerialReader;
```

- [ ] **Step 3: Build.**

```bash
cd interface && npm run build
```

Expected: build will fail because `SerialWriter.tsx` doesn't exist yet — fix in Task 14. To unblock builds in the meantime, temporarily comment out the `import SerialWriter` and the `'writer'` branch in `SerialMonitor.tsx`. Restore once Task 14 is done.

- [ ] **Step 4: Commit (after Task 14, since they belong together).**

Hold the commit until Task 14 lands.

---

## Task 14 — SerialWriter container (the writer tab shell)

**Files:**
- Create: `interface/src/examples/serialwriter/SerialWriter.tsx`

- [ ] **Step 1: Create the directory.**

```bash
mkdir -p interface/src/examples/serialwriter
```

- [ ] **Step 2: Write the container.**

```tsx
import React, { FC } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';
import { Box, Tab } from '@mui/material';
import { RouterTabs, useRouterTab, UartModeSwitcher } from '../../components';

import WriterStatus from './WriterStatus';
import WriterSettings from './WriterSettings';
import SendMessage from './SendMessage';
import SendViaWeb from './SendViaWeb';
import WriterLiveStream from './WriterLiveStream';
import WriterMqtt from './WriterMqtt';

const SerialWriter: FC = () => {
  const { routerTab } = useRouterTab();
  return (
    <Box>
      <UartModeSwitcher currentMode="writer" />
      <RouterTabs value={routerTab}>
        <Tab value="status"        label="Status" />
        <Tab value="settings"      label="Settings" />
        <Tab value="send-message"  label="Send Message" />
        <Tab value="send-via-web"  label="Send via Web" />
        <Tab value="live-stream"   label="Live Stream" />
        <Tab value="mqtt"          label="MQTT" />
      </RouterTabs>
      <Routes>
        <Route path="status"        element={<WriterStatus />} />
        <Route path="settings"      element={<WriterSettings />} />
        <Route path="send-message"  element={<SendMessage />} />
        <Route path="send-via-web"  element={<SendViaWeb />} />
        <Route path="live-stream"   element={<WriterLiveStream />} />
        <Route path="mqtt"          element={<WriterMqtt />} />
        <Route path="/*" element={<Navigate replace to="status" />} />
      </Routes>
    </Box>
  );
};

export default SerialWriter;
```

- [ ] **Step 3: Build.**

```bash
cd interface && npm run build
```

Expected: build will fail because `WriterStatus`, `WriterSettings`, etc. don't exist yet. To unblock, create stub files for each named tab: each is a one-liner placeholder component that returns `<div>{name}</div>`. Concretely:

Create six stub files in `interface/src/examples/serialwriter/`:

```tsx
// WriterStatus.tsx
import React from 'react'; const C = () => <div>Status (TBD)</div>; export default C;

// WriterSettings.tsx, SendMessage.tsx, SendViaWeb.tsx, WriterLiveStream.tsx, WriterMqtt.tsx — same pattern
```

The next tasks replace these stubs with real components.

- [ ] **Step 4: Build.**

```bash
cd interface && npm run build
```

Expected: clean (with stub components present).

- [ ] **Step 5: Commit Tasks 13 + 14 together.**

```bash
git add interface/src/examples/serial/SerialMonitor.tsx \
        interface/src/examples/serial/SerialReader.tsx \
        interface/src/examples/serialwriter/
git commit -m "feat(interface): mode-aware SerialMonitor with Writer container and stub tabs"
```

---

## Task 15 — WriterStatus.tsx

**Files:**
- Modify (replace stub): `interface/src/examples/serialwriter/WriterStatus.tsx`

- [ ] **Step 1: Write the real component.**

```tsx
import React, { FC, useEffect } from 'react';
import { Box, Card, CardContent, TextField, Typography, Alert, Chip, Button } from '@mui/material';
import { SectionContent, FormLoader } from '../../components';
import { useRest } from '../../utils';
import { readSerialWriter, updateSerialWriter, readWriterSource } from '../../api/serialWriter';
import { SerialWriterData, SerialWriterForwarderData } from '../../types/serialWriter';

const sourceLabels = ['none', 'manual', 'rest', 'ws', 'mqtt', 'reader'];

const WriterStatus: FC = () => {
  const { data, loadData, saveData, saving, setData, errorMessage } = useRest<SerialWriterData>({ read: readSerialWriter, update: updateSerialWriter });
  const [forwarder, setForwarder] = React.useState<SerialWriterForwarderData | null>(null);

  useEffect(() => {
    const tick = () => readWriterSource().then((r) => setForwarder(r.data)).catch(() => {});
    tick();
    const id = setInterval(tick, 2000);
    return () => clearInterval(id);
  }, []);

  if (!data) return <SectionContent title="Status" titleGutter><FormLoader onRetry={loadData} errorMessage={errorMessage} /></SectionContent>;

  return (
    <SectionContent title="Status" titleGutter>
      <Alert severity="info" sx={{ mb: 2 }}>This Writer's current state and link to its source Reader.</Alert>

      <Card sx={{ mb: 2 }}>
        <CardContent>
          <Typography variant="h6" gutterBottom>Friendly name</Typography>
          <TextField
            fullWidth
            value={data.friendly_name}
            onChange={(e) => setData((p) => p ? { ...p, friendly_name: e.target.value } : p)}
            placeholder={data.device_id}
            helperText="Shown on the source Reader's Writers tab."
          />
          <Box mt={1}><Button variant="contained" onClick={saveData} disabled={saving}>Save name</Button></Box>
        </CardContent>
      </Card>

      <Card sx={{ mb: 2 }}>
        <CardContent>
          <Typography variant="h6" gutterBottom>Source Reader</Typography>
          {forwarder ? (
            forwarder.connected ? (
              <Alert severity="success">Connected to <code>{forwarder.source_url}</code> · last received <code>{forwarder.last_received || '(none yet)'}</code></Alert>
            ) : forwarder.enabled ? (
              <Alert severity="warning">Reconnecting (attempt {forwarder.reconnect_attempts})… {forwarder.last_error}</Alert>
            ) : (
              <Alert severity="info">No source Reader configured. Open Settings to set one.</Alert>
            )
          ) : (
            <Typography color="text.secondary">Loading…</Typography>
          )}
        </CardContent>
      </Card>

      <Card>
        <CardContent>
          <Typography variant="h6" gutterBottom>Local outbound</Typography>
          <Typography>Last sent: <code>{data.last_sent || '(nothing yet)'}</code></Typography>
          <Typography>Last source: <Chip size="small" label={sourceLabels[data.last_sent_source] || 'none'} /></Typography>
          <Typography>Total bytes sent: {data.bytes_sent_total}</Typography>
        </CardContent>
      </Card>
    </SectionContent>
  );
};

export default WriterStatus;
```

- [ ] **Step 2: Build.**

```bash
cd interface && npm run build
```

Expected: clean.

- [ ] **Step 3: Commit.**

```bash
git add interface/src/examples/serialwriter/WriterStatus.tsx
git commit -m "feat(interface): WriterStatus tab"
```

---

## Task 16 — WriterSettings.tsx

**Files:**
- Modify (replace stub): `interface/src/examples/serialwriter/WriterSettings.tsx`

- [ ] **Step 1: Write the real component.**

```tsx
import React, { FC, useEffect } from 'react';
import { Box, Card, CardContent, TextField, MenuItem, FormControl, FormLabel, RadioGroup, FormControlLabel, Radio, Switch, Button, Alert, Typography } from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';
import { SectionContent, FormLoader, ButtonRow } from '../../components';
import { useRest } from '../../utils';
import { readSerialWriter, updateSerialWriter, readWriterSource, updateWriterSource } from '../../api/serialWriter';
import { SerialWriterData, SerialWriterForwarderData } from '../../types/serialWriter';

const BAUDRATES = [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400];

const WriterSettings: FC = () => {
  const { data, loadData, saveData, saving, setData, errorMessage } = useRest<SerialWriterData>({ read: readSerialWriter, update: updateSerialWriter });
  const [src, setSrc] = React.useState<SerialWriterForwarderData | null>(null);
  const [savingSrc, setSavingSrc] = React.useState(false);

  useEffect(() => { readWriterSource().then((r) => setSrc(r.data)).catch(() => {}); }, []);

  if (!data) return <SectionContent title="Settings" titleGutter><FormLoader onRetry={loadData} errorMessage={errorMessage} /></SectionContent>;

  const setField = <K extends keyof SerialWriterData>(k: K) => (v: SerialWriterData[K]) =>
    setData((p) => p ? { ...p, [k]: v } : p);

  const setSrcField = <K extends keyof SerialWriterForwarderData>(k: K) => (v: SerialWriterForwarderData[K]) =>
    setSrc((p) => p ? { ...p, [k]: v } : p);

  const saveSrc = async () => {
    if (!src) return;
    setSavingSrc(true);
    try { await updateWriterSource(src); } finally { setSavingSrc(false); }
  };

  return (
    <SectionContent title="Settings" titleGutter>
      <Alert severity="warning" sx={{ mb: 2 }}>Changing serial port settings restarts the connection.</Alert>

      <Card sx={{ mb: 2 }}>
        <CardContent>
          <Typography variant="h6" gutterBottom>Source Reader</Typography>
          {src && (
            <>
              <TextField fullWidth label="Source Reader URL" placeholder="ws://192.168.2.50/ws/serial"
                value={src.source_url} onChange={(e) => setSrcField('source_url')(e.target.value)} sx={{ mb: 2 }} />
              <FormControlLabel control={<Switch checked={src.enabled} onChange={(e) => setSrcField('enabled')(e.target.checked)} />} label="Enabled" />
              <FormControlLabel control={<Switch checked={src.auto_reconnect} onChange={(e) => setSrcField('auto_reconnect')(e.target.checked)} />} label="Auto-reconnect" />
              <ButtonRow mt={1}>
                <Button startIcon={<SaveIcon />} variant="contained" disabled={savingSrc} onClick={saveSrc}>Save source</Button>
              </ButtonRow>
            </>
          )}
        </CardContent>
      </Card>

      <Card>
        <CardContent>
          <Typography variant="h6" gutterBottom>Serial port</Typography>
          <TextField select fullWidth label="Baud rate" value={data.baud_rate} onChange={(e) => setField('baud_rate')(Number(e.target.value))} sx={{ mb: 2 }}>
            {BAUDRATES.map((r) => <MenuItem key={r} value={r}>{r}</MenuItem>)}
          </TextField>

          <FormControl component="fieldset" sx={{ mb: 2 }}>
            <FormLabel>Data bits</FormLabel>
            <RadioGroup row value={String(data.data_bits)} onChange={(e) => setField('data_bits')(Number(e.target.value))}>
              {[5,6,7,8].map((n) => <FormControlLabel key={n} value={String(n)} control={<Radio />} label={String(n)} />)}
            </RadioGroup>
          </FormControl>

          <FormControl component="fieldset" sx={{ mb: 2 }}>
            <FormLabel>Stop bits</FormLabel>
            <RadioGroup row value={String(data.stop_bits)} onChange={(e) => setField('stop_bits')(Number(e.target.value))}>
              <FormControlLabel value="1" control={<Radio />} label="1" />
              <FormControlLabel value="2" control={<Radio />} label="2" />
            </RadioGroup>
          </FormControl>

          <FormControl component="fieldset" sx={{ mb: 2 }}>
            <FormLabel>Parity</FormLabel>
            <RadioGroup row value={String(data.parity)} onChange={(e) => setField('parity')(Number(e.target.value))}>
              <FormControlLabel value="0" control={<Radio />} label="None" />
              <FormControlLabel value="1" control={<Radio />} label="Even" />
              <FormControlLabel value="2" control={<Radio />} label="Odd" />
            </RadioGroup>
          </FormControl>

          <TextField select fullWidth label="Line ending" value={data.line_ending} onChange={(e) => setField('line_ending')(Number(e.target.value) as any)}>
            <MenuItem value={0}>None</MenuItem>
            <MenuItem value={1}>CR (\r)</MenuItem>
            <MenuItem value={2}>LF (\n)</MenuItem>
            <MenuItem value={3}>CRLF (\r\n)</MenuItem>
          </TextField>

          <ButtonRow mt={2}>
            <Button startIcon={<SaveIcon />} variant="contained" disabled={saving} onClick={saveData}>Save serial port</Button>
          </ButtonRow>
        </CardContent>
      </Card>
    </SectionContent>
  );
};

export default WriterSettings;
```

- [ ] **Step 2: Build.**

```bash
cd interface && npm run build
```

Expected: clean.

- [ ] **Step 3: Commit.**

```bash
git add interface/src/examples/serialwriter/WriterSettings.tsx
git commit -m "feat(interface): WriterSettings tab (source Reader + serial port)"
```

---

## Task 17 — SendMessage.tsx

**Files:**
- Modify (replace stub): `interface/src/examples/serialwriter/SendMessage.tsx`

- [ ] **Step 1: Write the component.**

```tsx
import React, { FC, useState, useEffect } from 'react';
import { Box, Card, CardContent, TextField, Button, Alert, Chip, Typography } from '@mui/material';
import SendIcon from '@mui/icons-material/Send';
import { SectionContent } from '../../components';
import { sendWriterMessage } from '../../api/serialWriter';

const RECENT_MAX = 8;

const SendMessage: FC = () => {
  const [msg, setMsg] = useState('');
  const [recent, setRecent] = useState<string[]>([]);
  const [lastResult, setLastResult] = useState<{ ok: boolean; text: string } | null>(null);
  const [sending, setSending] = useState(false);

  const send = async (text?: string) => {
    const payload = (text ?? msg).trim();
    if (payload.length === 0) return;
    setSending(true);
    try {
      await sendWriterMessage(payload);
      setLastResult({ ok: true, text: payload });
      setRecent((r) => {
        const next = [payload, ...r.filter((x) => x !== payload)];
        return next.slice(0, RECENT_MAX);
      });
      if (text === undefined) setMsg('');
    } catch (e: any) {
      setLastResult({ ok: false, text: e?.response?.data?.error || 'Failed to send' });
    } finally {
      setSending(false);
    }
  };

  return (
    <SectionContent title="Send Message" titleGutter>
      <Alert severity="info" sx={{ mb: 2 }}>
        Most scales respond to <code>P</code> to print weight, or <code>ZERO</code> to tare.
      </Alert>

      <Card sx={{ mb: 2 }}>
        <CardContent>
          <TextField
            fullWidth
            multiline
            minRows={1}
            label="Message"
            value={msg}
            onChange={(e) => setMsg(e.target.value)}
            onKeyDown={(e) => { if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); send(); } }}
            placeholder="Type something… Enter to send · Shift+Enter for newline"
            sx={{ mb: 2 }}
          />
          <Box display="flex" justifyContent="flex-end">
            <Button variant="contained" startIcon={<SendIcon />} disabled={sending || msg.trim().length === 0} onClick={() => send()}>Send</Button>
          </Box>
        </CardContent>
      </Card>

      {lastResult && (
        <Alert severity={lastResult.ok ? 'success' : 'error'} sx={{ mb: 2 }}>
          {lastResult.ok ? 'Sent — ' : 'Failed — '} <code>{lastResult.text}</code>
        </Alert>
      )}

      {recent.length > 0 && (
        <Card>
          <CardContent>
            <Typography variant="body2" color="text.secondary" sx={{ mb: 1 }}>Recent — click to send again</Typography>
            <Box display="flex" gap={1} flexWrap="wrap">
              {recent.map((r) => <Chip key={r} label={r} clickable onClick={() => send(r)} />)}
            </Box>
          </CardContent>
        </Card>
      )}
    </SectionContent>
  );
};

export default SendMessage;
```

- [ ] **Step 2: Build.**

```bash
cd interface && npm run build
```

- [ ] **Step 3: Commit.**

```bash
git add interface/src/examples/serialwriter/SendMessage.tsx
git commit -m "feat(interface): SendMessage tab"
```

---

## Task 18 — SendViaWeb.tsx

**Files:**
- Modify (replace stub): `interface/src/examples/serialwriter/SendViaWeb.tsx`

- [ ] **Step 1: Write the component.**

```tsx
import React, { FC, useState } from 'react';
import { Card, CardContent, TextField, Button, Alert, Typography, Box } from '@mui/material';
import SendIcon from '@mui/icons-material/Send';
import ContentCopyIcon from '@mui/icons-material/ContentCopy';
import { SectionContent } from '../../components';
import { sendWriterMessage, SERIALW_SEND_PATH } from '../../api/serialWriter';

const SendViaWeb: FC = () => {
  const [msg, setMsg] = useState('');
  const [result, setResult] = useState<{ ok: boolean; text: string } | null>(null);
  const [sending, setSending] = useState(false);

  const send = async () => {
    if (msg.trim().length === 0) return;
    setSending(true);
    try {
      await sendWriterMessage(msg);
      setResult({ ok: true, text: msg });
    } catch (e: any) {
      setResult({ ok: false, text: e?.response?.data?.error || 'Failed' });
    } finally {
      setSending(false);
    }
  };

  const curlExample = `curl -X POST http://<device-ip>${SERIALW_SEND_PATH} \\\n  -H "Content-Type: application/json" \\\n  -d '{"data":"P"}'`;

  return (
    <SectionContent title="Send via Web (HTTP)" titleGutter>
      <Alert severity="info" sx={{ mb: 2 }}>POST a message and the device writes it to the serial port.</Alert>

      <Card sx={{ mb: 2 }}>
        <CardContent>
          <Typography variant="caption" color="text.secondary">Endpoint</Typography>
          <Typography sx={{ fontFamily: 'monospace', mb: 2 }}><strong>POST</strong> {SERIALW_SEND_PATH}<br/>Body: <code>{`{"data":"..."}`}</code></Typography>

          <Typography variant="subtitle2" gutterBottom>Try it</Typography>
          <TextField fullWidth label="Message" value={msg} onChange={(e) => setMsg(e.target.value)} sx={{ mb: 2 }} />
          <Box display="flex" justifyContent="flex-end">
            <Button variant="contained" startIcon={<SendIcon />} disabled={sending || msg.trim().length === 0} onClick={send}>Send</Button>
          </Box>
        </CardContent>
      </Card>

      {result && <Alert severity={result.ok ? 'success' : 'error'} sx={{ mb: 2 }}>{result.ok ? `Sent: ${result.text}` : result.text}</Alert>}

      <Card>
        <CardContent>
          <Typography variant="subtitle2" gutterBottom>Example with curl</Typography>
          <Box position="relative">
            <pre style={{ background: '#f5f5f5', padding: 12, fontSize: 12, overflowX: 'auto', margin: 0 }}>{curlExample}</pre>
            <Button size="small" startIcon={<ContentCopyIcon />} sx={{ position: 'absolute', top: 4, right: 4 }} onClick={() => navigator.clipboard?.writeText(curlExample)}>Copy</Button>
          </Box>
        </CardContent>
      </Card>
    </SectionContent>
  );
};

export default SendViaWeb;
```

- [ ] **Step 2: Build & commit.**

```bash
cd interface && npm run build
git add interface/src/examples/serialwriter/SendViaWeb.tsx
git commit -m "feat(interface): SendViaWeb tab"
```

---

## Task 19 — WriterLiveStream.tsx

**Files:**
- Modify (replace stub): `interface/src/examples/serialwriter/WriterLiveStream.tsx`

- [ ] **Step 1: Write the component (uses WebSocket to /ws/serialWriter for inbound TX events).**

```tsx
import React, { FC, useEffect, useRef, useState } from 'react';
import { Box, Card, CardContent, Chip, Button, Typography, Alert, TextField } from '@mui/material';
import { SectionContent } from '../../components';

interface TxEvent { ts: number; source: string; data: string; bytes: number; }

const sourceColor = (s: string): 'primary' | 'info' | 'warning' | 'success' | 'secondary' | 'default' => {
  switch (s) {
    case 'reader': return 'primary';
    case 'manual': return 'info';
    case 'rest':   return 'warning';
    case 'ws':     return 'success';
    case 'mqtt':   return 'secondary';
    default:       return 'default';
  }
};

const MAX_LINES = 100;

const WriterLiveStream: FC = () => {
  const [events, setEvents] = useState<TxEvent[]>([]);
  const [connected, setConnected] = useState(false);
  const [outgoing, setOutgoing] = useState('');
  const wsRef = useRef<WebSocket | null>(null);

  useEffect(() => {
    const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const ws = new WebSocket(`${proto}//${window.location.host}/ws/serialWriter`);
    wsRef.current = ws;
    ws.onopen = () => setConnected(true);
    ws.onclose = () => setConnected(false);
    ws.onmessage = (m) => {
      try {
        const obj = JSON.parse(m.data);
        if (obj.type === 'tx') {
          setEvents((prev) => [{ ts: Date.now(), source: obj.source ?? '', data: obj.data ?? '', bytes: (obj.data ?? '').length }, ...prev].slice(0, MAX_LINES));
        }
      } catch { /* ignore non-JSON frames */ }
    };
    return () => ws.close();
  }, []);

  const sendThroughWs = () => {
    if (!wsRef.current || wsRef.current.readyState !== WebSocket.OPEN) return;
    const text = outgoing.trim();
    if (text.length === 0) return;
    wsRef.current.send(JSON.stringify({ data: text }));
    setOutgoing('');
  };

  return (
    <SectionContent title="Live Stream" titleGutter>
      <Alert severity="info" sx={{ mb: 2 }}>Every message that left this Writer's serial port — by anyone.</Alert>

      <Alert severity={connected ? 'success' : 'warning'} sx={{ mb: 2 }}>
        {connected ? 'Connected · WebSocket /ws/serialWriter' : 'Connecting…'}
      </Alert>

      <Card sx={{ mb: 2 }}>
        <CardContent>
          <Typography variant="subtitle2" gutterBottom>Send through this socket</Typography>
          <Box display="flex" gap={1}>
            <TextField fullWidth size="small" value={outgoing} onChange={(e) => setOutgoing(e.target.value)} onKeyDown={(e) => { if (e.key === 'Enter') sendThroughWs(); }} placeholder="type message…" />
            <Button variant="contained" disabled={!connected || outgoing.trim().length === 0} onClick={sendThroughWs}>Send</Button>
          </Box>
        </CardContent>
      </Card>

      <Card>
        <CardContent>
          <Box display="flex" justifyContent="space-between" alignItems="center" mb={1}>
            <Typography variant="subtitle2">Recent activity (last {MAX_LINES})</Typography>
            <Button size="small" onClick={() => setEvents([])}>Clear</Button>
          </Box>
          <Box sx={{ maxHeight: 320, overflow: 'auto', fontFamily: 'monospace', fontSize: 12, border: '1px solid #eee', p: 1, borderRadius: 1 }}>
            {events.length === 0 ? <Typography color="text.secondary">Waiting for activity…</Typography> :
              events.map((e, i) => (
                <Box key={i} display="flex" gap={1} alignItems="center" mb={0.5}>
                  <Typography component="span" color="text.secondary" variant="caption">[{new Date(e.ts).toLocaleTimeString()}]</Typography>
                  <Chip size="small" color={sourceColor(e.source)} label={e.source.toUpperCase()} sx={{ minWidth: 70 }} />
                  <Typography component="span" sx={{ wordBreak: 'break-all' }}>{e.data}</Typography>
                  <Typography component="span" color="text.secondary" variant="caption">→ {e.bytes} bytes</Typography>
                </Box>
              ))
            }
          </Box>
        </CardContent>
      </Card>
    </SectionContent>
  );
};

export default WriterLiveStream;
```

- [ ] **Step 2: Build & commit.**

```bash
cd interface && npm run build
git add interface/src/examples/serialwriter/WriterLiveStream.tsx
git commit -m "feat(interface): WriterLiveStream tab"
```

---

## Task 20 — WriterMqtt.tsx

**Files:**
- Modify (replace stub): `interface/src/examples/serialwriter/WriterMqtt.tsx`

- [ ] **Step 1: Write the component.**

```tsx
import React, { FC, useEffect } from 'react';
import { Card, CardContent, TextField, Switch, FormControlLabel, Button, Alert, Typography } from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';
import { SectionContent, FormLoader, ButtonRow } from '../../components';
import { useRest } from '../../utils';
import { readSerialWriter, updateSerialWriter } from '../../api/serialWriter';
import { SerialWriterData } from '../../types/serialWriter';

const WriterMqtt: FC = () => {
  const { data, loadData, saveData, saving, setData, errorMessage } = useRest<SerialWriterData>({ read: readSerialWriter, update: updateSerialWriter });

  if (!data) return <SectionContent title="MQTT" titleGutter><FormLoader onRetry={loadData} errorMessage={errorMessage} /></SectionContent>;

  const setField = <K extends keyof SerialWriterData>(k: K) => (v: SerialWriterData[K]) =>
    setData((p) => p ? { ...p, [k]: v } : p);

  return (
    <SectionContent title="MQTT" titleGutter>
      <Alert severity="info" sx={{ mb: 2 }}>Subscribe to a topic and any message it receives gets written to the serial port.</Alert>

      <Card>
        <CardContent>
          <FormControlLabel control={<Switch checked={data.mqtt_enabled} onChange={(e) => setField('mqtt_enabled')(e.target.checked)} />} label="Enabled" sx={{ mb: 2, display: 'block' }} />

          <TextField fullWidth label="Subscribe topic" value={data.mqtt_subscribe_topic} onChange={(e) => setField('mqtt_subscribe_topic')(e.target.value)}
            helperText={`Suggested: weighsoft/serialwriter/${data.device_id}/send`} sx={{ mb: 2 }} />

          <TextField fullWidth label="Status publish topic" value={data.mqtt_status_topic} onChange={(e) => setField('mqtt_status_topic')(e.target.value)}
            helperText={`Suggested: weighsoft/serialwriter/${data.device_id}/status`} sx={{ mb: 2 }} />

          <ButtonRow><Button startIcon={<SaveIcon />} variant="contained" disabled={saving} onClick={saveData}>Save MQTT</Button></ButtonRow>
        </CardContent>
      </Card>
    </SectionContent>
  );
};

export default WriterMqtt;
```

- [ ] **Step 2: Build & commit.**

```bash
cd interface && npm run build
git add interface/src/examples/serialwriter/WriterMqtt.tsx
git commit -m "feat(interface): WriterMqtt tab"
```

---

## Task 21 — End-to-end smoke test

**Files:** none (verification only)

- [ ] **Step 1: Build the firmware once with everything wired.**

```bash
pio run -e esp32s3
```

Expected: clean build. Note RAM/Flash deltas.

- [ ] **Step 2: Flash the device.**

`pio run -e esp32s3 -t upload --upload-port <CURRENT-COM>` (or via OTA).

- [ ] **Step 3: Verify NEW state on first boot of a freshly-erased device.**

(Skip if you don't want to wipe config.) `pio run -e esp32s3 -t erase` then re-flash. Open the device's UI and confirm:
- The Serial section shows the mode switcher with no mode selected.
- An info banner says: "First-time setup: pick a mode below to get started."

- [ ] **Step 4: Pick Reader mode in the UI. Confirm the existing Reader tabs appear.**

Browse to Information / Configuration / REST View / Live Stream / BLE Stream — all should work as they did before.

- [ ] **Step 5: Switch to Writer mode using the toggle.**

Confirm:
- Confirmation dialog appears.
- After confirming, the URL stays at `/serial/...` but the tab bar now shows: Status · Settings · Send Message · Send via Web · Live Stream · MQTT.
- Reader tabs are gone.

- [ ] **Step 6: On the Writer tab, enter a Source Reader URL pointing to a second device running Reader.**

Open Settings → Source Reader, paste e.g. `ws://<reader-ip>/ws/serial`, enable, save.
Open Status → confirm "Connected to ws://…"
Open Live Stream → confirm new lines appear tagged READER as the Reader gets serial data from its scale.

- [ ] **Step 7: Test local channels.**

- Send Message: type "P", press Send. Confirm "Sent" alert + chip in Recent.
- Send via Web: try the in-page form, then via curl from a separate terminal.
- MQTT: configure a topic, publish via mosquitto_pub, confirm it appears on the wire.
- Live Stream: confirm each test entry appears tagged with the right source (manual / rest / mqtt / ws).

- [ ] **Step 8: Switch back to Reader and verify the original behavior is fully restored.**

Confirm scale data starts streaming again.

- [ ] **Step 9: Reboot the device and confirm mode persists.**

Power-cycle. After boot, confirm the device is still in the last-set mode.

- [ ] **Step 10: Commit a verification note (optional).**

```bash
git commit --allow-empty -m "chore: phase-1 end-to-end smoke test passed"
```

---

## Self-review notes

- Every spec section in [`2026-04-30-multi-mode-serial-reader-writer-design.md`](../specs/2026-04-30-multi-mode-serial-reader-writer-design.md) Section 2–7 is implemented above. Sections 3.1 (Reader's Writers tab), 5.4 (HTTP polling fallback as a stub only), 8 (hotspot mode), and parts of 9 (knownWriters.json on the Reader side) are deliberately deferred to Phase 2 / Phase 3 plans.
- Task 5 contains two stubbed methods (`registerWsSink`, `broadcastTxEvent`) that direct the implementer to mirror an existing pattern in `SerialService.cpp`. This is intentional — the framework's WS plumbing has nuances best learned from reading the existing code rather than reinventing.
- Task 9 includes adapt-to-codebase notes for the framework accessor names (`esp32sveltekit.getSecurityManager()` etc.). The implementer must check the existing `main.cpp` for the actual symbol name in this project.
- Task 13 + 14 are split for clarity but committed together; intermediate commits would not compile.
- All 6 Writer screens (Tasks 15–20) follow the same MUI Material Design patterns already used by `SerialConfig.tsx` to honor the project's existing aesthetic.

---

## Out of scope for this plan (covered by future plans)

| Feature | Plan |
|---|---|
| Reader's persistent Writers tab + Forget | Phase 2 plan |
| Hotspot mode + auto-off-on-WiFi | Phase 3 plan |
| mDNS auto-discovery | Phase 3 plan |
| BLE input on Writer | not planned for v1 |
| Scheduled / periodic auto-send | not planned for v1 |
| HTTP polling fallback (only stubbed in Task 8) | follow-on if needed |
