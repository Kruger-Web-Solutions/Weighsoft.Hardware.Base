# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **W-Soft: the screen goes to the Hub's Control room** (26 Nov 2026, Hub ADR 0010). A paired
  board pushes the words on its screen (the same JSON as `/rest/wsoftScreen`) with a signed
  `POST /api/v1/ingest/screen`: at most every 3 s while the screen changes, else every 30 s,
  only while the Hub is online and no firmware upload runs. An older Hub (404) is asked again
  after 10 minutes. `ui_model_json` got a mutex (two tasks call it now). Host test
  `test_wsoft_hub_screen` with the shared vector. Docs: `docs/WSOFT-SCREEN.md` section 5b.
  Not tried on a board yet.

### Changed
- **W-Soft: the Waveshare can read an RS232 scale or a load cell** (Rhyno, 23 Nov 2026). One thing
  is plugged in at a time, so the pins follow the source: RS485 on the RS485 plug; RS232 on the
  Sensor plug (GPIO6, receive only, through a MAX3232 like the yellow board); HX711 DT on the
  Sensor plug (GPIO6) and SCK on the I2C plug's SCL (GPIO9). With the load cell picked the touch
  screen is off (it shares the I2C wires); tap through the browser's Live tab. Relays stay off on
  the Waveshare (Rhyno: ignore them). Not bench-tested.
