# Design Patterns Catalog

## Overview

This document catalogs the key design patterns used in the ESP8266-React framework with implementation examples. Follow these patterns when extending the framework to ensure consistency and maintainability.

## Pattern 1: StatefulService Template Pattern

**Intent**: Provide a reusable base class for managing state with thread-safe access and event-driven updates.

**When to Use**:

- Managing any configuration or runtime state
- Need automatic synchronization across multiple interfaces
- Require thread-safe state access (ESP32)

**Structure**:

```cpp
template <class T>
class StatefulService {
    T _state;
    std::list<StateUpdateHandlerInfo_t> _updateHandlers;
    // Methods for update, read, handler management
};
```

**Implementation Example**:

```cpp
// 1. Define state class
class TemperatureSettings {
public:
    float threshold;
    bool alertEnabled;
    
    static void read(TemperatureSettings& settings, JsonObject& root) {
        root["threshold"] = settings.threshold;
        root["alert_enabled"] = settings.alertEnabled;
    }
    
    static StateUpdateResult update(JsonObject& root, TemperatureSettings& settings) {
        settings.threshold = root["threshold"] | 25.0;
        settings.alertEnabled = root["alert_enabled"] | false;
        return StateUpdateResult::CHANGED;
    }
};

// 2. Create service inheriting StatefulService
class TemperatureService : public StatefulService<TemperatureSettings> {
public:
    TemperatureService(AsyncWebServer* server, FS* fs, SecurityManager* securityManager);
    void begin();
    
private:
    HttpEndpoint<TemperatureSettings> _httpEndpoint;
    FSPersistence<TemperatureSettings> _fsPersistence;
};
```

**Benefits**:

- Consistent state management
- Automatic propagation
- Thread-safe on ESP32
- Reusable infrastructure

## Pattern 2: Service Composition Pattern

**Intent**: Build services by composing infrastructure components rather than inheritance.

**When to Use**:

- Creating new services
- Need REST API, persistence, WebSocket, or MQTT
- Want flexible feature combinations

**Structure**:

```cpp
class MyService : public StatefulService<MySettings> {
    HttpEndpoint<MySettings> _httpEndpoint;      // REST API
    FSPersistence<MySettings> _fsPersistence;    // File persistence
    WebSocketTxRx<MySettings> _webSocket;        // WebSocket
    MqttPubSub<MySettings> _mqttPubSub;          // MQTT
};
```

**Implementation Example**:

```cpp
TemperatureService::TemperatureService(
    AsyncWebServer* server, 
    FS* fs, 
    SecurityManager* securityManager
) :
    _httpEndpoint(
        TemperatureSettings::read,
        TemperatureSettings::update,
        this,
        server,
        "/rest/temperatureSettings",
        securityManager,
        AuthenticationPredicates::IS_ADMIN
    ),
    _fsPersistence(
        TemperatureSettings::read,
        TemperatureSettings::update,
        this,
        fs,
        "/config/temperatureSettings.json"
    )
{
    // Additional initialization
    addUpdateHandler([&](const String& originId) {
        // React to temperature settings changes
        configureAlerts();
    });
}
```

**Benefits**:

- Pick only needed features
- Clear separation of concerns
- Easy to test individual components
- Follows composition over inheritance

## Pattern 3: JSON Serialization Functions Pattern

**Intent**: Provide static functions for bidirectional JSON conversion.

**When to Use**:

- Every state class
- Need REST API or MQTT integration
- Filesystem persistence

**Structure**:

```cpp
class Settings {
public:
    // State fields
    
    static void read(Settings& settings, JsonObject& root);
    static StateUpdateResult update(JsonObject& root, Settings& settings);
};
```

**Implementation Example**:

```cpp
class TemperatureSettings {
public:
    float threshold;
    bool alertEnabled;
    String email;
    
    static void read(TemperatureSettings& settings, JsonObject& root) {
        root["threshold"] = settings.threshold;
        root["alert_enabled"] = settings.alertEnabled;
        root["email"] = settings.email;
    }
    
    static StateUpdateResult update(JsonObject& root, TemperatureSettings& settings) {
        // Parse with defaults
        settings.threshold = root["threshold"] | 25.0;
        settings.alertEnabled = root["alert_enabled"] | false;
        settings.email = root["email"] | "";
        
        // Validation
        if (settings.threshold < -50 || settings.threshold > 150) {
            return StateUpdateResult::ERROR;
        }
        
        return StateUpdateResult::CHANGED;
    }
};
```

**Benefits**:

- Centralized serialization logic
- Type-safe
- Reusable across infrastructure
- Easy validation

## Pattern 4: Update Handler Registration Pattern

**Intent**: React to state changes through registered callbacks.

**When to Use**:

- Hardware control based on config
- Cascading updates
- Logging or notifications

