# File Structure Reference

## Overview

This document provides a complete reference to the file structure of the ESP8266-React framework with detailed descriptions of each directory and key files.

## Root Directory Structure

```
C:\Project\Weighsoft.Hardware.Base\
├── .clang-format              # C++ code formatting rules
├── .github\                   # GitHub Actions CI/CD
│   └── workflows\
│       └── verify_build.yml   # Build verification workflow
├── .gitignore                 # Git ignore patterns
├── docs\                      # Architecture documentation (THIS!)
├── factory_settings.ini       # Factory default values
├── features.ini               # Feature flag configuration
├── interface\                 # React frontend source
├── lib\                       # C++ libraries
│   ├── framework\             # Core framework code
│   └── readme.txt             # Library info
├── LICENSE.txt                # License information
├── media\                     # Documentation images
├── platformio.ini             # PlatformIO configuration
├── README.md                  # Main project README
├── scripts\                   # Build scripts
│   └── build_interface.py     # Interface build automation
└── src\                       # Main application code
    ├── main.cpp               # Entry point, service initialization
    └── examples\              # Example projects
        ├── led\               # LED Example: Bidirectional control
        │   ├── LedExampleService.h
        │   └── LedExampleService.cpp
        └── serial\            # Serial Example: Data streaming
            ├── SerialState.h
            ├── SerialService.h
            └── SerialService.cpp
```

## Backend Structure (C++)

### /lib/framework/ - Framework Core

**Purpose**: Reusable framework infrastructure

**Key Files**:

| File | Purpose |
|------|---------|
| `ESP8266React.h/cpp` | Framework coordinator, service registry |
| `StatefulService.h/cpp` | State management base class |
| `HttpEndpoint.h` | REST API template |
| `FSPersistence.h` | Filesystem persistence template |
| `WebSocketTxRx.h` | WebSocket bidirectional template |
| `MqttPubSub.h` | MQTT pub/sub template |
| `SecurityManager.h` | Authentication interface |
| `Features.h` | Feature flag macros |
| `SettingValue.h/cpp` | Placeholder substitution |
| `ESPFS.h` | Filesystem abstraction |
| `JsonUtils.h` | JSON helper functions |
| `IPUtils.h` | IP address utilities |

**Service Files**:

| Service | Files | Purpose |
|---------|-------|---------|
| WiFi | `WiFiSettingsService.*`, `WiFiStatus.*`, `WiFiScanner.*` | WiFi management |
| AP | `APSettingsService.*`, `APStatus.*` | Access Point |
| Security | `SecuritySettingsService.*`, `AuthenticationService.*` | Auth & users |
| MQTT | `MqttSettingsService.*`, `MqttStatus.*` | MQTT broker |
| NTP | `NTPSettingsService.*`, `NTPStatus.*` | Network time |
| OTA | `OTASettingsService.*`, `UploadFirmwareService.*` | Firmware updates |
| System | `SystemStatus.*`, `RestartService.*`, `FactoryResetService.*` | System management |
| Features | `FeaturesService.*` | Feature flag exposure |

### /src/ - Application Code

**Purpose**: Main application and example projects

**Files**:

| File | Purpose |
|------|---------|
| `main.cpp` | Entry point, setup(), loop(), service initialization |

**Example Projects**:

| Directory | Files | Purpose |
|-----------|-------|---------|
| `examples/led/` | `LedExampleService.h/cpp` | LED control with 4 channels (REST, WS, MQTT, BLE) |
| `examples/serial/` | `SerialState.h`, `SerialService.h/cpp` | Serial2 monitoring and streaming |
| `examples/diagnostics/` | `DiagnosticsState.h`, `DiagnosticsService.h/cpp` | UART hardware testing (loopback, baud scan, signal quality) |

**Pattern**: Create your custom services in `examples/yourdevice/`

### Configuration Files

| File | Purpose |
|------|---------|
| `platformio.ini` | PlatformIO build configuration |
| `features.ini` | Feature flag definitions (FT_*) |
| `factory_settings.ini` | Factory default values |
| `.clang-format` | C++ formatting rules |

