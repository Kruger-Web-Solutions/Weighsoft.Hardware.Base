# Extension Guide

## Overview

This guide shows you how to extend the Weighsoft Hardware framework by creating custom services. We provide two reference examples to use as templates:

- **LED Example** (`src/examples/led/`) - Demonstrates bidirectional control with 4 communication channels
- **Serial Example** (`src/examples/serial/`) - Demonstrates read-only data streaming from hardware

Both follow the single-layer architecture pattern for clean, maintainable code.

## Quick Start: Choose Your Template

### LED Example - For Controllable Devices

The **LED Example Project** (`src/examples/led/` and `interface/src/examples/led/`) demonstrates:

- **Bidirectional communication**: Control and status feedback
- **Four communication channels**: REST, WebSocket, MQTT, BLE
- **Inline protocol configuration**: No separate settings services
- **Multi-channel synchronization**: Automatic state broadcast with origin tracking
- **Complete UI**: React components for all control methods

**Use LED Example as template for**:

- Controllable devices (relays, motors, actuators)
- Bidirectional state (read + write)
- Device configuration interfaces

### Serial Example - For Data Sources

The **Serial Example Project** (`src/examples/serial/` and `interface/src/examples/serial/`) demonstrates:

- **Read-only streaming**: Data flows from hardware → channels
- **Line-based parsing**: Handles newline-delimited text data
- **Four channels**: REST (polling), WebSocket (streaming), MQTT (pub), BLE (notify)
- **Real-time UI**: Auto-scrolling log view with timestamps
- **Complete documentation**: Hardware wiring, use cases, troubleshooting

**Use Serial Example as template for**:

- Sensors and data sources (GPS, temperature, weight scales)
- Read-only data streams
- Log monitoring and data collection

**See**: `docs/LED-EXAMPLE.md` for detailed walkthrough and `docs/DESIGN-PATTERNS.md` Pattern 11 for architecture.

## Single-Layer Architecture Pattern

Weighsoft services follow a **single-layer pattern** where application services directly compose framework components:

```cpp
class MyDeviceService : public StatefulService<MyDeviceState> {
  HttpEndpoint<MyDeviceState> _httpEndpoint;      // REST API
  WebSocketTxRx<MyDeviceState> _webSocket;        // Real-time updates
  MqttPubSub<MyDeviceState> _mqttPubSub;          // MQTT pub/sub
  BlePubSub<MyDeviceState> _blePubSub;            // BLE (Phase 2)
  
  // Inline protocol configuration (topics, UUIDs, etc.)
  String _mqttBaseTopic;
  String _bleServiceUuid;
  
  void configureMqtt();
  void configureBle();
  void onStateChanged();
};
```

**Benefits**:

- One service class manages all protocols
- Configuration is inline (no separate settings UI needed)
- Easy to add/remove protocols per device type
- Origin tracking prevents feedback loops automatically

**Copy the LED or Serial Example**:

For controllable devices (LED pattern):

1. Copy `src/examples/led/` → `src/examples/mydevice/`
2. Rename classes: `LedExampleService` → `MyDeviceService`
3. Modify state struct to match your device
4. Update MQTT topics and BLE UUIDs
5. Implement device-specific hardware control

For data sources (Serial pattern):

1. Copy `src/examples/serial/` → `src/examples/mysensor/`
2. Rename classes: `SerialService` → `MySensorService`
3. Modify state struct for your data format
4. Update data source (Serial2, SPI, I2C, etc.)
5. Implement parsing logic for your protocol

## Common Mistakes Checklist

Before implementing your device, review these common mistakes from real implementations:

### Backend Mistakes

❌ **Wrong MqttPubSub constructor**

```cpp
// ❌ WRONG - Passing read function twice
_mqttPubSub(MyState::read, MyState::read, ...)

// ✅ CORRECT - Pass update function as second argument
_mqttPubSub(MyState::read, MyState::update, ...)
```

❌ **Forgetting BLE server callback**

```cpp
// ❌ WRONG - Configuring BLE in begin()
void begin() {
  configureBle();  // BLEServer not ready yet!
}

// ✅ CORRECT - Use onBleServerStarted callback in main.cpp
esp8266React.onBleServerStarted([](BLEServer* bleServer) {
  myService->setBleServer(bleServer);
  myService->configureBle();
});
```

