# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Note
- This file covers `master` only. Device work since February 2026 is **not** on `master` and not logged here. It lives on its own branches:
  - `RelayBoardEspBuildIn` - relay board and Live Weight screens (last commit 2026-08-10).
  - `SerialReaderWriter` - serial Reader / Writer / Diagnostics, TFT screen, LCD, remote weight (last commit 2026-07-16).
- Status of each line and the open decisions: [ROADMAP.md](ROADMAP.md).

### Fixed
- CI: install `intelhex` so the `node32s` (ESP32) build can finish.
- Docs: branch lists now name branches that exist (there is no `serial2` or `display35`).
- Docs: `node32s` partition and OTA notes now match `platformio.ini` and `partitions_ble.csv`.

### Security
- `.gitignore`: `data/config/` (live device passwords) and `compile_commands.json` (copies build-flag passwords). `master` only: the device branches do not have this rule yet, and it does not stop files git already tracks. See [ROADMAP.md](ROADMAP.md).
- `docs/SECURITY.md`: never commit device config; what to do when a password reaches history.

## [0.7.0] - 2026-02-17

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
