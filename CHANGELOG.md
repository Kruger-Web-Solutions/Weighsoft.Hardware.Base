# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.8.0] - 2026-02-09 (weighingboard branch)

### Added
- **Weighing Board Service** (`src/examples/weighing/`):
  - `ADS1231.h/.cpp`: Bit-banged driver for ADS1231 24-bit load cell ADC (DOUT=GPIO25, SCLK=GPIO23, PDWN=GPIO22, SPEED=GPIO19). Supports 10 SPS / 80 SPS, power management, averaged reads, and timeout-based connection detection.
  - `WeighingState.h`: State with live measurement (gross, net, tare, stability, overload, underload), calibration config, seal state, and event counter. Factory PIN verified via SHA-256 at runtime. Event counter is monotonic (never decreases on load).
  - `AuditTrail.h/.cpp`: Append-only JSON log at `/audit/weighing.json` (LittleFS, survives OTA). NTP timestamps with `NO_NTP_<millis>` fallback. FIFO at 100 entries.
  - `WeighingService.h/.cpp`: Composes `HttpEndpoint` (IS_AUTHENTICATED), `FSPersistence`, `WebSocketTxRx`, `MqttPubSub`, `BlePubSub`. `loop()` reads ADC, computes stability/AZT/overload. Separate `GET /rest/weighingAudit` endpoint (IS_ADMIN). Full calibration, tare, zero, seal/unseal operations.

- **Version Service** (`src/VersionService.h/.cpp`, `src/version.h`):
  - Public `GET /rest/version` endpoint for OIML 6.2.1 software identification.

- **Factory calibration PIN** in `factory_settings.ini`:
  - `FACTORY_CALIBRATION_PIN` define (default `123456`). Immutable at runtime (requires rebuild to change).

- **Frontend (weighingboard)**:
  - 8 new components: `Weighing.tsx` (7-tab container), `WeighingInfo.tsx`, `WeighingConfig.tsx`, `WeighingCalibration.tsx`, `WeighingRest.tsx`, `WeighingLive.tsx`, `WeighingAudit.tsx`, `WeighingBle.tsx`
  - TypeScript types in `interface/src/types/weighing.ts`
  - REST API helpers in `interface/src/api/weighing.ts`
  - `WEIGHING_SOCKET_PATH` added to `endpoints.ts`
  - `ProjectMenu.tsx`: Added "Weighing" item with `ScaleIcon`
  - `ProjectRouting.tsx`: Added `weighing/*` route (default remains `led-example/information`)

- **Documentation**:
  - `docs/WEIGHING-EXAMPLE.md`: 18-section guide covering hardware setup, calibration procedure, weighing operations, seal/type approval, full API reference (REST/WS/MQTT/BLE), configuration, testing, troubleshooting, performance, and OIML compliance matrix.

### Changed
- `src/main.cpp`: LED example kept; `WeighingService` and `VersionService` added alongside it. Single BLE callback configures both LED and Weighing BLE services.



### Added
- **General Documentation from Serial Branch**:
  - OTA-UPLOAD.md: Complete OTA firmware upload guide (ESP32-S3, port 8266, troubleshooting)
  - PIN-CONFIGURATION.md: GPIO and hardware pin reference for Serial2, I2C, and platform-specific pins
  - PLATFORM-GPIO.md: Platform comparison (ESP8266, ESP32, ESP32-S3) and partition schemes
  - DIAGNOSTICS-EXAMPLE.md: UART diagnostics example with loopback, baud detection, signal quality
  - WEIGHT-FORWARDER-LESSONS.md: Multi-protocol weight forwarding patterns (WebSocket, SSE, UDP, MQTT)
  
- **Rules for Feature Branch Management**:
  - feature-branch-workflow.mdc: Complete workflow for creating and managing device-specific branches
  - version-strategy.mdc: SemVer strategy for master and feature branches
  - ota-upload.mdc: OTA upload guidelines and troubleshooting
  - platformio-upload.mdc: USB upload guidelines (kill Python before upload)

- **Documentation Index Updates**:
  - Updated docs/README.md with all general docs and examples
  - Updated docs/FILE-REFERENCE.md with hardware docs references
  - Updated .cursor/rules/documentation.mdc with complete doc inventory

### Changed
- LED-EXAMPLE.md: Improvements and clarifications from serial branch

### Framework
- Established clear branching model: master for framework, feature branches for devices
- Version strategy: master = framework maturity, feature branches = feature completeness
- All general improvements from serial branch now available for future projects (display, etc.)

## [0.1.0] - Previous

- Initial framework release
- Core services: WiFi, MQTT, NTP, OTA, Security
- React frontend with Material-UI
- LED Example implementation
