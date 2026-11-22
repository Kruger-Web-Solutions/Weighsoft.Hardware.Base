# API Reference

## Overview

This document provides complete API contracts for all REST, WebSocket, and MQTT endpoints in the ESP8266-React framework. Use this as the authoritative reference when integrating with or extending the system.

## REST API Endpoints

**Base URL**: `/rest/`

**Authentication**: Bearer token in `Authorization` header (where required)

**Content-Type**: `application/json`

### Authentication Endpoints

#### POST /rest/signIn

Sign in and receive JWT token.

**Security**: None (public endpoint)

**Request**:

```json
{
  "username": "admin",
  "password": "admin"
}
```

**Response** (200 OK):

```json
{
  "access_token": "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9...",
  "username": "admin",
  "admin": true
}
```

**Errors**:

- 401: Invalid credentials

#### GET /rest/verifyAuthorization

Verify JWT token validity.

**Security**: IS_AUTHENTICATED

**Response** (200 OK):

```json
{
  "username": "admin",
  "admin": true
}
```

**Errors**:

- 401: Invalid or expired token

### WiFi Management

#### GET /rest/wifiSettings

Get WiFi configuration.

**Security**: IS_ADMIN

**Response** (200 OK):

```json
{
  "ssid": "MyWiFi",
  "password": "secret",
  "hostname": "esp8266-device",
  "static_ip_config": false,
  "local_ip": "0.0.0.0",
  "gateway_ip": "0.0.0.0",
  "subnet_mask": "0.0.0.0",
  "dns_ip_1": "0.0.0.0",
  "dns_ip_2": "0.0.0.0"
}
```

#### POST /rest/wifiSettings

Update WiFi configuration.

**Security**: IS_ADMIN

**Request**:

```json
{
  "ssid": "NewWiFi",
  "password": "newsecret",
  "hostname": "my-device",
  "static_ip_config": true,
  "local_ip": "192.168.1.100",
  "gateway_ip": "192.168.1.1",
  "subnet_mask": "255.255.255.0",
  "dns_ip_1": "8.8.8.8",
  "dns_ip_2": "8.8.4.4"
}
```

**Response** (200 OK): Same as GET response

#### GET /rest/wifiStatus

Get current WiFi connection status.

**Security**: IS_AUTHENTICATED

**Response** (200 OK):

```json
{
  "status": "connected",
  "local_ip": "192.168.1.100",
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "rssi": -45,
  "ssid": "MyWiFi",
  "bssid": "00:11:22:33:44:55",
  "channel": 6,
  "subnet_mask": "255.255.255.0",
  "gateway_ip": "192.168.1.1",
  "dns_ip_1": "8.8.8.8"
}
```

#### GET /rest/scanNetworks

Scan for available WiFi networks.

**Security**: IS_ADMIN

**Response** (200 OK):

```json
{
  "networks": [
    {
      "rssi": -45,
      "ssid": "MyWiFi",
      "bssid": "00:11:22:33:44:55",
      "channel": 6,
      "encryption_type": 3
    }
  ]
}
```

### Access Point

#### GET /rest/apSettings

Get Access Point configuration.

**Security**: IS_ADMIN

**Response** (200 OK):

```json
{
  "provision_mode": 1,
  "ssid": "ESP8266-React-abc123",
  "password": "esp-react",
  "channel": 1,
  "ssid_hidden": false,
  "max_clients": 4,
  "local_ip": "192.168.4.1",
  "gateway_ip": "192.168.4.1",
  "subnet_mask": "255.255.255.0"
}
```

#### POST /rest/apSettings

Update Access Point configuration.

**Security**: IS_ADMIN

**Request**: Same structure as GET response

#### GET /rest/apStatus

Get Access Point status.

**Security**: IS_AUTHENTICATED

**Response** (200 OK):

```json
{
  "active": true,
  "ip_address": "192.168.4.1",
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "station_num": 2
}
```

### MQTT

#### GET /rest/mqttSettings

Get MQTT broker configuration.

**Security**: IS_ADMIN

**Response** (200 OK):

```json
{
  "enabled": true,
  "host": "192.168.1.50",
  "port": 1883,
  "username": "",
  "password": "",
  "client_id": "esp8266-abc123",
  "keep_alive": 60,
  "clean_session": true,
  "max_topic_length": 128
}
```

#### POST /rest/mqttSettings

Update MQTT broker configuration.

**Security**: IS_ADMIN

**Request**: Same structure as GET response