## Frontend Structure (React)

### /interface/ - React Application

**Root Files**:

| File | Purpose |
|------|---------|
| `package.json` | NPM dependencies, scripts, proxy config |
| `tsconfig.json` | TypeScript configuration |
| `.env` | Environment variables (app name) |
| `config-overrides.js` | Build customization (react-app-rewired) |
| `public/` | Static assets (icon, manifest) |

### /interface/src/ - Source Code

**Core Files**:

| File | Purpose |
|------|---------|
| `index.tsx` | React entry point |
| `App.tsx` | Root component with providers |
| `AppRouting.tsx` | Top-level routing |
| `AuthenticatedRouting.tsx` | Protected routes |
| `CustomTheme.tsx` | Material-UI theme |
| `SignIn.tsx` | Sign-in page |
| `setupProxy.js` | Dev proxy configuration |

### /interface/src/api/ - API Clients

| File | Purpose |
|------|---------|
| `endpoints.ts` | Axios configuration, helpers |
| `wifi.ts` | WiFi API endpoints |
| `mqtt.ts` | MQTT API endpoints |
| `security.ts` | Security API endpoints |
| `system.ts` | System API endpoints |
| `ntp.ts` | NTP API endpoints |
| `ap.ts` | Access Point API endpoints |
| `features.ts` | Features API endpoint |
| `authentication.ts` | Sign-in API |
| `env.ts` | Environment variable access |

### /interface/src/components/ - Shared Components

| Directory | Purpose |
|-----------|---------|
| `layout/` | App layout (AppBar, Drawer, Menu) |
| `routing/` | Routing components (RequireAuth, RouterTabs) |
| `inputs/` | Form inputs (ValidatedTextField, ValidatedPasswordField) |
| `loading/` | Loading states (Spinner, FormLoader) |
| `ButtonRow.tsx` | Button container |
| `SectionContent.tsx` | Page section wrapper |
| `MessageBox.tsx` | Info/warning/error box |

### /interface/src/contexts/ - React Contexts

| Directory | Files | Purpose |
|-----------|-------|---------|
| `authentication/` | `Authentication.tsx`, `context.ts` | Auth state, JWT tokens |
| `features/` | `FeaturesLoader.tsx`, `context.ts` | Feature flags |

### /interface/src/framework/ - Framework UI

| Directory | Purpose |
|-----------|---------|
| `wifi/` | WiFi management UI |
| `ap/` | Access Point UI |
| `mqtt/` | MQTT configuration UI |
| `ntp/` | NTP configuration UI |
| `security/` | User management UI |
| `system/` | System status, OTA, upload UI |

### /interface/src/project/ - Custom Features

| File | Purpose |
|------|---------|
| `ProjectRouting.tsx` | Custom feature routing |
| `ProjectMenu.tsx` | Custom menu items |

### /interface/src/examples/ - Example Projects

**LED Example** (`examples/led/`):

| File | Purpose |
|------|---------|
| `LedExample.tsx` | Main router with tabs |
| `LedExampleInfo.tsx` | Documentation and wiring |
| `LedControlRest.tsx` | REST control form |
| `LedControlWebSocket.tsx` | WebSocket real-time control |
| `LedControlBle.tsx` | BLE connection instructions |
| `api.ts` | LED API endpoints |
| `types.ts` | TypeScript types |

**Serial Example** (`examples/serial/`):

| File | Purpose |
|------|---------|
| `SerialMonitor.tsx` | Main router with tabs |
| `SerialInfo.tsx` | Documentation and hardware setup |
| `SerialConfig.tsx` | Serial port and regex configuration form |
| `SerialRest.tsx` | REST polling view |
| `SerialWebSocket.tsx` | Real-time streaming view |
| `SerialBle.tsx` | BLE connection instructions |

**Diagnostics Example** (`examples/diagnostics/`):