**Structure**:

```cpp
statefulService->addUpdateHandler([&](const String& originId) {
    // React to state change
}, allowRemove);
```

**Implementation Example**:

```cpp
class TemperatureService : public StatefulService<TemperatureSettings> {
public:
    TemperatureService() {
        // Register handler for temperature changes
        addUpdateHandler([&](const String& originId) {
            read([&](TemperatureSettings& settings) {
                if (currentTemp > settings.threshold && settings.alertEnabled) {
                    sendAlert();
                }
            });
        });
    }
    
private:
    void sendAlert() {
        // Send email or notification
    }
};
```

**Best Practices**:

- Use `allowRemove=false` for infrastructure handlers (FSPersistence, WebSocketTx)
- Use `allowRemove=true` (default) for application logic
- Check origin ID to prevent loops
- Keep handlers fast (avoid blocking operations)

## Pattern 5: Origin Tracking Pattern

**Intent**: Prevent circular updates when state changes propagate across multiple channels.

**When to Use**:

- Publishing to MQTT
- Broadcasting to WebSockets
- Any bidirectional communication

**Structure**:

```cpp
void onUpdate(const String& originId) {
    if (originId != "mqtt") {
        publishToMqtt();
    }
}
```

**Implementation Example**:

```cpp
class LightStateService : public StatefulService<LightState> {
    MqttPubSub<LightState> _mqttPubSub;
    
    LightStateService() {
        addUpdateHandler([&](const String& originId) {
            // Only update hardware, not MQTT
            // (MqttPubSub handles publishing automatically)
            read([&](LightState& state) {
                digitalWrite(LED_PIN, state.ledOn ? HIGH : LOW);
            });
        });
    }
};
```

**Origin ID Values**:

- `"http"` - REST API
- `"mqtt"` - MQTT message
- `"websocket:{id}"` - WebSocket client
- `"internal"` - Code initialization

**Benefits**:

- Prevents infinite loops
- Enables bidirectional sync
- Clear update source tracking

## Pattern 6: Factory Settings with Placeholders Pattern

**Intent**: Provide sensible defaults with dynamic value substitution.

**When to Use**:

- Need unique values per device
- Default configurations
- Factory reset behavior

**Structure**:

```cpp
settings.clientId = root["client_id"] | SettingValue::format("#{platform}-#{unique_id}");
```

**Available Placeholders**:

- `#{platform}` - "esp8266" or "esp32"
- `#{unique_id}` - MAC-derived unique ID
- `#{random}` - Random hex string

**Implementation Example**:

```cpp
static StateUpdateResult update(JsonObject& root, MqttSettings& settings) {
    settings.clientId = root["client_id"] | 
        SettingValue::format("#{platform}-#{unique_id}");
    settings.hostname = root["hostname"] | 
        SettingValue::format("esp-#{unique_id}");
    
    return StateUpdateResult::CHANGED;
}
```

**Benefits**:

- Unique defaults per device
- No hardcoded values
- Factory reset friendly

## Pattern 7: React useRest Hook Pattern

**Intent**: Manage server state in React components with loading states.

**When to Use**:

- Configuration forms
- Need to read and update server state
- Manual save button

**Structure**:

```typescript
const { data, setData, saveData, saving, loadData } = 
    useRest<DataType>(endpoint, initialData);
```

**Implementation Example**:

```typescript
const TemperatureSettings: FC = () => {
    const { data, setData, saveData, saving } = 
        useRest<TemperatureSettings>(TEMPERATURE_ENDPOINT);
    
    const updateValue = (name: string, value: any) => {
        setData({ ...data, [name]: value });
    };
    
    return (
        <form>
            <TextField
                value={data?.threshold || 25}
                onChange={(e) => updateValue('threshold', parseFloat(e.target.value))}
            />
            <Button onClick={saveData} disabled={saving}>
                Save
            </Button>
        </form>
    );
};
```

**Benefits**:

- Automatic loading on mount
- Loading state management
- Error handling with snackbar

## Pattern 8: React useWs Hook Pattern

**Intent**: Real-time bidirectional state sync via WebSocket.

**When to Use**:

- Real-time status display
- Live control (switches, sliders)
- Multiple clients need sync

**Structure**:

```typescript
const { data, updateData, connected } = 
    useWs<DataType>(endpoint, initialData);
```

**Implementation Example**:

```typescript
const LightControl: FC = () => {
    const { data, updateData, connected } = 
        useWs<LightState>("/ws/lightState", { led_on: false });
    
    const handleToggle = (event: React.ChangeEvent<HTMLInputElement>) => {
        updateData({ led_on: event.target.checked });
    };
    
    return (
        <Switch
            checked={data?.led_on || false}
            onChange={handleToggle}
            disabled={!connected}
        />
    );
};
```

**Benefits**:

- Real-time sync
- Optimistic updates
- Automatic reconnection
- Client ID tracking

## Pattern 9: Authentication Context Pattern

**Intent**: Manage global authentication state in React.

**When to Use**:

- Need user info across app
- JWT token management
- Protected routes

**Structure**:

```typescript
const { me, signIn, signOut } = useContext(AuthenticationContext);
```

**Implementation Example**:

```typescript
const SignInPage: FC = () => {
    const { signIn } = useContext(AuthenticationContext);
    const [username, setUsername] = useState('');
    const [password, setPassword] = useState('');
    
    const handleSubmit = async () => {
        try {
            await signIn(username, password, true);
            navigate('/dashboard');
        } catch (error) {
            // Show error
        }
    };
    
    return (
        <form onSubmit={handleSubmit}>
            {/* form fields */}
        </form>
    );
};
```

**Benefits**:

- Centralized auth state
- Automatic token injection
- Easy access control

## Pattern 10: Require Authentication Route Wrapper Pattern

**Intent**: Protect routes based on authentication status.

**When to Use**:

- Protected pages
- Admin-only sections
- Role-based access

**Structure**:

```typescript
<Route element={<RequireAdmin><Component /></RequireAdmin>} />
```

**Implementation Example**:

```typescript
<Routes>
    <Route 
        path="/security/*" 
        element={
            <RequireAdmin>
                <SecurityManagement />
            </RequireAdmin>
        } 
    />
    <Route 
        path="/dashboard" 
        element={
            <RequireAuthenticated>
                <Dashboard />
            </RequireAuthenticated>
        } 
    />
</Routes>
```

**Benefits**:

- Declarative access control
- Automatic redirects
- Clean route definitions

## Pattern 11: Single-Layer Protocol Integration

**Intent**: Simplify multi-protocol services by composing framework components directly without separate settings services.

**When to Use**:

- Building application-specific services (scale, relay, display, sensors)
- Need multiple communication channels (REST, WebSocket, MQTT, BLE)
- Want to minimize tech debt and simplify maintenance
- Industrial/commercial products with one primary function per device

**Problem**: The two-layer pattern (framework service + application-specific settings service) adds unnecessary complexity for most industrial IoT devices that have focused functionality.

**Solution**: Application services directly compose framework components and manage protocol-specific configuration inline.

**Structure**:

```cpp
class LedExampleService : public StatefulService<LedExampleState> {
  HttpEndpoint<LedExampleState> _httpEndpoint;
  WebSocketTxRx<LedExampleState> _webSocket;
  MqttPubSub<LedExampleState> _mqttPubSub;
  AsyncMqttClient* _mqttClient;
  
  // Inline protocol configuration
  String _mqttBasePath;
  String _mqttName;
  String _mqttUniqueId;
  
  void configureMqtt();
  void onConfigUpdated();
};
```

**Implementation Example**:

```cpp
LedExampleService::LedExampleService(
    AsyncWebServer* server,
    SecurityManager* securityManager,
    AsyncMqttClient* mqttClient
) :
    _httpEndpoint(LedExampleState::read, LedExampleState::update, this, server,
                  "/rest/ledExample", securityManager,
                  AuthenticationPredicates::IS_AUTHENTICATED),
    _mqttPubSub(LedExampleState::haRead, LedExampleState::haUpdate, this, mqttClient),
    _webSocket(LedExampleState::read, LedExampleState::update, this, server,
               "/ws/ledExample", securityManager,
               AuthenticationPredicates::IS_AUTHENTICATED),
    _mqttClient(mqttClient)
{
  // Inline MQTT configuration using SettingValue placeholders
  _mqttBasePath = SettingValue::format("homeassistant/light/#{unique_id}");
  _mqttName = SettingValue::format("led-example-#{unique_id}");
  _mqttUniqueId = SettingValue::format("led-#{unique_id}");
  
  pinMode(LED_PIN, OUTPUT);
  
  // Configure MQTT callback
  _mqttClient->onConnect(std::bind(&LedExampleService::configureMqtt, this));
  
  // Configure update handler for ALL channels
  // Origin tracking automatically prevents feedback loops
  addUpdateHandler([&](const String& originId) { onConfigUpdated(); }, false);
}

void LedExampleService::configureMqtt() {
  if (!_mqttClient->connected()) return;
  
  String configTopic = _mqttBasePath + "/config";
  String subTopic = _mqttBasePath + "/set";
  String pubTopic = _mqttBasePath + "/state";

  _mqttPubSub.configureTopics(pubTopic, subTopic);
  
  // Home Assistant auto-discovery
  DynamicJsonDocument doc(256);
  doc["~"] = _mqttBasePath;
  doc["name"] = _mqttName;
  doc["unique_id"] = _mqttUniqueId;
  doc["cmd_t"] = "~/set";
  doc["stat_t"] = "~/state";
  doc["schema"] = "json";
  
  String payload;
  serializeJson(doc, payload);
  _mqttClient->publish(configTopic.c_str(), 0, false, payload.c_str());
}

void LedExampleService::onConfigUpdated() {
  digitalWrite(LED_PIN, _state.ledOn ? LED_ON : LED_OFF);
}
```