#### GET /rest/mqttStatus

Get MQTT connection status.

**Security**: IS_AUTHENTICATED

**Response** (200 OK):

```json
{
  "enabled": true,
  "connected": true,
  "client_id": "esp8266-abc123"
}
```

### NTP (Network Time Protocol)

#### GET /rest/ntpSettings

Get NTP configuration.

**Security**: IS_ADMIN

**Response** (200 OK):

```json
{
  "enabled": true,
  "server": "time.google.com",
  "tz_label": "Europe/London",
  "tz_format": "GMT0BST,M3.5.0/1,M10.5.0"
}
```

#### POST /rest/ntpSettings

Update NTP configuration.

**Security**: IS_ADMIN

**Request**: Same structure as GET response

#### GET /rest/ntpStatus

Get NTP synchronization status.

**Security**: IS_AUTHENTICATED

**Response** (200 OK):

```json
{
  "enabled": true,
  "synced": true,
  "utc_time": "2024-01-15T14:30:00Z",
  "local_time": "2024-01-15T14:30:00",
  "uptime": 3600
}
```

### BLE (Bluetooth Low Energy)

#### GET /rest/bleSettings

Get BLE configuration.

**Security**: IS_AUTHENTICATED

**Platform**: ESP32 only (returns 404 on ESP8266)

**Response** (200 OK):

```json
{
  "enabled": true,
  "device_name": "Weighsoft-a4e57cdb7928"
}
```

#### POST /rest/bleSettings

Update BLE configuration.

**Security**: IS_AUTHENTICATED

**Request**: Same structure as GET response

**Notes**:

- Changing `device_name` requires BLE restart
- Device name appears in BLE scans
- Changes take effect immediately

#### GET /rest/bleStatus

Get BLE connection status.

**Security**: IS_AUTHENTICATED

**Platform**: ESP32 only

**Response** (200 OK):

```json
{
  "enabled": true,
  "connected_devices": 1,
  "device_name": "Weighsoft-a4e57cdb7928",
  "mac_address": "A4:E5:7C:DB:79:28"
}
```

**Field Descriptions**:

- `enabled`: Whether BLE server is running
- `connected_devices`: Number of active BLE connections
- `device_name`: Current advertised device name
- `mac_address`: Bluetooth MAC address

### OTA (Over-The-Air Updates)

#### GET /rest/otaSettings

Get OTA configuration.

**Security**: IS_ADMIN

**Response** (200 OK):

```json
{
  "enabled": true,
  "port": 8266,
  "password": "esp-react"
}
```

#### POST /rest/otaSettings

Update OTA configuration.

**Security**: IS_ADMIN

**Request**: Same structure as GET response

### Security

#### GET /rest/securitySettings

Get security settings including users.

**Security**: IS_ADMIN

**Response** (200 OK):

```json
{
  "users": [
    {
      "username": "admin",
      "password": "$2y$10$...",
      "admin": true
    },
    {
      "username": "guest",
      "password": "$2y$10$...",
      "admin": false
    }
  ],
  "jwt_secret": "randomsecretstring"
}
```

#### POST /rest/securitySettings

Update security settings.

**Security**: IS_ADMIN

**Request**: Same structure as GET response

### System Management

#### GET /rest/systemStatus

Get system information.

**Security**: IS_AUTHENTICATED

**Response** (200 OK):

```json
{
  "esp_platform": "esp8266",
  "max_alloc_heap": 32768,
  "psram_size": 0,
  "free_psram": 0,
  "cpu_freq_mhz": 160,
  "sketch_size": 524288,
  "free_sketch_space": 3670016,
  "sdk_version": "2.2.2-dev(38a443e)",
  "flash_chip_size": 4194304,
  "flash_chip_speed": 40000000
}
```

#### POST /rest/restart

Restart the device.

**Security**: IS_ADMIN

**Request**: Empty body

**Response** (200 OK):

```json
{
  "message": "Restarting device..."
}
```

#### POST /rest/factoryReset

Perform factory reset.

**Security**: IS_ADMIN

**Request**: Empty body

**Response** (200 OK):

```json
{
  "message": "Factory reset complete"
}
```

#### POST /rest/uploadFirmware

Upload new firmware.

**Security**: IS_ADMIN

**Request**: `multipart/form-data` with firmware binary

**Response** (200 OK):

```json
{
  "message": "Firmware uploaded successfully"
}
```

### Features

#### GET /rest/features

Get enabled feature flags.