| File | Purpose |
|------|---------|
| `Diagnostics.tsx` | Main router with tabs |
| `DiagnosticsInfo.tsx` | Documentation and use cases |
| `LoopbackTest.tsx` | Loopback test UI with live results |
| `BaudDetector.tsx` | Baud rate scanner UI with progress |
| `SignalQuality.tsx` | Signal quality test UI with metrics |

### /interface/src/api/ - API Layer

| File | Purpose |
|------|---------|
| `endpoints.ts` | Base URLs and WebSocket paths |
| `serial.ts` | Serial example API functions |
| `diagnostics.ts` | Diagnostics API functions |

### /interface/src/types/ - TypeScript Types

| File | Purpose |
|------|---------|
| `serial.ts` | Serial data types |
| `diagnostics.ts` | Diagnostics test state types |
| `wifi.ts` | WiFi types |
| `mqtt.ts` | MQTT types |
| `security.ts` | Security types |
| `system.ts` | System types |
| `features.ts` | Feature flag types |
| `me.ts` | User types |

### /interface/src/utils/ - Utility Hooks

| File | Purpose |
|------|---------|
| `useRest.ts` | REST API state management |
| `useWs.ts` | WebSocket real-time sync |
| `binding.ts` | Form binding helpers |
| `time.ts` | Time formatting |
| `route.ts` | Routing utilities |

### /interface/src/validators/ - Form Validation

| File | Purpose |
|------|---------|
| `wifi.ts` | WiFi form validation |
| `mqtt.ts` | MQTT form validation |
| `security.ts` | User form validation |
| `shared.ts` | Common validators (IP, port) |

## Build Artifacts

### Generated Directories

**Not in Git**:

```
/.pio/                  # PlatformIO build artifacts
interface/node_modules/ # NPM dependencies
interface/build/        # React production build
.cache/                 # Build cache
```

### Uploaded to Device

**Firmware**:

- `.pio/build/{env}/firmware.bin` - Main firmware
- `.pio/build/{env}/spiffs.bin` - Filesystem image (if not PROGMEM_WWW)

**Filesystem** (uploaded separately if not PROGMEM_WWW):

```
/www/
├── index.html
├── js/main.*.js
├── css/main.*.css
├── fonts/*.woff2
└── app/
    ├── icon.png
    └── manifest.json
```

**Configuration** (created at runtime):

```
/config/
├── wifiSettings.json
├── apSettings.json
├── securitySettings.json
├── mqttSettings.json
├── ntpSettings.json
├── otaSettings.json
├── brokerSettings.json
└── serialConfig.json
```

## Key File Responsibilities

### main.cpp

**Responsibilities**:

- Create AsyncWebServer instance
- Create ESP8266React framework instance
- Create custom service instances
- Initialize all services in setup()
- Call framework loop() in loop()
- Register custom endpoints

### ESP8266React.cpp

**Responsibilities**:

- Initialize filesystem
- Create all framework services
- Register WWW routes (PROGMEM or filesystem)
- Coordinate service begin() methods
- Coordinate service loop() methods

### StatefulService Template

**Responsibilities**:

- Store state object
- Provide thread-safe access
- Manage update handlers
- Propagate state changes

### Infrastructure Templates

**Responsibilities**:

- HttpEndpoint: Expose REST API
- FSPersistence: Save/load from filesystem
- WebSocketTxRx: Real-time bidirectional sync
- MqttPubSub: MQTT pub/sub integration

## File Naming Conventions

### Backend (C++)

- `.h` - Header files
- `.cpp` - Implementation files
- PascalCase: `WiFiSettingsService.h`

### Frontend (TypeScript/React)

- `.tsx` - React components
- `.ts` - TypeScript modules
- PascalCase for components: `WiFiSettingsForm.tsx`
- camelCase for utilities: `useRest.ts`

## Next Steps

- [ARCHITECTURE.md](ARCHITECTURE.md) - System overview
- [EXTENSION-GUIDE.md](EXTENSION-GUIDE.md) - Add custom files
- [CONFIGURATION.md](CONFIGURATION.md) - Build configuration
- [OTA-UPLOAD.md](OTA-UPLOAD.md) - OTA firmware upload (ESP32-S3, port 8266)
