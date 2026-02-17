# Data Flows

## Overview

This document describes how data moves through the ESP8266-React framework, including state propagation patterns, communication protocols, and data transformation flows.

## State Propagation Architecture

### Core Principle: Event-Driven Updates

The framework uses an event-driven architecture where state changes trigger registered handlers. This enables automatic synchronization across multiple interfaces (REST, WebSocket, MQTT, filesystem).

```mermaid
graph LR
    StateChange[State Change] --> UpdateHandlers[Update Handlers]
    UpdateHandlers --> FS[Filesystem<br/>Save config]
    UpdateHandlers --> WS[WebSocket<br/>Broadcast]
    UpdateHandlers --> MQTT[MQTT<br/>Publish]
    UpdateHandlers --> Custom[Custom Logic<br/>Hardware control]
    
    style StateChange fill:#4fc3f7,stroke:#01579b,stroke-width:3px
    style UpdateHandlers fill:#fff59d,stroke:#f57f17,stroke-width:2px
```

### Origin Tracking for Circular Prevention

```mermaid
flowchart TB
    HTTPPost[HTTP POST<br/>origin: http] --> StatefulService
    WebSocket[WebSocket Message<br/>origin: websocket:123] --> StatefulService
    MQTT[MQTT Message<br/>origin: mqtt] --> StatefulService
    
    StatefulService[StatefulService<br/>Update State] --> CallHandlers[Call Update Handlers]
    
    CallHandlers --> CheckOrigin{Check Origin ID}
    
    CheckOrigin -->|origin != mqtt| MQTTPub[MQTT Publish]
    CheckOrigin -->|origin == mqtt| SkipMQTT[Skip MQTT]
    
    CheckOrigin -->|origin != websocket:123| WSBroadcast[WebSocket Broadcast]
    CheckOrigin -->|origin == websocket:123| SkipOriginWS[Skip Origin Client]
    
    CheckOrigin --> FSPersist[Filesystem Persist<br/>Always execute]
    
    MQTTPub --> Done[Complete]
    SkipMQTT --> Done
    WSBroadcast --> Done
    SkipOriginWS --> Done
    FSPersist --> Done
    
    style StatefulService fill:#f8bbd0,stroke:#880e4f,stroke-width:2px
    style CheckOrigin fill:#fff59d,stroke:#f57f17,stroke-width:2px
```

## REST API Data Flow

### Request Flow (GET)

```mermaid
sequenceDiagram
    participant Browser
    participant HTTP as HttpEndpoint
    participant Service as StatefulService
    participant State as _state object
    participant JSON as JsonStateReader
    
    Browser->>HTTP: GET /rest/wifiSettings
    HTTP->>Service: read(jsonObject, stateReader)
    Service->>Service: beginTransaction() [mutex lock]
    Service->>JSON: stateReader(_state, jsonObject)
    JSON->>State: Read fields
    State-->>JSON: ssid, password, hostname, ...
    JSON->>JSON: Populate jsonObject
    JSON-->>Service: JSON populated
    Service->>Service: endTransaction() [mutex unlock]
    Service-->>HTTP: jsonObject filled
    HTTP->>HTTP: Serialize to JSON string
    HTTP-->>Browser: 200 OK<br/>{"ssid":"MyWiFi","password":"***",...}
```

### Request Flow (POST)

```mermaid
sequenceDiagram
    participant Browser
    participant HTTP as HttpEndpoint
    participant Service as StatefulService
    participant JSON as JsonStateUpdater
    participant State as _state object
    participant Handlers as Update Handlers
    
    Browser->>HTTP: POST /rest/wifiSettings<br/>{"ssid":"NewWiFi","password":"new123"}
    HTTP->>HTTP: Parse JSON body
    HTTP->>Service: updateWithoutPropagation(json, stateUpdater)
    Service->>Service: beginTransaction() [mutex lock]
    Service->>JSON: stateUpdater(jsonObject, _state)
    JSON->>JSON: Parse JSON fields
    JSON->>State: Update fields<br/>ssid = "NewWiFi"<br/>password = "new123"
    State-->>JSON: Fields updated
    JSON-->>Service: StateUpdateResult::CHANGED
    Service->>Service: endTransaction() [mutex unlock]
    Service-->>HTTP: CHANGED
    
    HTTP->>HTTP: Schedule callUpdateHandlers<br/>on connection close
    HTTP->>Service: read(response, stateReader)
    HTTP-->>Browser: 200 OK<br/>{"ssid":"NewWiFi",...}
    
    Note over Browser,HTTP: Connection closed
    HTTP->>Service: callUpdateHandlers("http")
    Service->>Handlers: Notify all handlers
```

