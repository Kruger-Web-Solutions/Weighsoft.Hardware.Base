# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- (Future changes will be listed here)

### Changed

- (Future changes will be listed here)

## [0.7.0] - 2026-02-17

### Added

- **General Documentation from Serial Branch merged to Master**:
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
- Serial branch synced with master (version 0.7.0)

### Framework

- Established clear branching model: master for framework, feature branches for devices
- Version strategy: master = framework maturity, feature branches = feature completeness
- All general improvements from serial branch now available for future projects (display, etc.)

## [0.6.1] - 2026-02-17

### Changed

- **Serial REST API**: POST `/rest/serial` now accepts only config fields (baud_rate, data_bits, stop_bits, parity, regex_pattern); UI and API layer send config-only payload to avoid 400 when last_line contains binary/non-UTF-8.
- **Serial Configuration UI**: Current Data Preview polls every 2s so last_line and extracted weight stay in sync with the device; baud rate Select uses `Number(data.baud_rate)` so 9600 displays correctly after refresh.
- **Serial display**: `last_line` with binary/non-printable data is sanitized for display (control chars shown as middle dot) in Configuration and REST view.

### Added

- **OTA documentation**: `docs/OTA-UPLOAD.md` with device setup, platformio.ini, and upload command; CONFIGURATION.md and docs/README.md updated; `.cursor/rules/ota-upload.mdc` so OTA (port 8266, esp32s3) is always documented.

### Fixed

- POST `/rest/serial` 400 Bad Request when full state (including last_line) was sent.
- Serial Configuration page showing "(none yet)" and wrong baud rate (115200) when device had data and 9600 configured.

## [0.6.0] - 2026-02-17

### Added

- **Weight Stream Forwarder**: Multi-protocol weight data distribution service
  - WebSocket broadcasting to connected clients
  - HTTP Server-Sent Events (SSE) streaming
  - UDP unicast/broadcast support
  - MQTT publishing with retained messages
  - Frontend UI with real-time weight display and protocol status indicators
  - REST API (`/rest/weightForwarder`) and WebSocket (`/ws/weightForwarder`)
  - Comprehensive documentation in `WEIGHT-FORWARDER-LESSONS.md`
- **PowerShell Serial Testing Utilities**: Development tooling for UART debugging
  - `test-serial-stream.ps1` for simulating scale data output
  - Configurable weight patterns and update rates
  - Line ending options (CRLF/LF) for testing serial parsing

### Changed

- Updated `FRONTEND-PATTERNS.md` with weight forwarder context management examples
- Fixed Weight Forwarder frontend to use features context and correct useWs hook

## [0.5.0] - 2026-02-15

### Added

- **ESP32-S3 Platform Support**: Extended multi-platform capabilities
  - Optimized partition scheme for ESP32-S3
  - Platform-specific configurations in `platformio.ini`
  - Documentation updated in `docs/` for ESP32-S3 specifics

## [0.4.0] - 2026-02-14

### Added

- **Comprehensive UART Documentation**: Major documentation overhaul
  - Updated `SERIAL-EXAMPLE.md` with 525 lines of detailed UART mode system documentation
  - Architecture diagrams, API references, troubleshooting guides
  - Frontend integration patterns and WebSocket communication details

## [0.3.0] - 2026-02-13

### Added

- **UART Mode Management**: Persistent mode switcher (Live Monitoring vs Diagnostics)
  - `UartModeService` for backend state management with flash persistence (`/config/uartMode.json`)
  - `UartModeSwitcher` React component for UI mode control
  - REST API (`/rest/uartMode`) and WebSocket (`/ws/uartMode`) endpoints
  - Automatic mode application on boot based on persisted setting
  - Mode switcher integrated into Serial and Diagnostics info pages
- **Hardware Coordination**: Enhanced mutual exclusion for Serial2 access
  - `stopAllTests()` method in DiagnosticsService for clean test termination
  - Automatic cleanup of diagnostic tests when switching to Live mode
  - SerialService properly resumes after diagnostics mode ends

