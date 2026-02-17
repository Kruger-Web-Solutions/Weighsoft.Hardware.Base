# Security Architecture

## Overview

This document describes the security architecture of the ESP8266-React framework, including authentication, authorization, JWT implementation, and security best practices.

## Security Model

### Authentication vs Authorization

**Authentication**: Verify identity (who are you?)

- Handled by JWT tokens
- Sign in with username/password
- Token stored in browser

**Authorization**: Verify permissions (what can you do?)

- Handled by authentication predicates
- Three levels: NONE_REQUIRED, IS_AUTHENTICATED, IS_ADMIN
- Checked on every request

## JWT (JSON Web Token) Implementation

### Token Structure

**Header**:

```json
{
  "typ": "JWT",
  "alg": "HS256"
}
```

**Payload**:

```json
{
  "username": "admin",
  "admin": true,
  "exp": 1705334400
}
```

**Signature**: HMAC-SHA256(base64(header) + "." + base64(payload), secret)

### Token Generation

```cpp
// In AuthenticationService
String generateToken(User& user) {
    unsigned long exp = now() + JWT_EXPIRY;  // Default: 3600 seconds
    
    DynamicJsonDocument doc(256);
    doc["username"] = user.username;
    doc["admin"] = user.admin;
    doc["exp"] = exp;
    
    return ArduinoJsonJWT::sign(doc, jwtSecret);
}
```

### Token Validation

```cpp
// In SecurityManager
bool validateToken(const String& token, User& user) {
    DynamicJsonDocument doc = ArduinoJsonJWT::verify(token, jwtSecret);
    
    if (doc.isNull()) return false;  // Invalid signature
    
    unsigned long exp = doc["exp"];
    if (exp < now()) return false;  // Expired
    
    user.username = doc["username"];
    user.admin = doc["admin"];
    return true;
}
```

## Authentication Predicates

### NONE_REQUIRED

**Usage**: Public endpoints (sign-in, features)

**Implementation**:

```cpp
server->on("/rest/features", HTTP_GET, handler);  // No security wrapper
```

**Risk**: Anyone on network can access

### IS_AUTHENTICATED

**Usage**: User-level endpoints (status, basic config)

**Implementation**:

```cpp
server->on("/rest/wifiStatus", HTTP_GET, 
    securityManager->wrapRequest(handler, AuthenticationPredicates::IS_AUTHENTICATED)
);
```

**Check**: Verifies valid JWT token exists

### IS_ADMIN

**Usage**: Administrative endpoints (security, firmware upload)

**Implementation**:

```cpp
server->on("/rest/securitySettings", HTTP_GET,
    securityManager->wrapRequest(handler, AuthenticationPredicates::IS_ADMIN)
);
```

**Check**: Verifies valid JWT token AND admin=true

## Request Security Wrapping

### REST Endpoint Wrapping

```cpp
// Secure endpoint
HttpEndpoint<Settings> endpoint(
    Settings::read,
    Settings::update,
    this,
    server,
    "/rest/settings",
    securityManager,                      // SecurityManager provided
    AuthenticationPredicates::IS_ADMIN    // Admin required
);

// Unsecured endpoint
HttpEndpoint<Settings> endpoint(
    Settings::read,
    Settings::update,
    this,
    server,
    "/rest/settings"  // No SecurityManager
);
```

### WebSocket Security

```cpp
// Secure WebSocket
WebSocketTxRx<State> webSocket(
    State::read,
    State::update,
    this,
    server,
    "/ws/state",
    securityManager,                      // Filter applied
    AuthenticationPredicates::IS_AUTHENTICATED
);
```

**Filter Applied**: Connection rejected if authentication fails

## Password Security

### Password Hashing

**Algorithm**: bcrypt (cost factor 10)

**Storage**: Only hashed passwords stored

```cpp
// Hash password before storing
String hashedPassword = hashPassword(plainPassword);

// Verify password
bool valid = verifyPassword(plainPassword, hashedPassword);
```

### Password Requirements

**Minimum Length**: 8 characters (recommended)

**Validation** (frontend):

```typescript
password: [
    { required: true },
    { min: 8, message: 'At least 8 characters' }
]
```

## Default Credentials

**CRITICAL**: Change these immediately in production!

| Username | Password | Role | Risk Level |
|----------|----------|------|------------|
| admin | admin | Admin | CRITICAL |
| guest | guest | User | HIGH |

### Changing Defaults

**Method 1**: Via factory_settings.ini before first flash

```ini
-D FACTORY_ADMIN_USERNAME=\"myadmin\"
-D FACTORY_ADMIN_PASSWORD=\"mySecureP@ssw0rd\"
```

**Method 2**: Via UI after deployment

1. Sign in as admin
2. Navigate to Security section
3. Edit users
4. Save

**Method 3**: Via REST API

```bash
curl -X POST http://device/rest/securitySettings \
  -H "Authorization: Bearer TOKEN" \
  -d '{"users":[{"username":"admin","password":"newpass","admin":true}]}'
```