### Data Transformation (Serialization)

**State Object → JSON**:

```cpp
// Input: WiFiSettings object
{
    ssid: "MyWiFi",
    password: "secret123",
    hostname: "esp8266-device",
    staticIPConfig: false
}

// Output: JsonObject
{
    "ssid": "MyWiFi",
    "password": "secret123",
    "hostname": "esp8266-device",
    "static_ip_config": false
}
```

**JSON → State Object**:

```cpp
// Input: JsonObject
{
    "ssid": "NewWiFi",
    "password": "newsecret",
    "hostname": "my-device"
}

// Output: WiFiSettings object
{
    ssid: "NewWiFi",
    password: "newsecret",
    hostname: "my-device",
    staticIPConfig: false  // Not in JSON, use default
}
```

## WebSocket Data Flow

### Connection Establishment

```mermaid
sequenceDiagram
    participant Browser
    participant WS as WebSocketTxRx
    participant Service as StatefulService
    participant State as _state object
    
    Browser->>WS: WebSocket Upgrade Request
    WS->>Browser: Connection Accepted
    
    WS->>WS: Generate client ID: "websocket:12345"
    WS->>Browser: {"type":"id","id":"websocket:12345"}
    
    WS->>Service: read(_state)
    Service->>State: Read current state
    State-->>Service: State data
    Service-->>WS: State data
    
    WS->>WS: Serialize to JSON
    WS->>Browser: {"type":"payload","origin_id":"internal","payload":{...}}
```

### Bidirectional Update

```mermaid
flowchart TB
    subgraph Browser_to_Server [Browser → Server Flow]
        B1[User changes UI] --> B2[updateData called]
        B2 --> B3[Throttle 100ms]
        B3 --> B4[Serialize to JSON]
        B4 --> B5[Send via WebSocket]
    end
    
    subgraph Server_Processing [Server Processing]
        S1[Parse JSON] --> S2[StatefulService.update]
        S2 --> S3[State modified]
        S3 --> S4[callUpdateHandlers]
    end
    
    subgraph Server_to_Browsers [Server → Browsers Flow]
        T1[Update handler triggered] --> T2{Check origin}
        T2 -->|Not from this client| T3[Serialize state]
        T2 -->|From this client| T4[Skip this client]
        T3 --> T5[Broadcast to other clients]
    end
    
    B5 --> S1
    S4 --> T1
    
    style S3 fill:#f8bbd0,stroke:#880e4f,stroke-width:2px
```

### Message Format

**Type 1: Client ID**

```json
{
    "type": "id",
    "id": "websocket:12345"
}
```

**Type 2: Payload Update**

```json
{
    "type": "payload",
    "origin_id": "http",
    "payload": {
        "led_on": true
    }
}
```

### Client-Side State Sync

```typescript
// Receive from server
ws.onmessage = (event) => {
    const msg = JSON.parse(event.data);
    
    if (msg.type === "id") {
        clientId = msg.id;  // Store our ID
    }
    
    if (msg.type === "payload") {
        if (msg.origin_id !== clientId) {
            // Update from external source
            setData(msg.payload);
        }
        // Ignore our own updates
    }
};

// Send to server
const updateData = (newData) => {
    const json = JSON.stringify(newData);
    ws.send(json);
};
```

## MQTT Data Flow

### Publish Flow