**Security**: None (public)

**Response** (200 OK):

```json
{
  "project": true,
  "security": true,
  "mqtt": true,
  "ntp": true,
  "ota": true,
  "upload_firmware": true
}
```

### Demo Project Endpoints

#### GET /rest/ledExample

Get LED state.

**Security**: IS_AUTHENTICATED

**Response** (200 OK):

```json
{
  "led_on": false
}
```

#### POST /rest/ledExample

Update LED state.

**Security**: IS_AUTHENTICATED

**Request**:

```json
{
  "led_on": true
}
```

**Response** (200 OK): Same as GET response

#### GET /rest/serial

Get last received serial line, extracted weight, and serial/regex configuration.

**Security**: IS_AUTHENTICATED

**Response** (200 OK):

```json
{
  "last_line": "Weight: 12.5 kg",
  "weight": "12.5",
  "timestamp": 12345678,
  "baud_rate": 115200,
  "data_bits": 8,
  "stop_bits": 1,
  "parity": 0,
  "regex_pattern": "(\\d+\\.\\d+)"
}
```

**Field Descriptions**:

- `last_line`: Most recently received complete line from Serial2
- `weight`: Extracted value from first regex capture group, or empty string
- `timestamp`: ESP32 millis() when line was received
- `baud_rate`, `data_bits`, `stop_bits`, `parity`, `regex_pattern`: Serial port and weight-extraction configuration

#### POST /rest/serial

Update serial port and regex configuration. **Request body must contain only config fields** (`baud_rate`, `data_bits`, `stop_bits`, `parity`, `regex_pattern`). Do not send `last_line`, `weight`, or `timestamp` (read-only; resending them can cause 400 Bad Request if they contain binary or invalid UTF-8 from the scale). Response same as GET.

#### GET /rest/diagnostics

Get current UART diagnostic test state (loopback, baud scan, signal quality).

**Security**: IS_AUTHENTICATED

**Response** (200 OK):

```json
{
  "loopback": {
    "enabled": true,
    "status": "pass",
    "tx_count": 142,
    "rx_count": 142,
    "error_count": 0,
    "success_rate": 100.0,
    "last_test": "TEST:142",
    "last_received": "TEST:142",
    "uptime_seconds": 14
  },
  "baud_scan": {
    "enabled": false,
    "status": "found",
    "detected_baud": 9600,
    "current_index": 3,
    "current_baud": 9600,
    "test_packets": 5
  },
  "signal_quality": {
    "enabled": false,
    "status": "complete",
    "quality_percent": 98,
    "total_packets": 1000,
    "sent_packets": 1000,
    "received_packets": 980,
    "avg_latency_ms": 0.52,
    "jitter_ms": 0.08,
    "error_count": 20,
    "progress": 100.0
  }
}
```

**Field Descriptions**:

**Loopback Test**:

- `enabled`: Test is currently running
- `status`: "idle", "running", "pass" (≥95% success), "fail" (<95%)
- `tx_count`, `rx_count`, `error_count`: Packet counters
- `success_rate`: (rx_count - error_count) / tx_count * 100
- `last_test`, `last_received`: Most recent test strings
- `uptime_seconds`: Test duration

**Baud Scan**:

- `enabled`: Scan is currently running
- `status`: "idle", "scanning", "found", "not_found"
- `detected_baud`: Detected baud rate (0 if not found)
- `current_index`: Index in baud rate array being tested (0-7)
- `current_baud`: Current baud rate being tested
- `test_packets`: Packets received at current baud

**Signal Quality**:

- `enabled`: Test is currently running
- `status`: "idle", "running", "complete"
- `quality_percent`: Success rate 0-100%
- `total_packets`, `sent_packets`, `received_packets`: Packet counters
- `avg_latency_ms`: Average round-trip latency
- `jitter_ms`: Latency standard deviation
- `error_count`: Corrupted/mismatched packets
- `progress`: Test completion percentage

#### POST /rest/diagnostics

Start/stop diagnostic tests and configure parameters.

**Security**: IS_AUTHENTICATED

**Start Loopback Test**:

```json
{
  "loopback_enabled": true
}
```

**Stop Loopback Test**:

```json
{
  "loopback_enabled": false
}
```

**Start Baud Scan**:

```json
{
  "baud_scan_enabled": true
}
```

**Stop Baud Scan**:

```json
{
  "baud_scan_enabled": false
}
```

**Start Signal Quality Test**:

```json
{
  "signal_test_enabled": true,
  "signal_total_packets": 1000
}
```

**Stop Signal Quality Test**:

```json
{
  "signal_test_enabled": false
}
```

**Response** (200 OK): Same as GET response

#### GET /rest/brokerSettings

Get MQTT topic configuration for light.

**Security**: IS_AUTHENTICATED

**Response** (200 OK):

```json
{
  "unique_id": "light-abc123",
  "name": "ESP Light",
  "mqtt_path": "homeassistant/light/esp_light"
}
```

#### POST /rest/brokerSettings

Update MQTT topic configuration.

**Security**: IS_AUTHENTICATED

**Request**: Same structure as GET response

#### GET /rest/weightForwarder

Get weight stream forwarder configuration and status.

**Security**: IS_AUTHENTICATED

**Response** (200 OK):

```json
{
  "protocol": 0,
  "target_url": "http://192.168.1.50:8080/weight",
  "ws_url": "ws://192.168.1.50:8080/ws",
  "mqtt_topic": "remote/device/weight",
  "ble_service_uuid": "12340000-e8f2-537e-4f6c-d104768a1234",
  "ble_char_uuid": "12340001-e8f2-537e-4f6c-d104768a1234",
  "enabled": true,
  "display_mode": false,
  "auth_username": "",
  "auth_password": "",
  "connected": true,
  "last_error": "",
  "last_forward_time": 1234567890
}
```

**Field Descriptions**:

- `protocol`: Forward protocol (0=HTTP POST, 1=WebSocket Client, 2=MQTT, 3=BLE Client)
- `target_url`: HTTP POST endpoint URL (e.g. `http://192.168.3.100/rest/display` for display device)
- `ws_url`: WebSocket client endpoint URL
- `mqtt_topic`: MQTT publish topic
- `ble_service_uuid`: Remote BLE service UUID to connect to
- `ble_char_uuid`: Remote BLE characteristic UUID to write to
- `enabled`: Master enable/disable toggle
- `display_mode`: Data format toggle (false=standard JSON, true=16-char LCD format)
- `auth_username`: Optional username for HTTP target when it requires login (e.g. display device returns 401)
- `auth_password`: Optional password for HTTP target; used to obtain JWT via target's `/rest/signIn`
- `connected`: Current connection status to remote device
- `last_error`: Most recent error message (empty if no error)
- `last_forward_time`: Timestamp of last successful forward (millis)