❌ **Missing loop() call in main.cpp**

```cpp
// Don't forget to call loop() for services that need periodic execution
void loop() {
  esp8266React.loop();
  myService->loop();  // REQUIRED for Serial, sensors, etc.
}
```

### Frontend Mistakes

❌ **Wrong import paths**

```typescript
// ❌ WRONG
import { AXIOS } from './axios-fetch';  // Doesn't exist
import { useRest } from '../../components';  // Wrong location

// ✅ CORRECT
import { AXIOS } from './endpoints';
import { useRest } from '../../utils';
```

❌ **Hardcoded WebSocket paths**

```typescript
// ❌ WRONG - Duplicates /ws/ prefix
export const MY_SOCKET_PATH = '/ws/mydevice';

// ✅ CORRECT - Use WS_BASE_URL
export const MY_SOCKET_PATH = `${WS_BASE_URL}mydevice`;
```

❌ **Wrong useRest destructuring**

```typescript
// ❌ WRONG - 'loading' doesn't exist
const { data, loading } = useRest<T>({ read });

// ✅ CORRECT - Check data for loading state
const { data } = useRest<T>({ read });
if (!data) return <FormLoader />;
```

❌ **Wrong useWs signature**

```typescript
// ❌ WRONG - useWs doesn't take object parameter
const { data } = useWs<T>({ url: WEBSOCKET_URL });

// ✅ CORRECT - Pass URL string directly
const { data } = useWs<T>(WEBSOCKET_URL);
```

### Integration Mistakes

❌ **Inconsistent naming**

```typescript
// Menu says "Serial Monitor" but route is "serial"
<LayoutMenuItem path="/serial" label="Serial Monitor" />

// ✅ CORRECT - Keep naming consistent and concise
<LayoutMenuItem path="/serial" label="Serial" />
```

❌ **Not updating all documentation**

- Update API-REFERENCE.md with new endpoints
- Update FILE-REFERENCE.md with new files
- Update README.md if it's a major feature
- Create device-specific docs (LED-EXAMPLE.md, SERIAL-EXAMPLE.md)

## Complete Example: Temperature Sensor Service

### Requirements

We'll create a service that:

- Reads temperature from a DHT22 sensor
- Exposes current temperature via REST and WebSocket
- Publishes to MQTT (Home Assistant compatible)
- Allows threshold configuration
- Sends alerts when threshold exceeded

### Step 1: Define State Classes

Create `src/TemperatureSensor.h`:

```cpp
#ifndef TemperatureSensor_h
#define TemperatureSensor_h

#include <StatefulService.h>
#include <HttpEndpoint.h>
#include <WebSocketTxRx.h>
#include <MqttPubSub.h>
#include <FSPersistence.h>

// Settings (persisted configuration)
class TemperatureSettings {
public:
    float alertThreshold;
    bool alertEnabled;
    String mqttTopic;
    
    static void read(TemperatureSettings& settings, JsonObject& root) {
        root["alert_threshold"] = settings.alertThreshold;
        root["alert_enabled"] = settings.alertEnabled;
        root["mqtt_topic"] = settings.mqttTopic;
    }
    
    static StateUpdateResult update(JsonObject& root, TemperatureSettings& settings) {
        settings.alertThreshold = root["alert_threshold"] | 30.0;
        settings.alertEnabled = root["alert_enabled"] | false;
        settings.mqttTopic = root["mqtt_topic"] | SettingValue::format("homeassistant/sensor/esp_temp");
        
        // Validation
        if (settings.alertThreshold < -50 || settings.alertThreshold > 150) {
            return StateUpdateResult::ERROR;
        }
        
        return StateUpdateResult::CHANGED;
    }
};

// Status (runtime data, not persisted)
class TemperatureStatus {
public:
    float currentTemp;
    float currentHumidity;
    unsigned long lastRead;
    bool sensorError;
    
    static void read(TemperatureStatus& status, JsonObject& root) {
        root["current_temp"] = status.currentTemp;
        root["current_humidity"] = status.currentHumidity;
        root["last_read"] = status.lastRead;
        root["sensor_error"] = status.sensorError;
    }
    
    static StateUpdateResult update(JsonObject& root, TemperatureStatus& status) {
        status.currentTemp = root["current_temp"] | 0.0;
        status.currentHumidity = root["current_humidity"] | 0.0;
        status.lastRead = root["last_read"] | 0UL;
        status.sensorError = root["sensor_error"] | false;
        return StateUpdateResult::CHANGED;
    }
};

#endif
```