```mermaid
sequenceDiagram
    participant Service as StatefulService
    participant Handler as Update Handler
    participant MQTT as MqttPub
    participant Reader as JsonStateReader
    participant Client as AsyncMqttClient
    participant Broker as MQTT Broker
    
    Service->>Service: State updated (origin: "http")
    Service->>Handler: handler callback("http")
    Handler->>MQTT: publish()
    
    MQTT->>MQTT: Check if connected
    MQTT->>MQTT: Check if pubTopic configured
    
    MQTT->>Service: read(jsonObject, stateReader)
    Service->>Reader: stateReader(_state, jsonObject)
    Reader-->>Service: JSON populated
    Service-->>MQTT: jsonObject
    
    MQTT->>MQTT: Serialize JsonObject to string
    MQTT->>Client: publish(pubTopic, payload, retain)
    Client->>Broker: PUBLISH message
```

### Subscribe Flow

```mermaid
sequenceDiagram
    participant Broker as MQTT Broker
    participant Client as AsyncMqttClient
    participant MQTT as MqttSub
    participant Updater as JsonStateUpdater
    participant Service as StatefulService
    
    Broker->>Client: PUBLISH to subTopic
    Client->>MQTT: onMessage(topic, payload, len)
    
    MQTT->>MQTT: Check if topic matches subTopic
    
    alt Topic Matches
        MQTT->>MQTT: Parse JSON payload
        MQTT->>MQTT: Create JsonObject from payload
        
        MQTT->>Service: update(jsonObject, stateUpdater, "mqtt")
        Service->>Updater: stateUpdater(jsonObject, _state)
        Updater->>Updater: Parse fields, update state
        Updater-->>Service: StateUpdateResult::CHANGED
        
        Service->>Service: callUpdateHandlers("mqtt")
        Note over Service: Handlers execute<br/>(but skip MQTT publish<br/>due to origin="mqtt")
    else Topic Doesn't Match
        MQTT->>MQTT: Ignore message
    end
```

### Home Assistant Data Format

**Standard Format (REST/WebSocket)**:

```json
{
    "led_on": true
}
```

**Home Assistant Format (MQTT)**:

```json
{
    "state": "ON"
}
```

**Conversion in Code**:

```cpp
// Standard read
static void read(LightState& state, JsonObject& root) {
    root["led_on"] = state.ledOn;
}

// Home Assistant read
static void haRead(LightState& state, JsonObject& root) {
    root["state"] = state.ledOn ? "ON" : "OFF";
}

// Home Assistant update
static StateUpdateResult haUpdate(JsonObject& root, LightState& state) {
    String stateStr = root["state"] | "OFF";
    state.ledOn = (stateStr == "ON");
    return StateUpdateResult::CHANGED;
}
```

## Filesystem Data Flow

### Read Flow (Initialization)

```mermaid
flowchart TB
    Start[Service.begin] --> ReadFS[FSPersistence.readFromFS]
    ReadFS --> OpenFile{File exists?}
    
    OpenFile -->|Yes| ParseJSON[Parse JSON file]
    OpenFile -->|No| Defaults[Use factory defaults]
    
    ParseJSON --> ParseOK{Valid JSON?}
    ParseOK -->|Yes| Deserialize[JsonStateUpdater<br/>deserializes to state]
    ParseOK -->|No| Defaults
    
    Deserialize --> UpdateState[updateWithoutPropagation<br/>loads state]
    Defaults --> UpdateState
    
    UpdateState --> Complete[Service initialized<br/>with configuration]
    
    Defaults --> WriteDefaults[writeToFS saves defaults]
    WriteDefaults --> Complete
    
    style ReadFS fill:#fff59d,stroke:#f57f17,stroke-width:2px
    style Complete fill:#c8e6c9,stroke:#2e7d32,stroke-width:2px
```

### Write Flow (Auto-Save)

