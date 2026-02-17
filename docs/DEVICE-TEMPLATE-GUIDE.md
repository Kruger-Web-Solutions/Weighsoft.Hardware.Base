# Quick Start: Add a New Device Project

## Overview

This guide provides a step-by-step checklist for adding a new device project to the Weighsoft Hardware framework. Follow this guide to add sensors, actuators, or other devices with minimal friction.

**Estimated Time**: 30-60 minutes for a complete device implementation (backend + frontend + documentation)

## Choose Your Template

Before starting, decide which pattern fits your device:

### LED Pattern (Bidirectional Control)

**Use for**: Controllable devices where users send commands and receive status

**Characteristics**:

- ✅ Read + write state
- ✅ User controls device via UI
- ✅ Bidirectional communication
- ✅ State persistence (optional)

**Examples**:

- Relays (on/off control)
- Motors (speed, direction)
- Displays (text, brightness)
- Actuators (position, state)
- LED strips (color, brightness)

**Template Location**:

- Backend: `src/examples/led/`
- Frontend: `interface/src/examples/led/`

### Serial Pattern (Data Source)

**Use for**: Read-only devices that stream data to the system

**Characteristics**:

- ✅ Read-only streaming
- ✅ Device pushes data to channels
- ✅ Real-time monitoring
- ✅ Data logging

**Examples**:

- Sensors (temperature, humidity, pressure)
- GPS modules (location, speed)
- Weight scales (readings, units)
- Serial monitors (log parsing)
- Data collectors (multi-sensor)

**Template Location**:

- Backend: `src/examples/serial/`
- Frontend: `interface/src/examples/serial/`

## 10-Step Implementation Checklist

### Backend Implementation (15-20 minutes)

#### Step 1: Copy Template Files

```bash
# For controllable devices (LED pattern)
cp -r src/examples/led src/examples/mydevice
# OR for data sources (Serial pattern)
cp -r src/examples/serial src/examples/mydevice
```

**Files created**:

- `MyDeviceState.h` - State structure
- `MyDeviceService.h` - Service header
- `MyDeviceService.cpp` - Service implementation

#### Step 2: Rename Classes

**Find and replace** in your new files:

- `LedExample` → `MyDevice` (or `Serial` → `MyDevice`)
- `LED_EXAMPLE` → `MY_DEVICE`
- `ledExample` → `myDevice`

**Files to update**:

- `MyDeviceState.h`
- `MyDeviceService.h`
- `MyDeviceService.cpp`

#### Step 3: Update State Structure

**Edit `MyDeviceState.h`**:

Define your device's state fields:

```cpp
class MyDeviceState {
public:
  // Your device-specific fields
  float temperature;
  bool heaterOn;
  unsigned long timestamp;
  
  static void read(MyDeviceState& state, JsonObject& root) {
    root["temperature"] = state.temperature;
    root["heater_on"] = state.heaterOn;  // Use snake_case in JSON
    root["timestamp"] = state.timestamp;
  }
  
  static StateUpdateResult update(JsonObject& root, MyDeviceState& state) {
    bool changed = false;
    
    // For controllable devices: Update from JSON
    bool newHeaterOn = root["heater_on"] | false;
    if (state.heaterOn != newHeaterOn) {
      state.heaterOn = newHeaterOn;
      changed = true;
    }
    
    // For read-only devices: Return UNCHANGED
    // (state is updated only from hardware)
    
    return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
  }
};
```

#### Step 4: Configure Endpoints and Topics

**Edit `MyDeviceService.h`**:

Update these constants:

```cpp
// REST endpoint
#define MY_DEVICE_ENDPOINT "/rest/myDevice"

// WebSocket endpoint
#define MY_DEVICE_WS_ENDPOINT "/ws/myDevice"

// MQTT topics
#define MY_DEVICE_MQTT_BASE "weighsoft/myDevice"

// BLE UUIDs (generate new ones at uuidgenerator.net)
#define MY_DEVICE_SERVICE_UUID "12340000-e8f2-537e-4f6c-d104768a1234"
#define MY_DEVICE_CHAR_UUID    "12340001-e8f2-537e-4f6c-d104768a1234"
```