### Step 2: Create Settings Service

Create `src/TemperatureSettingsService.h`:

```cpp
#ifndef TemperatureSettingsService_h
#define TemperatureSettingsService_h

#include <TemperatureSensor.h>

class TemperatureSettingsService : public StatefulService<TemperatureSettings> {
public:
    TemperatureSettingsService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager) :
        _httpEndpoint(TemperatureSettings::read,
                     TemperatureSettings::update,
                     this,
                     server,
                     "/rest/temperatureSettings",
                     securityManager,
                     AuthenticationPredicates::IS_AUTHENTICATED),
        _fsPersistence(TemperatureSettings::read,
                      TemperatureSettings::update,
                      this,
                      fs,
                      "/config/temperatureSettings.json") {
    }
    
    void begin() {
        _fsPersistence.readFromFS();
    }
    
private:
    HttpEndpoint<TemperatureSettings> _httpEndpoint;
    FSPersistence<TemperatureSettings> _fsPersistence;
};

#endif
```

### Step 3: Create Status Service

Create `src/TemperatureStatusService.h`:

```cpp
#ifndef TemperatureStatusService_h
#define TemperatureStatusService_h

#include <TemperatureSensor.h>
#include <DHT.h>

#define DHT_PIN 4
#define DHT_TYPE DHT22

class TemperatureStatusService : public StatefulService<TemperatureStatus> {
public:
    TemperatureStatusService(AsyncWebServer* server,
                            SecurityManager* securityManager,
                            AsyncMqttClient* mqttClient,
                            TemperatureSettingsService* settingsService) :
        _httpEndpoint(TemperatureStatus::read,
                     TemperatureStatus::update,
                     this,
                     server,
                     "/rest/temperatureStatus",
                     securityManager,
                     AuthenticationPredicates::IS_AUTHENTICATED),
        _webSocket(TemperatureStatus::read,
                  TemperatureStatus::update,
                  this,
                  server,
                  "/ws/temperatureStatus",
                  securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED),
        _mqttPubSub(TemperatureStatus::read,
                   TemperatureStatus::update,
                   this,
                   mqttClient,
                   "",  // Pub topic (set dynamically)
                   ""), // No sub topic
        _settingsService(settingsService),
        _dht(DHT_PIN, DHT_TYPE),
        _lastReadTime(0) {
        
        // Register handler for settings changes
        _settingsService->addUpdateHandler([&](const String& originId) {
            reconfigureMqtt();
        });
    }
    
    void begin() {
        _dht.begin();
        reconfigureMqtt();
        readSensor(); // Initial read
    }
    
    void loop() {
        unsigned long now = millis();
        if (now - _lastReadTime >= 5000) {  // Read every 5 seconds
            readSensor();
            _lastReadTime = now;
        }
    }
    
private:
    HttpEndpoint<TemperatureStatus> _httpEndpoint;
    WebSocketTxRx<TemperatureStatus> _webSocket;
    MqttPubSub<TemperatureStatus> _mqttPubSub;
    TemperatureSettingsService* _settingsService;
    DHT _dht;
    unsigned long _lastReadTime;
    
    void readSensor() {
        float temp = _dht.readTemperature();
        float humidity = _dht.readHumidity();
        
        update([&](TemperatureStatus& status) {
            if (isnan(temp) || isnan(humidity)) {
                status.sensorError = true;
                return StateUpdateResult::ERROR;
            }
            
            status.currentTemp = temp;
            status.currentHumidity = humidity;
            status.lastRead = millis();
            status.sensorError = false;
            
            // Check alert threshold
            checkAlert(temp);
            
            return StateUpdateResult::CHANGED;
        }, "sensor");
    }
    
    void checkAlert(float temp) {
        _settingsService->read([&](TemperatureSettings& settings) {
            if (settings.alertEnabled && temp > settings.alertThreshold) {
                Serial.printf("ALERT: Temperature %.1f°C exceeds threshold %.1f°C\n",
                            temp, settings.alertThreshold);
                // Could send email, notification, etc.
            }
        });
    }
    
    void reconfigureMqtt() {
        _settingsService->read([&](TemperatureSettings& settings) {
            _mqttPubSub.setPubTopic(settings.mqttTopic + "/state");
        });
    }
};

#endif
```