**HTTP target authentication**: When `target_url` points to a protected endpoint (e.g. a display device's `/rest/display`), set `auth_username` and `auth_password` to the target device's admin credentials. The forwarder will sign in to the target's `/rest/signIn`, obtain a JWT, and send `Authorization: Bearer <token>` with each POST. On 401 it re-logs in and retries once.

**Protocol Values**:

- `0` - HTTP POST: Posts JSON to `target_url` on each weight update
- `1` - WebSocket Client: Connects to `ws_url` and streams JSON
- `2` - MQTT: Publishes JSON to `mqtt_topic` (requires FT_MQTT=1)
- `3` - BLE Client: Connects to remote BLE device and writes JSON to characteristic (requires FT_BLE=1)

**Display Mode**:

- `false` (Standard): `{"last_line":"...", "weight":"...", "timestamp":123}`
- `true` (LCD): `{"line1":"Weight: 12.5 kg ", "line2":"..."}`  (16 chars per line, padded/truncated)

**Note**: Protocol availability is controlled by feature flags. If `FT_MQTT=0`, MQTT protocol is not selectable. If `FT_BLE=0`, BLE Client protocol is not selectable.

#### POST /rest/weightForwarder

Update weight forwarder configuration.

**Security**: IS_AUTHENTICATED

**Request**:

```json
{
  "protocol": 1,
  "ws_url": "ws://192.168.1.100:3000/weight",
  "enabled": true,
  "display_mode": true
}
```

**Response**: Same as GET

**Behavior**:

- Protocol switch is handled automatically (previous protocol resources cleaned up)
- Configuration persists to flash on update
- Automatic reconnection for WebSocket and BLE protocols
- Rate limited to max 10 forwards per second
- Skips forwarding if WiFi disconnected
- Errors are logged to `last_error` field and forwarding continues

## WebSocket Endpoints

**Base Path**: `/ws/`

**Protocol**: WebSocket (ws:// or wss://)

**Authentication**: Applied at connection time via filter

### Message Format

**Client ID Message** (Server → Client on connect):

```json
{
  "type": "id",
  "id": "websocket:12345"
}
```

**Payload Message** (Bidirectional):

```json
{
  "type": "payload",
  "origin_id": "http",
  "payload": {
    /* state data */
  }
}
```

### WebSocket Endpoints

#### /ws/ledExample

Real-time LED state synchronization.

**Security**: IS_AUTHENTICATED

**Payload Format**:

```json
{
  "led_on": true
}
```

**Behavior**: Bidirectional - changes from any source (REST, MQTT, BLE, WebSocket) are broadcast to all WebSocket clients.

#### /ws/serial

Real-time serial data streaming.

**Security**: IS_AUTHENTICATED

**Payload Format**:

```json
{
  "last_line": "Weight: 12.5 kg",
  "weight": "12.5",
  "timestamp": 12345678
}
```

**Behavior**: Server → Client only. New lines from Serial2 are automatically pushed to all connected clients.

**Flow**:

1. Client connects
2. Server sends client ID
3. Server sends current state
4. Client can send updates (bidirectional)
5. Server broadcasts changes to all clients

#### /ws/uartMode

Real-time UART mode change notifications.

**Security**: IS_AUTHENTICATED

**Message Format**:

```json
{
  "mode": "live"
}
```

**Direction**: Server → Client (broadcast on mode change)

#### /ws/weightForwarder

Real-time weight forwarder status and configuration streaming.

**Security**: IS_AUTHENTICATED

**Payload Format**:

```json
{
  "protocol": 1,
  "target_url": "http://192.168.1.50:8080/weight",
  "ws_url": "ws://192.168.1.50:8080/ws",
  "mqtt_topic": "remote/device/weight",
  "ble_service_uuid": "12340000-e8f2-537e-4f6c-d104768a1234",
  "ble_char_uuid": "12340001-e8f2-537e-4f6c-d104768a1234",
  "enabled": true,
  "display_mode": false,
  "connected": true,
  "last_error": "",
  "last_forward_time": 1234567890
}
```

**Behavior**: Bidirectional - client can update configuration, server broadcasts status updates (connection state, errors, last forward time).

**Usage**:

- **Client → Server**: Send configuration updates (same as POST /rest/weightForwarder)
- **Server → Client**: Automatic updates when connection status changes, errors occur, or configuration is modified
- **Frequency**: Updates pushed on every connection state change, error, or successful forward

#### /ws/diagnostics

Real-time UART diagnostic test updates.

**Security**: IS_AUTHENTICATED

**Payload Format**:

```json
{
  "loopback": {
    "enabled": true,
    "status": "pass",
    "tx_count": 142,
    "rx_count": 142,
    "error_count": 0,
    "success_rate": 100.0,
    "last_test": "TEST:142",
    "last_received": "TEST:142",
    "uptime_seconds": 14
  },
  "baud_scan": {
    "enabled": false,
    "status": "found",
    "detected_baud": 9600,
    "current_index": 3,
    "current_baud": 9600,
    "test_packets": 5
  },
  "signal_quality": {
    "enabled": false,
    "status": "complete",
    "quality_percent": 98,
    "total_packets": 1000,
    "sent_packets": 1000,
    "received_packets": 980,
    "avg_latency_ms": 0.52,
    "jitter_ms": 0.08,
    "error_count": 20,
    "progress": 100.0
  }
}
```

**Behavior**: Bidirectional - client can start/stop tests, server broadcasts real-time test updates.

**Usage**:

- **Client → Server**: Send `{"loopback_enabled": true}` to start loopback test
- **Server → Client**: Automatic updates every time test state changes (new packet sent/received)
- **Frequency**: Updates pushed on every test event (100ms for loopback, varies for other tests)

## MQTT Topics

### Topic Structure

Projects define their own MQTT topic structure. The framework provides two example patterns:

#### LED Example Topics (Bidirectional)

**Base Path**: Configured in project settings (e.g., `homeassistant/light/{unique_id}`)

**Topics**:

- `{mqtt_path}/config` - Home Assistant discovery
- `{mqtt_path}/state` - Device publishes state updates
- `{mqtt_path}/set` - Device subscribes to commands

#### Serial Example Topics (Publish-Only)

**Base Path**: `weighsoft/serial/{unique_id}`

**Topics**:

- `weighsoft/serial/{unique_id}/data` - Device publishes serial lines

**Example**:

```
Topic: weighsoft/serial/a4e57cdb7928/data
Payload: {"last_line":"Weight: 12.5 kg","weight":"12.5","timestamp":12345678}
```

### Home Assistant Discovery

**Topic**: `{mqtt_path}/config`

**Payload** (Published by device):

```json
{
  "name": "ESP Light",
  "unique_id": "light-abc123",
  "cmd_t": "homeassistant/light/esp_light/set",
  "stat_t": "homeassistant/light/esp_light/state",
  "schema": "json",
  "brightness": false
}
```

**Retention**: Retained message

**QoS**: 0

### State Topic

**Topic**: `{mqtt_path}/state`

**Payload** (Published by device):

```json
{
  "state": "ON"
}
```

**Retention**: Retained message

**QoS**: 0

**Frequency**: On state change

### Command Topic

**Topic**: `{mqtt_path}/set`

**Payload** (Subscribed by device):

```json
{
  "state": "OFF"
}
```

**QoS**: 2 (exactly once)

**Response**: Device publishes updated state to state topic

## BLE Characteristics

When BLE is enabled (`FT_BLE=1`), services expose characteristics for wireless communication.

### LED Example BLE Service

**Service UUID**: `19b10000-e8f2-537e-4f6c-d104768a1214`

**Characteristic UUID**: `19b10001-e8f2-537e-4f6c-d104768a1214`

**Properties**: READ | WRITE | NOTIFY

**Value Format**: UTF-8 JSON string

```json
{"led_on":true}
```

**Usage**:

- READ: Get current LED state
- WRITE: Change LED state (triggers notifications to all subscribers)
- NOTIFY: Receive updates when state changes from any source

### Serial Example BLE Service

**Service UUID**: `12340000-e8f2-537e-4f6c-d104768a1234`

**Characteristic UUID**: `12340001-e8f2-537e-4f6c-d104768a1234`

**Properties**: READ | NOTIFY

**Value Format**: UTF-8 JSON string

```json
{"last_line":"Weight: 12.5 kg","weight":"12.5","timestamp":12345678}
```

**Usage**:

- READ: Get last received serial line and extracted weight
- NOTIFY: Receive updates as new lines arrive from Serial2

**Note**: Read-only - no WRITE property (data flows from hardware only)

## HTTP Status Codes

| Code | Meaning | Usage |
|------|---------|-------|
| 200 | OK | Successful request |
| 400 | Bad Request | Invalid JSON or parameters |
| 401 | Unauthorized | Missing or invalid JWT token |
| 403 | Forbidden | Insufficient permissions |
| 404 | Not Found | Endpoint doesn't exist |
| 500 | Internal Server Error | Backend error |

## Security Headers

### JWT Token Format

**Header**:

```
Authorization: Bearer eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9...
```

**JWT Payload**:

```json
{
  "username": "admin",
  "admin": true,
  "exp": 1705334400
}
```

**Signature**: HMAC-SHA256 with JWT secret from security settings

### CORS Headers

When `ENABLE_CORS` is defined:

```
Access-Control-Allow-Origin: *
Access-Control-Allow-Headers: Accept, Content-Type, Authorization
Access-Control-Allow-Credentials: true
```

## Error Response Format

**Standard Error Response**:

```json
{
  "message": "Error description"
}
```

**Validation Error Example**:

```json
{
  "message": "Invalid SSID length"
}
```

## Rate Limiting

No built-in rate limiting. Consider network-level protection for production deployments.

## Best Practices

### For API Consumers

1. **Always check HTTP status codes**
2. **Handle 401 responses** by refreshing token or re-authenticating
3. **Include JWT token** in Authorization header for protected endpoints
4. **Use WebSocket for real-time data** instead of polling REST endpoints
5. **Validate input** before sending to API
6. **Handle network errors** gracefully with retry logic

### For API Implementers

1. **Return appropriate HTTP status codes**
2. **Use StateUpdateResult::ERROR** for validation failures
3. **Implement JsonStateReader/Updater** with validation
4. **Check origin ID** in update handlers to prevent loops
5. **Use authentication predicates** appropriately
6. **Document custom endpoints** following this format

## Next Steps

- [SEQUENCE-DIAGRAMS.md](SEQUENCE-DIAGRAMS.md) - See API flows
- [DATA-FLOWS.md](DATA-FLOWS.md) - Understand data movement
- [SECURITY.md](SECURITY.md) - Security implementation
- [EXTENSION-GUIDE.md](EXTENSION-GUIDE.md) - Add custom endpoints