**⚠️ Important**: Always generate NEW UUIDs for BLE. Never reuse UUIDs from examples!

#### Step 5: Implement Hardware Logic

**Edit `MyDeviceService.cpp`**:

Implement device-specific logic:

```cpp
void MyDeviceService::begin() {
  // Initialize hardware
  pinMode(HEATER_PIN, OUTPUT);
  // Load persisted state (if using FSPersistence)
  _fsPersistence.readFromFS();
}

void MyDeviceService::loop() {
  // Periodic tasks
  readTemperature();
}

void MyDeviceService::onConfigUpdated() {
  // React to state changes
  MyDeviceState state = read([](MyDeviceState& s) { return s; });
  digitalWrite(HEATER_PIN, state.heaterOn ? HIGH : LOW);
}
```

#### Step 6: Wire Up in main.cpp

**Edit `src/main.cpp`**:

Add your service:

```cpp
#include <examples/mydevice/MyDeviceService.h>

// Declare service pointer
MyDeviceService* myDeviceService;

void setup() {
  // ... existing setup code ...
  
  // Create your service
  Serial.println("[6/7] Initializing MyDevice service...");
  myDeviceService = new MyDeviceService(
    &server, 
    &securityManager, 
    &mqttClient
  );
  myDeviceService->begin();
  Serial.println("[6/7] MyDevice service created OK");
  
  // Register BLE callback (if using BLE)
  #if FT_ENABLED(FT_BLE)
  esp8266React.onBleServerStarted([](BLEServer* bleServer) {
    ledService->setBleServer(bleServer);
    ledService->configureBle();
    
    myDeviceService->setBleServer(bleServer);
    myDeviceService->configureBle();
  });
  #endif
}

void loop() {
  esp8266React.loop();
  myDeviceService->loop();  // Call if you implemented loop()
}
```

### Frontend Implementation (15-20 minutes)

#### Step 7: Copy UI Template

```bash
# For controllable devices (LED pattern)
cp -r interface/src/examples/led interface/src/examples/mydevice
# OR for data sources (Serial pattern)
cp -r interface/src/examples/serial interface/src/examples/mydevice
```

**Files created**:

- `MyDevice.tsx` - Main router with tabs
- `MyDeviceInfo.tsx` - Documentation tab
- `MyDeviceRest.tsx` - REST polling (optional)
- `MyDeviceWebSocket.tsx` - Real-time streaming
- `MyDeviceBle.tsx` - BLE instructions

#### Step 8: Create Types and API Layer

**Create `interface/src/types/mydevice.ts`**:

```typescript
export interface MyDeviceData {
  temperature: number;      // Matches temperature in backend
  heaterOn: boolean;        // Matches heater_on (camelCase auto-converts)
  timestamp: number;
}
```

**Create `interface/src/api/mydevice.ts`**:

```typescript
import { AxiosPromise } from 'axios';
import { AXIOS } from './endpoints';
import { MyDeviceData } from '../types/mydevice';

export const MY_DEVICE_ENDPOINT = '/rest/myDevice';

export function readMyDeviceData(): AxiosPromise<MyDeviceData> {
  return AXIOS.get(MY_DEVICE_ENDPOINT);
}

export function updateMyDeviceData(data: MyDeviceData): AxiosPromise<MyDeviceData> {
  return AXIOS.post(MY_DEVICE_ENDPOINT, data);
}
```

**Update `interface/src/api/endpoints.ts`**:

```typescript
// Add WebSocket path
export const MY_DEVICE_SOCKET_PATH = `${WS_BASE_URL}myDevice`;
```

#### Step 9: Customize Components

**Rename and update** all component files:

- `LedExample.tsx` → `MyDevice.tsx`
- `LedExampleInfo.tsx` → `MyDeviceInfo.tsx`
- etc.

**Update imports** in each file:

```typescript
// ✅ CORRECT imports
import { AXIOS } from '../../api/endpoints';
import { useRest, useWs } from '../../utils';
import { SectionContent, FormLoader } from '../../components';
import { WEB_SOCKET_ROOT } from '../../api/endpoints';
```

**Update labels** to match your device:

```typescript
const MyDevice: FC = () => {
  useLayoutTitle('My Device');  // Concise label
  // ... rest of component
};
```

#### Step 10: Add to Menu and Routing

**Edit `interface/src/project/ProjectMenu.tsx`**:

```typescript
import MyDeviceIcon from '@mui/icons-material/DeviceThermostat';

<LayoutMenuItem path="/my-device" label="My Device" icon={MyDeviceIcon} />
```

**Edit `interface/src/project/ProjectRouting.tsx`**:

```typescript
import MyDevice from '../examples/mydevice/MyDevice';

<Route path="my-device/*" element={<MyDevice />} />
```

## Common Mistakes Checklist

Before you finish, check these common mistakes from real implementations:

### Backend Mistakes

- [ ] ❌ **Wrong MqttPubSub constructor** - Passing `read` function twice instead of `read` then `update`
- [ ] ❌ **BLE configured in begin()** - Must use `onBleServerStarted` callback instead
- [ ] ❌ **Missing loop() call** - Forgot to call `myService->loop()` in main.cpp
- [ ] ❌ **Hardcoded GPIO pins** - Should use `#define` constants
- [ ] ❌ **No origin tracking** - For bidirectional services, pass `"mydevice_hw"` to prevent loops

### Frontend Mistakes