### Step 4: Integrate into main.cpp

Edit `src/main.cpp`:

```cpp
#include <ESP8266React.h>
#include <TemperatureSensor.h>
#include <TemperatureSettingsService.h>
#include <TemperatureStatusService.h>

#define SERIAL_BAUD_RATE 115200

AsyncWebServer server(80);
ESP8266React esp8266React(&server);

// Temperature services
TemperatureSettingsService temperatureSettingsService(
    &server, 
    esp8266React.getFS(), 
    esp8266React.getSecurityManager()
);

TemperatureStatusService temperatureStatusService(
    &server,
    esp8266React.getSecurityManager(),
    esp8266React.getMqttClient(),
    &temperatureSettingsService
);

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    
    // Start framework
    esp8266React.begin();
    
    // Start temperature services
    temperatureSettingsService.begin();
    temperatureStatusService.begin();
    
    // Start server
    server.begin();
}

void loop() {
    esp8266React.loop();
    temperatureStatusService.loop();  // Read sensor periodically
}
```

### Step 5: Add PlatformIO Library

Edit `platformio.ini` to add DHT sensor library:

```ini
lib_deps =
    ArduinoJson@>=6.0.0,<7.0.0
    https://github.com/me-no-dev/ESPAsyncWebServer
    AsyncMqttClient@>=0.9.0,<1.0.0
    DHT sensor library@>=1.4.0
```

### Step 6: Create React Frontend

Create `interface/src/temperature/types.ts`:

```typescript
export interface TemperatureSettings {
    alert_threshold: number;
    alert_enabled: boolean;
    mqtt_topic: string;
}

export interface TemperatureStatus {
    current_temp: number;
    current_humidity: number;
    last_read: number;
    sensor_error: boolean;
}
```

Create `interface/src/temperature/api.ts`:

```typescript
import { AxiosPromise } from 'axios';
import { restEndpoint } from '../api/endpoints';
import { TemperatureSettings, TemperatureStatus } from './types';

export const TEMPERATURE_SETTINGS_ENDPOINT = 
    restEndpoint<TemperatureSettings>('/temperatureSettings');

export const TEMPERATURE_STATUS_ENDPOINT = 
    restEndpoint<TemperatureStatus>('/temperatureStatus');
```

Create `interface/src/temperature/TemperatureSettingsForm.tsx`:

```typescript
import { FC } from 'react';
import { TextField, Checkbox, FormControlLabel, Button } from '@mui/material';
import { SectionContent } from '../components';
import { useRest } from '../utils';
import { TemperatureSettings } from './types';
import { TEMPERATURE_SETTINGS_ENDPOINT } from './api';

const TemperatureSettingsForm: FC = () => {
    const { data, setData, saveData, saving } = 
        useRest<TemperatureSettings>(TEMPERATURE_SETTINGS_ENDPOINT);
    
    const updateValue = (name: string, value: any) => {
        setData({ ...data!, [name]: value });
    };
    
    return (
        <SectionContent title="Temperature Settings">
            <TextField
                label="Alert Threshold (°C)"
                type="number"
                value={data?.alert_threshold || 30}
                onChange={(e) => updateValue('alert_threshold', parseFloat(e.target.value))}
                fullWidth
                margin="normal"
            />
            <FormControlLabel
                control={
                    <Checkbox
                        checked={data?.alert_enabled || false}
                        onChange={(e) => updateValue('alert_enabled', e.target.checked)}
                    />
                }
                label="Enable Alerts"
            />
            <TextField
                label="MQTT Topic"
                value={data?.mqtt_topic || ''}
                onChange={(e) => updateValue('mqtt_topic', e.target.value)}
                fullWidth
                margin="normal"
            />
            <Button 
                variant="contained" 
                color="primary" 
                onClick={saveData}
                disabled={saving}
            >
                Save Settings
            </Button>
        </SectionContent>
    );
};

export default TemperatureSettingsForm;
```