```mermaid
flowchart TB
    StateChange[State updated<br/>via any source] --> CallHandlers[callUpdateHandlers]
    CallHandlers --> FSHandler[FSPersistence handler]
    
    FSHandler --> Serialize[JsonStateReader<br/>serializes state]
    Serialize --> CreateJSON[Create JsonDocument]
    CreateJSON --> MkDirs[Create directories<br/>if needed]
    MkDirs --> OpenFile[Open file for writing]
    OpenFile --> WriteJSON[Write JSON to file]
    WriteJSON --> CloseFile[Close file]
    CloseFile --> Saved[Configuration saved]
    
    style FSHandler fill:#fff59d,stroke:#f57f17,stroke-width:2px
    style Saved fill:#c8e6c9,stroke:#2e7d32,stroke-width:2px
```

### File Structure

```
/config/
├── wifiSettings.json        # WiFi configuration
├── apSettings.json           # Access Point configuration
├── mqttSettings.json         # MQTT broker configuration
├── ntpSettings.json          # NTP configuration
├── otaSettings.json          # OTA settings
├── securitySettings.json     # Users and JWT secret
└── brokerSettings.json       # Custom project settings (demo)
```

### JSON File Example

`/config/wifiSettings.json`:

```json
{
  "ssid": "MyHomeWiFi",
  "password": "mysecretpassword",
  "hostname": "esp8266-light",
  "static_ip_config": false,
  "local_ip": "0.0.0.0",
  "gateway_ip": "0.0.0.0",
  "subnet_mask": "0.0.0.0",
  "dns_ip_1": "0.0.0.0",
  "dns_ip_2": "0.0.0.0"
}
```

## Frontend Data Flow

### React State Management

```mermaid
flowchart TB
    subgraph Context_Layer [Global State - Context]
        Features[Features Context<br/>Feature flags]
        Auth[Authentication Context<br/>User, JWT token]
        Layout[Layout Context<br/>Page title]
    end
    
    subgraph Component_State [Component State - useState]
        FormData[Form Inputs]
        UIState[UI State<br/>Dialogs, menus]
    end
    
    subgraph Server_State [Server State - Hooks]
        RestHook[useRest Hook<br/>Configuration data]
        WsHook[useWs Hook<br/>Real-time data]
    end
    
    App[App Component] --> Context_Layer
    Context_Layer --> Components[Feature Components]
    Components --> Component_State
    Components --> Server_State
    
    Server_State --> Backend[Backend APIs]
    
    style Context_Layer fill:#e1f5fe,stroke:#01579b,stroke-width:2px
    style Server_State fill:#fff3e0,stroke:#e65100,stroke-width:2px
```

### useRest Hook Flow

```mermaid
sequenceDiagram
    participant Component
    participant Hook as useRest
    participant Axios
    participant Backend
    participant State as Local State
    
    Component->>Hook: useRest(endpoint, initialData)
    Hook->>Hook: Mount effect
    Hook->>Hook: loadData()
    Hook->>Axios: GET endpoint
    Axios->>Backend: HTTP Request
    Backend-->>Axios: JSON Response
    Axios-->>Hook: Data
    Hook->>State: setData(response)
    State-->>Component: Re-render with data
    
    Note over Component: User modifies data
    Component->>Hook: setData(newData)
    Hook->>State: Update local state
    State-->>Component: Re-render
    
    Note over Component: User clicks save
    Component->>Hook: saveData()
    Hook->>Axios: POST endpoint with data
    Axios->>Backend: HTTP Request
    Backend-->>Axios: Updated JSON
    Axios-->>Hook: Confirmation
    Hook->>State: setData(response)
    State-->>Component: Re-render with saved data
```

### useWs Hook Flow

