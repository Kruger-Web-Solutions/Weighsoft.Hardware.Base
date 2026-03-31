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

#### GET /rest/lightState

Get light state.

**Security**: IS_AUTHENTICATED

**Response** (200 OK):
```json
{
  "led_on": false
}
```

#### POST /rest/lightState

Update light state.

**Security**: IS_AUTHENTICATED

**Request**:
```json
{
  "led_on": true
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

#### /ws/lightState

Real-time light state synchronization.

**Security**: IS_AUTHENTICATED

**Payload Format**:
```json
{
  "led_on": true
}
```

**Flow**:
1. Client connects
2. Server sends client ID
3. Server sends current state
4. Client can send updates (bidirectional)
5. Server broadcasts changes to all clients

## MQTT Topics

### Topic Structure

Projects define their own MQTT topic structure. The demo project uses:

**Base Path**: Configured in `/rest/brokerSettings`

**Topics**:
- `{mqtt_path}/config` - Home Assistant discovery
- `{mqtt_path}/state` - Device publishes state updates
- `{mqtt_path}/set` - Device subscribes to commands

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

---

## Growbox Integration Endpoints

> These endpoints are available on the `growbox` branch.

### REST

| Method | Path | Description |
|--------|------|-------------|
| GET/POST | `/rest/growbox` | Live relay state, sensor readings, alarms |
| GET/POST | `/rest/growboxSettings` | Hardware config (GPIO pins, calibration) |
| GET/POST | `/rest/growboxAutomation` | Automation settings (schedules, thresholds) |

### WebSocket

| Path | Data |
|------|------|
| `/ws/growbox` | Live `GrowboxState` stream |
| `/ws/growboxAutomation` | `GrowboxAutomationSettings` stream |

### MQTT Topics

All topics are namespaced under `growbox/<device_unique_id>/`.

| Topic | Direction | Description |
|-------|-----------|-------------|
| `growbox/<id>/lights/state` | Publish | Lights state `{"state":"ON/OFF"}` |
| `growbox/<id>/lights/set` | Subscribe | Control lights |
| `growbox/<id>/fan_in/state` | Publish | Fan In state |
| `growbox/<id>/fan_in/set` | Subscribe | Control Fan In |
| `growbox/<id>/fan_out/state` | Publish | Fan Out state |
| `growbox/<id>/fan_out/set` | Subscribe | Control Fan Out |
| `growbox/<id>/state` | Publish | Full sensor state JSON |
| `growbox/<id>/automation/state` | Publish | Automation settings |
| `growbox/<id>/automation/set` | Subscribe | Update automation settings |
| `homeassistant/.../<id>/config` | Publish (on connect) | HA auto-discovery payloads |

### BLE Service

Service UUID: `4b6e5d7f-bf21-4e9a-9c87-6e5d1042a3b1`

| Characteristic | UUID | Properties | Description |
|---------------|------|-----------|-------------|
| Controls | `a3b4c5d6-...a3b2` | R/W/Notify | Relay outputs + alarm (compact JSON) |
| Sensors | `b5c6d7e8-...a3b3` | R/Notify | Live sensor readings (compact JSON) |
| Automation | `c7d8e9f0-...a3b4` | R/W/Notify | Automation settings (full key names) |

See [GROWBOX-EXAMPLE.md](GROWBOX-EXAMPLE.md) for complete field reference and usage examples.