Create `interface/src/temperature/TemperatureStatusDisplay.tsx`:

```typescript
import { FC } from 'react';
import { Typography, Box, Alert } from '@mui/material';
import { SectionContent } from '../components';
import { useWs } from '../utils';
import { TemperatureStatus } from './types';

const TemperatureStatusDisplay: FC = () => {
    const { data, connected } = useWs<TemperatureStatus>(
        "/ws/temperatureStatus",
        { current_temp: 0, current_humidity: 0, last_read: 0, sensor_error: false }
    );
    
    return (
        <SectionContent title="Current Temperature">
            {!connected && <Alert severity="warning">Not connected</Alert>}
            {data?.sensor_error && <Alert severity="error">Sensor Error!</Alert>}
            
            <Box sx={{ my: 2 }}>
                <Typography variant="h3">
                    {data?.current_temp.toFixed(1)}°C
                </Typography>
                <Typography variant="h5" color="textSecondary">
                    Humidity: {data?.current_humidity.toFixed(1)}%
                </Typography>
                <Typography variant="caption" color="textSecondary">
                    Last updated: {new Date(data?.last_read || 0).toLocaleTimeString()}
                </Typography>
            </Box>
        </SectionContent>
    );
};

export default TemperatureStatusDisplay;
```

### Step 7: Add to Router

Edit `interface/src/project/ProjectRouting.tsx`:

```typescript
import { Route, Routes, Navigate } from 'react-router-dom';
import TemperatureSettingsForm from '../temperature/TemperatureSettingsForm';
import TemperatureStatusDisplay from '../temperature/TemperatureStatusDisplay';

const ProjectRouting = () => (
    <Routes>
        <Route index element={<Navigate to="temperature/status" />} />
        <Route path="temperature/status" element={<TemperatureStatusDisplay />} />
        <Route path="temperature/settings" element={<TemperatureSettingsForm />} />
    </Routes>
);

export default ProjectRouting;
```

## Quick Reference Checklist

### Backend Service

- [ ] Define state class(es) with read/update methods
- [ ] Create service inheriting StatefulService<T>
- [ ] Compose infrastructure (HttpEndpoint, FSPersistence, WebSocket, MQTT)
- [ ] Implement begin() method
- [ ] Implement loop() if periodic tasks needed
- [ ] Register in main.cpp

### Frontend

- [ ] Define TypeScript types
- [ ] Create API endpoints
- [ ] Build form components (useRest for config)
- [ ] Build status components (useWs for real-time)
- [ ] Add to router
- [ ] Add to navigation menu

### Testing

- [ ] Test REST endpoints with Postman/curl
- [ ] Test WebSocket with browser dev tools
- [ ] Test MQTT with mosquitto_pub/sub
- [ ] Test filesystem persistence (restart device)
- [ ] Test state propagation across interfaces

## Common Extensions

### Add MQTT Command Support

```cpp
// Add MqttSub for commands
_mqttSub(TemperatureCommand::update,
        this,
        mqttClient,
        "homeassistant/sensor/esp_temp/set")
```

### Add Periodic Task

```cpp
void loop() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate >= 60000) {  // Every minute
        performTask();
        lastUpdate = millis();
    }
}
```

### Add Multi-Format Serialization

```cpp
// Standard format
static void read(State& state, JsonObject& root) {
    root["value"] = state.value;
}

// Home Assistant format  
static void haRead(State& state, JsonObject& root) {
    root["state"] = state.value;
    root["attributes"] = { /* ... */ };
}
```

## Next Steps

- [DESIGN-PATTERNS.md](DESIGN-PATTERNS.md) - Pattern reference
- [API-REFERENCE.md](API-REFERENCE.md) - API contracts
- [C4-CODE-PATTERNS.md](C4-CODE-PATTERNS.md) - Code diagrams