- **W-Soft: MENU > SCREEN says MINI EDGE** for the Full Colour look (the mini Edge, PR #70), so it
  is easy to find: DARK, LIGHT, MINI EDGE, DIAL. Every board.
- **W-Soft: one RS485 Scale setup, ZERO, stable and auto zero that work** (Rhyno, 23 Nov 2026).
  The Scale setup tab has one form for the RS485 card: unit, decimal places, e, stable range and
  time, auto zero range and time, Filter 1 and Filter 2. Save writes each value where it lives and
  reads it back; the old second "Weighing card" write table is gone, and a check table under the
  form shows what the card holds (Read / Read all). A DJB-M1X keeps all of them; a DJB-W1X keeps
  only the filters, so the board keeps the rest and does the stable judging and auto zero itself.
  ZERO on the main screen now works on RS485 too (board-side zero, kept over a restart, nothing
  written to the card). The trade engine and the saver use the stable range and time: a reading
  that wobbles inside the range is one weighing, not several. Every W-Soft board (Waveshare,
  yellow, S3). Not bench-tested.
- **W-Soft: decimal places and e work with a DJB-W1X** (Rhyno, 23 Nov 2026). The W1X (no display)
  has no decimal or e setting, so Scale setup saved to nowhere and came back empty. The board now
  sees that (the card reads e = 0) and keeps decimal places (0 to 4) and e itself. An M1X still
  keeps them on the card. Every RS485 board.
- **W-Soft: every board shows the same menus** (Rhyno, 23 Nov 2026). Serial is in the menu, and
  Settings shows "Weight comes from", the Hi-Lo relays and the Hi-Lo light on every W-Soft board. An
  option a board is not wired for (the Waveshare: RS232, load cell, relays, light) is greyed with
  "needs add-on wiring"; the board still refuses it, so no pin is driven. Web page only.
- **W-Soft: the S3 board can weigh with a load cell too** (Rhyno, 23 Nov 2026). HX711 DT on GPIO1,
  SCK on GPIO2, picked in Settings "Weight comes from". Same as the yellow board. Not bench-tested.
- **W-Soft: Live shows the screen AND the old info** (Rhyno, 23 Nov 2026). Under the board's
  screen the Live tab has the Approval and Auto save switches, the approval log, SAVE WEIGHING,
  the alarm test note and the status table again. Same page on every W-Soft board.
- **W-Soft: the browser's Live screen is sharp** (Rhyno, 22 Nov 2026). The page paints the
  board's screen with the Waveshare's letters at 800 x 480 instead of blowing up the 320 x 240
  picture, with round corners and a soft shadow. About 29 KB more firmware.
- **W-Soft: auto save counts down 3, 2, 1** (Rhyno, 22 Nov 2026). Auto save now waits for 3
  still seconds (it was 1) and the screen and the browser copy show SAVING IN 3, 2, 1. A tap on
  SAVE saves as before.
- **W-Soft: light mode in the Weighsoft brand colours** (Rhyno, 22 Nov 2026). Paper ground, white
  cards, Bunting ink and status bar, Sky buttons, brand green, amber and red. Dark mode is unchanged.
- **W-Soft: one Calibrate page for the junction box and the load cell** (Rhyno, 22 Nov 2026).
  Same look and the same two steps (Zero, Test weight) on RS485 and on an HX711 board; only the
  last step differs (sent to the junction box, or kept on the board). Both ask "Yes, calibrate"
  first and warn when the weight is still moving. The typed test weight is read one way: a
  weight finer than the scale shows (2.0005 on 3 decimals) is refused, where RS485 used to
  round it quietly. RS485 span with a real test weight is still **not bench-proven**.

### Added
- **W-Soft: Hi-Lo light on the yellow board** (Rhyno, 22 Nov 2026). W-Soft > Settings has a new
  switch "Hi-Lo light". LED1 shows the zone: blue under, green correct, red over, dark otherwise.
  OFF by default; with it and the relay switch both off the light stays dark. LED1 sits on the
  relay pins (GPIO17 / 16 / 4), so the relay service drives it (one writer, one output on at a
  time) and a relay board wired to those pads switches with the light. Setting `relay_light`,
  its own field: `esp_led` (the S3's light) is ON by default and is not used on these pins.
  **Not flashed, not bench-tested.**
- **W-Soft: two more looks for the board screen, Full Colour and Dial** (Rhyno, 22 Nov 2026).
  MENU > SCREEN now steps DARK, LIGHT, COLOUR, DIAL. Full Colour is the mini Edge look: a navy
  deck, a teal line on the status bar, the weight floating with a thick zone-colour outline,
  black words on a zone-colour pill, glass buttons with a cyan edge and a blue SAVE. Dial bends
  the Hi-Lo bar into a gauge over the weight. Both say what to do: TOO LOW ADD MORE (arrow up),
  TOO HIGH REMOVE PRODUCT (arrow down), SAVING, DONE. Same rows and taps; the browser's Live
  tab follows the board. Classic dark stays the default. Host-tested dot for dot on both panels;
  **not flashed, not seen on a board yet**. See docs/WSOFT-SCREEN.md section 3a.
- **W-Soft: Drift Guard for the load cell** (Rhyno, 22 Nov 2026: the zero floats in a slow
  wave of about 12 g, the same empty and loaded). Scale setup → *Drift Guard* (off by default,
  1 to 20 divisions). Once a weight has been still for 2 seconds, slow movement is held back so
  the weight shown stands still. It lets go at once when a count moves more than half a
  division, and when the drift held back would pass the limit, so a slow top-up is never
  hidden by more than the limit. It rests near zero when Auto zero is on, and while Approval
  is ON. Held in memory only. A slow change smaller than the limit stays hidden: that is the
  price. Built and host-tested, **not bench-proven**. Saved weighings do not record whether
  it was holding. On the USB cable `LOADCELL INFO` shows what it is doing and `LOADCELL SET`
  changes the load cell setup (not with Approval ON).
- **W-Soft: the board's screen in the browser** (Rhyno, 22 Nov 2026: one screen, two places).
  The Live tab shows the board's own screen and a click is a tap on the board: the board sends
  the words on its screen (`GET /rest/wsoftScreen`), the page paints them with the reference
  drawing the firmware is ported from, and a click goes back as a tap (`POST /rest/wsoftScreen`).
  A board with no panel gets its screen this way. ZERO is on the live page next to MENU on a
  load cell board. The old Live page is removed. See `docs/WSOFT-SCREEN.md`, section 5a.
- **W-Soft: ZERO on the screen and a filter for the load cell** (Rhyno, 22 Nov 2026). MENU →
  ZERO sets the load cell's zero from the touch screen (not with Approval ON, a tare on, or an
  unsteady reading). Scale setup gets *Filter* (Off, Low, Medium, High): a longer average that
  still follows a new load at once, drops single spikes, and holds the last digit so it does
  not flip between two divisions. The page shows the raw noise in divisions. The filter works
  on tenths of a second, so it is the same at 10 or 80 counts a second. *Auto zero* (off by
  default) pulls the empty scale back to 0 when it creeps. Calibrate shows the internal count
  live and the chip's measured speed.
- **W-Soft: scale setup for the load cell** (Rhyno, 22 Nov 2026). The Scale setup tab is the
  one home of the setup on every source. With a load cell it shows *Unit* and *Division size*
  in the place of the weighing card's fields, next to the same Max, Min, blanking, stable
  samples, tare and SA number as RS485. Calibrate keeps only Zero and Test weight. The test
  weight is kept in the scale's own digits, so the typed decimals no longer set what the scale
  shows.
- **W-Soft: load cell on an HX711 board as a third weight source** (yellow board, env
  `wsoft_cyd28`; Rhyno, 22 Nov 2026). Settings → *Weight comes from* → *Load cell (HX711
  board)*; the HX711 goes on CN1 (DT GPIO22, SCK GPIO27, 3.3 V). Calibrate tab: zero, then a
  known test weight, kept on the board. Built and host-tested, **not bench-tested**.
  See `docs/WSOFT-CYD28.md`, section 1c.

- **W-Soft board names** (Rhyno, 22 Nov 2026): every board that gets flashed is named
  `wsoft/<board make>/<number>` (e.g. `wsoft/waveshare/1`; hostname `wsoft-waveshare-1`), one
  number per board for life, kept in a register. `docs/WSOFT-WAVESHARE43.md` section 0c.
- **W-Soft: Wi-Fi update switch** (Rhyno, 14 Nov 2026). New software can go onto a board over
  Wi-Fi, so it does not have to be plugged into a laptop. W-Soft > Settings > Wi-Fi update
  (admin): OFF by default, the board's own password (4 to 64 characters since PR #58, never shown again, no
  factory or shared password), blocked while Approval is ON, and every change and every
  installed update adds 1 to the Approval audit counter and a line to the Approval log.
  Waveshare and S3 boards can do it (two app slots). The yellow board (`wsoft_cyd28`) has room
  for one copy only and stays on the cable; its page says so. The framework's own OTA service
  is no longer built into W-Soft (`FT_OTA=0`): its password was the public factory one and it
  ignored Approval. Guide and the rule for all boards: `docs/WSOFT-WAVESHARE43.md` section 0b.
  Not bench-tested yet; each board needs one last cable flash to get the switch.