## JWT Secret Security

### Secret Generation

**Default**: Randomly generated on first boot

```cpp
jwtSecret = SettingValue::format("#{random}-#{random}");
```

**Stored**: `/config/securitySettings.json`

**Length**: 16 characters (hex)

### Secret Rotation

**Manual Rotation**:

1. Sign in as admin
2. Edit `/config/securitySettings.json` manually
3. Restart device
4. All users must sign in again

**Automatic**: Not supported (would invalidate all tokens)

## Network Security

### Threat Model

**Assumed Threats**:

- Unauthorized local network access
- Man-in-the-middle attacks (no HTTPS by default)
- Brute force attacks on weak passwords
- Token theft from browser storage

**Not Protected Against**:

- Physical access to device
- Serial port access
- Flash memory dump
- Network sniffing (no TLS)

### HTTPS Support

**Not Built-In**: Requires additional setup

**Options**:

1. Reverse proxy (nginx) with TLS termination
2. ESP32 with mbedTLS (custom implementation)
3. VPN for secure access

### Network Isolation

**Recommendation**: Deploy on isolated VLAN or subnet

**Firewall Rules**:

- Allow port 80 (HTTP) only from trusted networks
- Allow port 8266 (OTA) only from admin network
- Block internet access if not needed

## MQTT Security

### Authentication

**Support**: Username/password authentication

**Configuration**:

```json
{
  "username": "mqtt_user",
  "password": "mqtt_password"
}
```

**Recommendation**: Always enable MQTT broker authentication

### TLS/SSL

**Not Built-In**: AsyncMqttClient supports TLS but requires additional setup

**Implementation**: Configure secure port (8883) and certificates

### Topic ACLs

**Broker-Side**: Configure access control lists

**Example** (Mosquitto):

```
user mqtt_device
topic readwrite homeassistant/sensor/esp_temp/#
topic read homeassistant/status
```

## Security Best Practices

### Deployment Checklist

- [ ] Change default admin password
- [ ] Change default guest password (or remove)
- [ ] Change default AP password
- [ ] Enable MQTT authentication
- [ ] Use strong JWT secret (leave as random)
- [ ] Disable unused features (FT_* flags)
- [ ] Enable HTTPS (if possible)
- [ ] Isolate network/VLAN
- [ ] Disable serial console in production
- [ ] Set OTA password
- [ ] Regular firmware updates

### Development vs Production

| Setting | Development | Production |
|---------|-------------|------------|
| Admin Password | "admin" | Strong password |
| AP Password | "esp-react" | Strong password |
| JWT Secret | Random | Random (don't change) |
| CORS | Enabled | Disabled |
| Serial Logging | Verbose | Minimal |
| OTA | Enabled | Password protected |

## Attack Vectors and Mitigations

### 1. Weak Credentials

**Attack**: Brute force default passwords

**Mitigation**:

- Change defaults immediately
- Use strong passwords (12+ chars, mixed case, numbers, symbols)
- Consider disabling guest account

### 2. Token Theft

**Attack**: XSS or local storage access

**Mitigation**:

- Keep firmware updated
- Use HTTPS
- Short token expiry (default 1 hour)
- HttpOnly cookies (not supported, use with caution)

### 3. MQTT Snooping

**Attack**: Intercept MQTT messages

**Mitigation**:

- Use MQTT authentication
- Enable TLS (port 8883)
- Use broker ACLs
- Encrypt sensitive payloads

### 4. Physical Access

**Attack**: Serial console access, flash dump

**Mitigation**:

- Physically secure device
- Disable serial in production build
- Use flash encryption (ESP32 only)
- Use secure boot (ESP32 only)

### 5. Network Scanning

**Attack**: Discover devices on network

**Mitigation**:

- Change default AP SSID pattern
- Use hidden SSID (AP mode)
- Network isolation
- MAC filtering

## Audit and Logging

### Current Logging

**Serial Output**:

- Connection events
- Errors
- State changes (debug builds)

**Not Logged**:

- Authentication attempts
- Failed sign-ins
- API access logs

### Recommendations

Add logging for:

- Failed authentication attempts
- Admin actions (user management, factory reset)
- Firmware uploads
- Configuration changes

## Compliance Considerations

### GDPR

**User Data**: Usernames stored in device

**Rights**:

- Right to access: Export securitySettings.json
- Right to erasure: Factory reset or delete user
- Right to portability: JSON format

### Password Storage

**Compliance**: Passwords hashed with bcrypt (industry standard)

**NOT Stored**: Plaintext passwords never stored

## Next Steps

- [API-REFERENCE.md](API-REFERENCE.md) - Secured endpoints
- [CONFIGURATION.md](CONFIGURATION.md) - Configure security
- [DESIGN-PATTERNS.md](DESIGN-PATTERNS.md) - Implementation patterns