**Multi-Channel Synchronization**:

The single update handler broadcasts changes across all channels while origin tracking prevents loops:

```cpp
// In main.cpp
ledExampleService = new LedExampleService(
    server,
    esp8266React->getSecurityManager(),
    esp8266React->getMqttClient()
);
```

When a user changes LED state via WebSocket:

1. `WebSocketTxRx` calls `StatefulService::update(json, WS_ORIGIN_ID)`
2. State changes, `callUpdateHandlers(WS_ORIGIN_ID)` is invoked
3. Update handler calls `onConfigUpdated()` (updates hardware)
4. `MqttPubSub` checks `originId != MQTT_ORIGIN_ID`, publishes to MQTT
5. `WebSocketTxRx` checks `originId == WS_ORIGIN_ID`, skips broadcast (originator)
6. `HttpEndpoint` not notified (REST is request/response only)

**BLE Integration (Phase 2)**:

When adding BLE, follow the same pattern:

```cpp
class LedExampleService : public StatefulService<LedExampleState> {
  HttpEndpoint<LedExampleState> _httpEndpoint;
  WebSocketTxRx<LedExampleState> _webSocket;
  MqttPubSub<LedExampleState> _mqttPubSub;
  BlePubSub<LedExampleState> _blePubSub;  // Add BLE component
  AsyncMqttClient* _mqttClient;
  BLEServer* _bleServer;
  
  // Inline BLE configuration
  String _bleServiceUuid;
  String _bleCharacteristicUuid;
};
```

**Benefits**:

- **Simplicity**: One service class, inline configuration
- **Maintainability**: No separate settings services to sync
- **Clarity**: All protocol config in one place
- **Flexibility**: Easy to add/remove protocols per device type
- **Scale**: Pattern works for serial devices, relays, displays, sensors

**When Not to Use**:

- User needs runtime configuration of protocol parameters (topics, UUIDs) - rare in industrial devices
- Multiple independent applications need to share the same protocol settings
- Protocol configuration is complex and benefits from dedicated UI

**Comparison**:

**Two-Layer (OLD - Complex)**:

```
MqttSettingsService → manages topics, settings UI, persistence
LedMqttSettingsService → demo-specific MQTT settings
LedStateService → depends on LedMqttSettingsService
```

**Single-Layer (NEW - Simple)**:

```
LedExampleService → inline topics, direct MQTT client composition
```

**Related Patterns**:

- Pattern 2: Service Composition (compose framework components)
- Pattern 5: Origin Tracking (prevent feedback loops)

**Examples**:

- `src/examples/led/LedExampleService.cpp` - Complete working example
- `docs/LED-EXAMPLE.md` - Detailed implementation guide

## Anti-Patterns to Avoid

### 1. Direct State Access

**Don't**:

```cpp
digitalWrite(LED_PIN, service._state.ledOn);  // Not thread-safe!
```

**Do**:

```cpp
service.read([&](LightState& state) {
    digitalWrite(LED_PIN, state.ledOn ? HIGH : LOW);
});
```

### 2. Ignoring StateUpdateResult

**Don't**:

```cpp
static StateUpdateResult update(JsonObject& root, Settings& settings) {
    settings.value = root["value"];
    return StateUpdateResult::CHANGED;  // Always CHANGED!
}
```

**Do**:

```cpp
static StateUpdateResult update(JsonObject& root, Settings& settings) {
    int newValue = root["value"] | 0;
    if (newValue < 0 || newValue > 100) {
        return StateUpdateResult::ERROR;  // Validation
    }
    if (settings.value == newValue) {
        return StateUpdateResult::UNCHANGED;  // No change
    }
    settings.value = newValue;
    return StateUpdateResult::CHANGED;
}
```

### 3. Creating Update Loops

**Don't**:

```cpp
addUpdateHandler([&](const String& originId) {
    // Always publish, creates loop!
    mqttClient->publish(topic, payload);
});
```

**Do**:

```cpp
addUpdateHandler([&](const String& originId) {
    if (originId != "mqtt") {
        mqttClient->publish(topic, payload);
    }
});
```

## Next Steps

- [EXTENSION-GUIDE.md](EXTENSION-GUIDE.md) - Apply these patterns
- [C4-CODE-PATTERNS.md](C4-CODE-PATTERNS.md) - See diagrams
- [API-REFERENCE.md](API-REFERENCE.md) - API contracts