- **W-Soft: Calibrate, backup Hub addresses and an alarm test switch on every W-Soft board**
  (`wsoft_waveshare43`, `wsoft_cyd28`, `wsoft_s3_headless`; Rhyno, 14 Nov 2026). Brought back
  from the 2 Nov Waveshare branch. Calibrate tab (admin): zero, span, check, through the junction
  box's AD register; a plain note on RS232 scales and Writers. Backup Hub addresses: the same
  Hub's other LAN addresses from the pairing key, tried when the Hub gives no answer. Alarm test
  switch (Settings): OFF by default, nothing with Approval ON, every weighing it lets through
  marked `alarm_test_override` for the Hub. Rules for every W-Soft board and the how-to:
  `docs/WSOFT-WAVESHARE43.md` sections 0 and 4c.
- **W-Soft Hi-Lo relays on the yellow board** (`wsoft_cyd28`, firmware 0.7.0): three relay
  outputs on the RGB LED pads follow the Hi-Lo check (LOW / OK / HIGH). Off by default; all
  OFF with no signal, an alarm, no product or an empty scale. Settings and a relay test on
  W-Soft > Settings. Guide: `docs/WSOFT-CYD28.md` section 2d. Not bench-tested yet.

### Fixed
- Build: shared sources compile for `esp12e` again (no BLE library, ESP8266 Wi-Fi and HTTP client, Serial1 `begin`, Wi-Fi scan). The sketch still does not link: it is about 2.6MB and the ESP8266 app slot is about 1MB. CI builds `node32s`.
- CI: install `intelhex` so the `node32s` (ESP32) build can finish.
- Build: pin the ESP32 platform to `espressif32@6.13.0` (same as the device branches). Unpinned, it moved to 8.0.0 (Arduino core 3.x) and `node32s` stopped compiling.
- Docs: branch lists now name branches that exist (there is no `serial2` or `display35`).
- Docs: `node32s` partition and OTA notes now match `platformio.ini` and `partitions_ble.csv`.

### Security
- `.gitignore`: `data/config/` (device runtime config) and `compile_commands.json` (generated build file).
- `docs/SECURITY.md`: never commit device config; what to do when a password reaches history.

### Note
- W-Soft through 22 Nov 2026 (PRs #58–#70) is on `master`. Other device lines are still on their own branches and are not fully logged here:
  - `RelayBoardEspBuildIn` - relay board and Live Weight screens (last commit 2026-08-10).
  - `SerialReaderWriter` - serial Reader / Writer / Diagnostics, TFT screen, LCD, remote weight (last commit 2026-07-16).
- Status of each line and the open decisions: [ROADMAP.md](ROADMAP.md). Branch list: [docs/BRANCH-MAP.md](docs/BRANCH-MAP.md).

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