### Changed

- Updated boot sequence to include UartModeService initialization ([x/10] now)
- Main.cpp now links UartModeService with Serial and Diagnostics services for coordination
- API documentation updated with UART Mode Management endpoints
- Frontend components refactored to support mode-aware rendering

### Fixed

- Diagnostics signal quality test: Reduced packet rate to prevent loss
- Diagnostics signal quality test: Fixed packet counts to realistic values
- Diagnostics: Prevented Serial2 hardware buffer overflow in signal test
- Diagnostics: Prevented memory exhaustion in signal quality test
- Diagnostics: Prevented WebSocket queue overflow with update throttling
- Diagnostics: Resolved min() type mismatch compilation error
- Serial: Disabled heartbeat logging to reduce log spam

## [0.2.0] - 2024-02-12

### Added

- **UART Diagnostics Project**: Complete hardware testing suite
  - Loopback Test: Verifies GPIO16-17 hardware functionality with echo packets
  - Baud Rate Detection: Automatically detects device baud rate (1200-115200)
  - Signal Quality Test: Measures latency, jitter, and packet loss
- Backend diagnostics service (`DiagnosticsService.h/cpp`, `DiagnosticsState.h`)
- REST endpoint: `/rest/diagnostics` for test control
- WebSocket endpoint: `/ws/diagnostics` for real-time test updates
- Frontend diagnostics UI with Material-UI components:
  - `DiagnosticsInfo.tsx`: Documentation and use cases
  - `LoopbackTest.tsx`: Real-time loopback test with success rate display
  - `BaudDetector.tsx`: Baud scanner with progress stepper
  - `SignalQuality.tsx`: Signal analysis with quality meter
- Comprehensive documentation: `DIAGNOSTICS-EXAMPLE.md` (534 lines)
- Software versioning system:
  - `src/version.h`: Backend version tracking
  - Version display in boot logs
  - Software versioning rule (`.cursor/rules/software-versioning.mdc`)
- Automatic version control rule (`.cursor/rules/version-control.mdc` updated)

### Changed

- Updated `main.cpp` boot sequence to [x/8] for diagnostics service
- Updated `API-REFERENCE.md` with diagnostics endpoints
- Updated `FILE-REFERENCE.md` with new diagnostics files
- Enhanced boot logging with version, build date/time, and API version
- Version control rule now enforces automatic commit/push after task completion

### Fixed

- None in this release

## [0.1.0] - 2024-01-10

### Added

- Initial serial monitoring service
- Serial2 (GPIO16/17) monitoring and streaming
- Configurable serial port settings (baud rate, data bits, stop bits, parity)
- Regex-based weight extraction from serial data
- REST API: `/rest/serial`
- WebSocket: `/ws/serial` for real-time data streaming
- MQTT publishing to `weighsoft/serial/{unique_id}/data`
- BLE notifications for serial data
- Serial configuration UI with live preview
- FSPersistence for serial configuration
- Comprehensive serial documentation: `SERIAL-EXAMPLE.md`
- LED example service for bidirectional control
- ESP8266-React framework integration
- Material-UI responsive frontend
- JWT authentication
- WiFi, MQTT, NTP, OTA configuration
- Comprehensive project documentation

### Changed

- N/A (initial release)

### Fixed

- Serial2 not initializing on boot
- Baud rate configuration not persisting after reboot
- Line ending parsing (now accepts both `\r` and `\n`)

---

## Version History

- **0.7.0** - Merged general docs/rules to master; Feature branch strategy
- **0.6.1** - Serial API fixes; UI polling and sanitization
- **0.6.0** - Weight Stream Forwarder + PowerShell Testing Tools
- **0.5.0** - ESP32-S3 Platform Support
- **0.4.0** - Comprehensive UART Documentation Update
- **0.3.0** - UART Mode Management System
- **0.2.0** - UART Diagnostics + Software Versioning
- **0.1.0** - Initial Release (Serial Monitoring + LED Example)

## Upcoming

See GitHub Issues for planned features and improvements.