```mermaid
sequenceDiagram
    participant Component
    participant Hook as useWs
    participant WS as WebSocket (sockette)
    participant Backend
    participant State as Local State
    
    Component->>Hook: useWs(endpoint, initialData)
    Hook->>WS: Connect to ws://...
    WS->>Backend: WebSocket Upgrade
    Backend-->>WS: Connection Accepted
    Backend->>WS: Client ID message
    WS->>Hook: Store clientId
    Backend->>WS: Initial payload
    WS->>Hook: onMessage
    Hook->>State: setData(payload)
    State-->>Component: Re-render
    
    Note over Component: User modifies data
    Component->>Hook: updateData(newData)
    Hook->>State: setData(newData) immediately
    State-->>Component: Re-render (optimistic)
    Hook->>Hook: Throttle 100ms
    Hook->>WS: send(JSON.stringify(newData))
    WS->>Backend: WebSocket message
    
    Note over Backend: State updated, broadcast to others
    Backend->>WS: Payload (origin != clientId)
    WS->>Hook: onMessage
    Hook->>Hook: Check origin_id != clientId
    Note over Hook: Message is echo, ignore
```

### Axios Request Interceptor

```typescript
// Add JWT token to all requests
axios.interceptors.request.use(
    (config) => {
        const token = localStorage.getItem('access_token');
        if (token) {
            config.headers.Authorization = `Bearer ${token}`;
        }
        return config;
    },
    (error) => Promise.reject(error)
);

// Handle 401 responses
axios.interceptors.response.use(
    (response) => response,
    (error) => {
        if (error.response?.status === 401) {
            // Clear token and redirect to sign-in
            localStorage.removeItem('access_token');
            window.location.href = '/';
        }
        return Promise.reject(error);
    }
);
```

## Data Validation and Error Handling

### Backend Validation

```cpp
// In JsonStateUpdater
static StateUpdateResult update(JsonObject& root, WiFiSettings& settings) {
    // Extract and validate
    settings.ssid = root["ssid"] | "";
    if (settings.ssid.length() == 0 || settings.ssid.length() > 32) {
        return StateUpdateResult::ERROR;  // Invalid SSID length
    }
    
    settings.password = root["password"] | "";
    if (settings.password.length() > 64) {
        return StateUpdateResult::ERROR;  // Invalid password length
    }
    
    // All validations passed
    return StateUpdateResult::CHANGED;
}
```

### Frontend Validation

```typescript
// async-validator schema
const schema = new Schema({
    ssid: [
        { required: true, message: 'SSID is required' },
        { max: 32, message: 'SSID must be 32 characters or less' }
    ],
    password: [
        { required: true, message: 'Password is required' },
        { min: 8, message: 'Password must be at least 8 characters' },
        { max: 64, message: 'Password must be 64 characters or less' }
    ]
});

// Validate before submit
schema.validate(formData, (errors) => {
    if (errors) {
        setErrors(errors);  // Display errors to user
    } else {
        saveData();  // Submit to backend
    }
});
```

## Performance Considerations

### Throttling and Debouncing

**WebSocket Updates** (100ms throttle):

```typescript
// Throttle rapid updates
const throttledUpdate = throttle((data) => {
    ws.send(JSON.stringify(data));
}, 100);
```

**Network Scanning** (debounce):

```typescript
// Debounce scan requests
const debouncedScan = debounce(() => {
    fetchNetworks();
}, 500);
```

### Buffer Sizing

**Backend JSON Buffers**:

```cpp
// Default buffer size: 1024 bytes
#define DEFAULT_BUFFER_SIZE 1024

// Service-specific sizes
HttpEndpoint<Settings> endpoint(
    reader, updater, service, server, path,
    securityManager, predicate,
    2048  // Larger buffer for complex settings
);
```

### Memory Management

**ArduinoJson Memory**:

```cpp
// Dynamic allocation for variable-sized data
DynamicJsonDocument doc(1024);

// Calculation for required size
size_t capacity = JSON_OBJECT_SIZE(10) + JSON_ARRAY_SIZE(5) + 512;
DynamicJsonDocument doc(capacity);
```

## Next Steps

- [API-REFERENCE.md](API-REFERENCE.md) - Complete API contracts
- [SEQUENCE-DIAGRAMS.md](SEQUENCE-DIAGRAMS.md) - Interaction flows
- [DESIGN-PATTERNS.md](DESIGN-PATTERNS.md) - Implementation patterns
