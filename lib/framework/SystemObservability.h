#ifndef SystemObservability_h
#define SystemObservability_h

class AsyncWebServer;

// Boot: reset reason, chip, heap (call once early in setup() after Serial.begin).
void systemObservabilityBootLog();

// Register global Wi-Fi event logging (ESP32). Safe to call once from framework begin().
void systemObservabilityRegister(AsyncWebServer* webServer);

// Periodic RAM / Wi-Fi / uptime snapshot (ESP32). Call from main framework loop.
void systemObservabilityLoop();

#endif