- [ ] ❌ **Wrong import path** - `import { AXIOS } from './axios-fetch'` (file doesn't exist)
- [ ] ❌ **Wrong hook location** - `import { useRest } from '../../components'` (should be `'../../utils'`)
- [ ] ❌ **Hardcoded WebSocket path** - `'/ws/mydevice'` instead of `${WS_BASE_URL}mydevice`
- [ ] ❌ **Wrong useRest usage** - Destructuring `loading` (doesn't exist)
- [ ] ❌ **Wrong useWs signature** - Passing object instead of string URL
- [ ] ❌ **Verbose labels** - "My Device Example" instead of "My Device"

### Integration Mistakes

- [ ] ❌ **Inconsistent naming** - Menu label doesn't match route
- [ ] ❌ **Missing documentation updates** - Didn't update API-REFERENCE.md, FILE-REFERENCE.md
- [ ] ❌ **Forgot to test all channels** - REST, WebSocket, MQTT, BLE
- [ ] ❌ **No device-specific docs** - Should create MY-DEVICE-EXAMPLE.md

## Documentation Requirements

### Always Update These Files

**`docs/API-REFERENCE.md`**:

```markdown
### My Device Endpoints

#### GET /rest/myDevice
Returns current device state.

**Response:**
```json
{
  "temperature": 23.5,
  "heater_on": true,
  "timestamp": 1234567890
}
```

#### WebSocket: /ws/myDevice

Real-time streaming of device state.

```

**`docs/FILE-REFERENCE.md`**:
```markdown
### Backend
- `src/examples/mydevice/MyDeviceState.h` - State structure
- `src/examples/mydevice/MyDeviceService.h/cpp` - Service implementation

### Frontend
- `interface/src/examples/mydevice/` - UI components
- `interface/src/api/mydevice.ts` - API layer
- `interface/src/types/mydevice.ts` - TypeScript types
```

**`README.md`**:

- Add your device to the project structure description

### Create Device-Specific Documentation

**`docs/MY-DEVICE-EXAMPLE.md`**:

Follow the template from `LED-EXAMPLE.md` or `SERIAL-EXAMPLE.md`:

```markdown
# My Device Example

## Overview
Brief description of what this device does.

## Features
- Feature 1
- Feature 2

## Hardware Setup
Wiring diagrams, pin configurations.

## Architecture
How the service works.

## API Reference
REST, WebSocket, MQTT, BLE endpoints.

## Use Cases
Real-world applications.

## Troubleshooting
Common issues and solutions.
```

## Testing Checklist

Before committing, verify:

### Backend Tests

- [ ] Code compiles for ESP8266
- [ ] Code compiles for ESP32
- [ ] No memory issues (check free heap)
- [ ] REST endpoint returns data: `GET /rest/myDevice`
- [ ] WebSocket streams updates: `/ws/myDevice`
- [ ] MQTT publishes to correct topics
- [ ] BLE service advertises with correct UUID

### Frontend Tests

- [ ] Frontend builds without errors: `npm run build`
- [ ] No TypeScript errors
- [ ] UI components load correctly
- [ ] Menu item appears and navigates
- [ ] REST polling works (if implemented)
- [ ] WebSocket updates in real-time
- [ ] BLE instructions are accurate

### Integration Tests

- [ ] Device responds via REST
- [ ] WebSocket shows live updates
- [ ] MQTT messages appear in broker
- [ ] BLE characteristic reads correctly
- [ ] All tabs in UI work
- [ ] Error handling works (disconnect, invalid data)

## Quick Reference

### Import Patterns

```typescript
// API imports
import { AXIOS } from './endpoints';
import { useRest, useWs } from '../../utils';
import { SectionContent, FormLoader } from '../../components';
import { WEB_SOCKET_ROOT } from '../../api/endpoints';

// Types
import { MyDeviceData } from '../types/mydevice';
```

### Hook Usage

```typescript
// useRest (configuration forms)
const { loadData, saveData, data, setData } = useRest<MyDeviceSettings>({
  read: () => readMyDeviceSettings()
});

// useWs (real-time monitoring)
const WEBSOCKET_URL = `${WEB_SOCKET_ROOT}mydevice`;
const { connected, updateData, data } = useWs<MyDeviceData>(WEBSOCKET_URL);
```

### File Naming Conventions

**Backend** (PascalCase for classes):

- `MyDeviceState.h`
- `MyDeviceService.h`
- `MyDeviceService.cpp`

**Frontend** (PascalCase for components, lowercase for API/types):

- `MyDevice.tsx`
- `MyDeviceInfo.tsx`
- `MyDeviceWebSocket.tsx`
- `api/mydevice.ts`
- `types/mydevice.ts`

## Time Estimates

With this guide:

- **Backend**: 15-20 minutes
- **Frontend**: 15-20 minutes
- **Documentation**: 10-15 minutes
- **Testing**: 5-10 minutes
- **Total**: 30-60 minutes

Without this guide (first time):

- **Total**: 1-2 hours (discovering patterns, fixing mistakes)

## Next Steps

1. Choose your template (LED or Serial pattern)
2. Follow the 10-step checklist
3. Check common mistakes
4. Update documentation
5. Test all channels
6. Commit with atomic, descriptive message

## Reference Implementations

- **LED Example**: `src/examples/led/`, `interface/src/examples/led/`
- **Serial Example**: `src/examples/serial/`, `interface/src/examples/serial/`
- **Extension Guide**: `docs/EXTENSION-GUIDE.md`
- **Frontend Patterns**: `docs/FRONTEND-PATTERNS.md`
- **Design Patterns**: `docs/DESIGN-PATTERNS.md`

## Getting Help

If stuck:

1. Check `docs/FRONTEND-PATTERNS.md` for import/hook issues
2. Check `docs/EXTENSION-GUIDE.md` for detailed examples
3. Check `.cursor/rules/adding-device-projects.mdc` for comprehensive guidelines
4. Reference LED or Serial examples for working code
