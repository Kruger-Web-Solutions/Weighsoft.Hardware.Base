#ifndef WeighingState_h
#define WeighingState_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <StatefulService.h>
#include <mbedtls/sha256.h>

// Factory calibration PIN -- set in factory_settings.ini, compile-time only
#ifndef FACTORY_CALIBRATION_PIN
#define FACTORY_CALIBRATION_PIN "000000"
#endif

// Default configuration values
#define WEIGHING_DEFAULT_UNIT             "kg"
#define WEIGHING_DEFAULT_MAX_CAPACITY     100.0f
#define WEIGHING_DEFAULT_MIN_DIVISION     0.02f
#define WEIGHING_DEFAULT_DECIMAL_PLACES   2
#define WEIGHING_DEFAULT_ZERO_TRACKING    true
#define WEIGHING_DEFAULT_ZERO_TRACK_RANGE 0.01f   // 0.5d at default 0.02 division
#define WEIGHING_DEFAULT_POWER_ON_ZERO    10.0f   // 10% of max capacity
#define WEIGHING_DEFAULT_MANUAL_ZERO      2.0f    // 2% of max capacity
#define WEIGHING_DEFAULT_SPEED_MODE       0       // 0=10SPS, 1=80SPS
#define WEIGHING_DEFAULT_SAMPLES          8

// SHA-256 hex digest of FACTORY_CALIBRATION_PIN computed at runtime in begin()
// We store it as a 64-char hex string
static String _weighingPinHash;    // populated once by WeighingService::begin()
static uint8_t _failedPinAttempts = 0;
static unsigned long _pinLockoutUntil = 0;
#define WEIGHING_PIN_MAX_ATTEMPTS 5
#define WEIGHING_PIN_LOCKOUT_MS   300000UL  // 5 minutes

// Helper: SHA-256 hash a string, return 64-char hex string
static String weighingHashPin(const String& pin) {
  uint8_t hash[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0);
  mbedtls_sha256_update_ret(&ctx, (const unsigned char*)pin.c_str(), pin.length());
  mbedtls_sha256_finish_ret(&ctx, hash);
  mbedtls_sha256_free(&ctx);

  String hex = "";
  hex.reserve(64);
  for (int i = 0; i < 32; i++) {
    if (hash[i] < 0x10) hex += "0";
    hex += String(hash[i], HEX);
  }
  return hex;
}

// Initialise pin hash from factory constant -- call once at startup
static void weighingInitPinHash() {
  _weighingPinHash = weighingHashPin(String(FACTORY_CALIBRATION_PIN));
}

// Verify incoming PIN against factory hash; enforces brute-force lockout
// Returns true if PIN correct AND not locked out
static bool weighingVerifyPin(const String& pin) {
  unsigned long now = millis();
  if (_pinLockoutUntil > 0 && now < _pinLockoutUntil) {
    return false;  // Still locked out
  }
  if (_weighingPinHash.isEmpty()) {
    weighingInitPinHash();
  }
  bool match = (weighingHashPin(pin) == _weighingPinHash);
  if (match) {
    _failedPinAttempts = 0;
    _pinLockoutUntil = 0;
  } else {
    _failedPinAttempts++;
    if (_failedPinAttempts >= WEIGHING_PIN_MAX_ATTEMPTS) {
      _pinLockoutUntil = now + WEIGHING_PIN_LOCKOUT_MS;
      _failedPinAttempts = 0;
    }
  }
  return match;
}

// Returns remaining lockout seconds (0 = not locked)
static uint32_t weighingPinLockoutSeconds() {
  unsigned long now = millis();
  if (_pinLockoutUntil > 0 && now < _pinLockoutUntil) {
    return (uint32_t)((_pinLockoutUntil - now) / 1000);
  }
  return 0;
}

class WeighingState {
 public:
  // --- Live / measurement fields (NOT persisted, updated every loop cycle) ---
  float    grossWeight   = 0.0f;
  float    netWeight     = 0.0f;
  float    tareValue     = 0.0f;
  String   weightMode    = "gross";  // "gross" or "net"
  float    weight        = 0.0f;     // displayed weight (gross or net)
  int32_t  rawValue      = 0;
  bool     stable        = false;
  bool     centerOfZero  = false;
  bool     overload      = false;
  bool     underload     = false;
  bool     adcConnected  = false;

  // --- Calibration / configuration fields (persisted) ---
  String   unit                 = WEIGHING_DEFAULT_UNIT;
  int32_t  zeroOffset           = 0;
  float    calibrationFactor    = 1.0f;
  float    maxCapacity          = WEIGHING_DEFAULT_MAX_CAPACITY;
  float    minDivision          = WEIGHING_DEFAULT_MIN_DIVISION;
  uint8_t  decimalPlaces        = WEIGHING_DEFAULT_DECIMAL_PLACES;
  bool     zeroTrackingEnabled  = WEIGHING_DEFAULT_ZERO_TRACKING;
  float    zeroTrackingRange    = WEIGHING_DEFAULT_ZERO_TRACK_RANGE;
  float    powerOnZeroRange     = WEIGHING_DEFAULT_POWER_ON_ZERO;
  float    manualZeroRange      = WEIGHING_DEFAULT_MANUAL_ZERO;
  uint8_t  speedMode            = WEIGHING_DEFAULT_SPEED_MODE;
  uint8_t  samplesToAverage     = WEIGHING_DEFAULT_SAMPLES;

  // --- Seal / audit fields (persisted) ---
  bool     sealActive           = false;
  uint32_t eventCounter         = 0;     // OIML 6.2.3.5 -- never decreases
  String   lastCalibrationTime  = "";

  // --- Command fields (transient, not persisted, not in read() output) ---
  bool     tareCommand          = false;
  bool     presetTareCommand    = false;
  float    presetTareValue      = 0.0f;
  bool     clearTareCommand     = false;
  bool     zeroCommand          = false;
  bool     calibrateCommand     = false;
  float    knownWeight          = 0.0f;
  bool     sealCommand          = false;
  bool     unsealCommand        = false;
  String   pin                  = "";    // for unseal verification only

  // --- Full state read (all non-sensitive fields) for REST/WS/MQTT/BLE ---
  static void read(WeighingState& state, JsonObject& root) {
    // Live measurement
    root["weight"]          = state.weight;
    root["gross_weight"]    = state.grossWeight;
    root["net_weight"]      = state.netWeight;
    root["tare_value"]      = state.tareValue;
    root["weight_mode"]     = state.weightMode;
    root["raw_value"]       = state.rawValue;
    root["stable"]          = state.stable;
    root["center_of_zero"]  = state.centerOfZero;
    root["overload"]        = state.overload;
    root["underload"]       = state.underload;
    root["adc_connected"]   = state.adcConnected;
    // Configuration
    root["unit"]                   = state.unit;
    root["zero_offset"]            = state.zeroOffset;
    root["calibration_factor"]     = state.calibrationFactor;
    root["max_capacity"]           = state.maxCapacity;
    root["min_division"]           = state.minDivision;
    root["decimal_places"]         = state.decimalPlaces;
    root["zero_tracking_enabled"]  = state.zeroTrackingEnabled;
    root["zero_tracking_range"]    = state.zeroTrackingRange;
    root["power_on_zero_range"]    = state.powerOnZeroRange;
    root["manual_zero_range"]      = state.manualZeroRange;
    root["speed_mode"]             = state.speedMode;
    root["samples_to_average"]     = state.samplesToAverage;
    // Seal / audit (PIN hash is NEVER exposed)
    root["seal_active"]            = state.sealActive;
    root["event_counter"]          = state.eventCounter;
    root["last_calibration_time"]  = state.lastCalibrationTime;
    // Lockout info (0 = not locked)
    root["pin_lockout_seconds"]    = weighingPinLockoutSeconds();
  }

  // --- Config-only read for FSPersistence (calibration + seal fields only) ---
  static void readConfig(WeighingState& state, JsonObject& root) {
    root["unit"]                   = state.unit;
    root["zero_offset"]            = state.zeroOffset;
    root["calibration_factor"]     = state.calibrationFactor;
    root["max_capacity"]           = state.maxCapacity;
    root["min_division"]           = state.minDivision;
    root["decimal_places"]         = state.decimalPlaces;
    root["zero_tracking_enabled"]  = state.zeroTrackingEnabled;
    root["zero_tracking_range"]    = state.zeroTrackingRange;
    root["power_on_zero_range"]    = state.powerOnZeroRange;
    root["manual_zero_range"]      = state.manualZeroRange;
    root["speed_mode"]             = state.speedMode;
    root["samples_to_average"]     = state.samplesToAverage;
    root["seal_active"]            = state.sealActive;
    root["event_counter"]          = state.eventCounter;  // monotonic
    root["last_calibration_time"]  = state.lastCalibrationTime;
  }

  // --- Load from flash (FSPersistence). eventCounter NEVER decreases. ---
  static StateUpdateResult updateConfig(JsonObject& root, WeighingState& state) {
    state.unit                  = root["unit"]                  | String(WEIGHING_DEFAULT_UNIT);
    state.zeroOffset            = root["zero_offset"]           | (int32_t)0;
    state.calibrationFactor     = root["calibration_factor"]    | 1.0f;
    state.maxCapacity           = root["max_capacity"]          | WEIGHING_DEFAULT_MAX_CAPACITY;
    state.minDivision           = root["min_division"]          | WEIGHING_DEFAULT_MIN_DIVISION;
    state.decimalPlaces         = root["decimal_places"]        | (uint8_t)WEIGHING_DEFAULT_DECIMAL_PLACES;
    state.zeroTrackingEnabled   = root["zero_tracking_enabled"] | WEIGHING_DEFAULT_ZERO_TRACKING;
    state.zeroTrackingRange     = root["zero_tracking_range"]   | WEIGHING_DEFAULT_ZERO_TRACK_RANGE;
    state.powerOnZeroRange      = root["power_on_zero_range"]   | WEIGHING_DEFAULT_POWER_ON_ZERO;
    state.manualZeroRange       = root["manual_zero_range"]     | WEIGHING_DEFAULT_MANUAL_ZERO;
    state.speedMode             = root["speed_mode"]            | (uint8_t)WEIGHING_DEFAULT_SPEED_MODE;
    state.samplesToAverage      = root["samples_to_average"]    | (uint8_t)WEIGHING_DEFAULT_SAMPLES;
    state.sealActive            = root["seal_active"]           | false;
    state.lastCalibrationTime   = root["last_calibration_time"] | String("");

    // OIML 6.2.3.5: event counter must NEVER decrease
    uint32_t persisted = root["event_counter"] | (uint32_t)0;
    if (persisted > state.eventCounter) {
      state.eventCounter = persisted;
    }

    // Clamp values
    if (state.decimalPlaces > 6) state.decimalPlaces = WEIGHING_DEFAULT_DECIMAL_PLACES;
    if (state.speedMode > 1)     state.speedMode = 0;
    if (state.samplesToAverage < 1 || state.samplesToAverage > 32) state.samplesToAverage = WEIGHING_DEFAULT_SAMPLES;
    if (state.maxCapacity <= 0)  state.maxCapacity = WEIGHING_DEFAULT_MAX_CAPACITY;
    if (state.minDivision <= 0)  state.minDivision = WEIGHING_DEFAULT_MIN_DIVISION;

    return StateUpdateResult::CHANGED;
  }

  // --- Patch-style update from channels (REST POST / WS / MQTT set / BLE write) ---
  static StateUpdateResult update(JsonObject& root, WeighingState& state) {
    StateUpdateResult result = StateUpdateResult::UNCHANGED;

    // --- Commands (never blocked by seal) ---
    if (root.containsKey("tare") && root["tare"].as<bool>()) {
      state.tareCommand = true;
      result = StateUpdateResult::CHANGED;
    }
    if (root.containsKey("preset_tare") && root["preset_tare"].as<bool>()) {
      state.presetTareCommand = true;
      state.presetTareValue = root["preset_tare_value"] | 0.0f;
      result = StateUpdateResult::CHANGED;
    }
    if (root.containsKey("clear_tare") && root["clear_tare"].as<bool>()) {
      state.clearTareCommand = true;
      result = StateUpdateResult::CHANGED;
    }

    // --- Seal-gated commands ---
    if (root.containsKey("zero") && root["zero"].as<bool>()) {
      if (state.sealActive) return StateUpdateResult::ERROR;
      state.zeroCommand = true;
      result = StateUpdateResult::CHANGED;
    }
    if (root.containsKey("calibrate") && root["calibrate"].as<bool>()) {
      if (state.sealActive) return StateUpdateResult::ERROR;
      state.calibrateCommand = true;
      state.knownWeight = root["known_weight"] | 0.0f;
      result = StateUpdateResult::CHANGED;
    }

    // --- Seal command (IS_ADMIN enforced at WeighingService level) ---
    if (root.containsKey("seal") && root["seal"].as<bool>()) {
      state.sealCommand = true;
      result = StateUpdateResult::CHANGED;
    }

    // --- Unseal command: requires correct factory PIN ---
    if (root.containsKey("unseal") && root["unseal"].as<bool>()) {
      state.pin = root["pin"] | String("");
      state.unsealCommand = true;
      result = StateUpdateResult::CHANGED;
    }

    // --- Config param changes (blocked when sealed) ---
    if (state.sealActive) {
      // If any sealed config key present, reject the whole update
      const char* sealedKeys[] = {
        "unit", "zero_offset", "calibration_factor",
        "max_capacity", "min_division", "decimal_places",
        "zero_tracking_enabled", "zero_tracking_range",
        "power_on_zero_range", "manual_zero_range"
      };
      for (auto key : sealedKeys) {
        if (root.containsKey(key)) return StateUpdateResult::ERROR;
      }
    } else {
      // Apply config patches
      if (root.containsKey("unit")) {
        String v = root["unit"].as<String>();
        if (v == "kg" || v == "g" || v == "lb") {
          if (v != state.unit) { state.unit = v; result = StateUpdateResult::CHANGED; }
        }
      }
      if (root.containsKey("max_capacity")) {
        float v = root["max_capacity"];
        if (v > 0 && v != state.maxCapacity) { state.maxCapacity = v; result = StateUpdateResult::CHANGED; }
      }
      if (root.containsKey("min_division")) {
        float v = root["min_division"];
        if (v > 0 && v != state.minDivision) { state.minDivision = v; result = StateUpdateResult::CHANGED; }
      }
      if (root.containsKey("decimal_places")) {
        uint8_t v = (uint8_t)root["decimal_places"].as<int>();
        if (v <= 6 && v != state.decimalPlaces) { state.decimalPlaces = v; result = StateUpdateResult::CHANGED; }
      }
      if (root.containsKey("zero_tracking_enabled")) {
        bool v = root["zero_tracking_enabled"];
        if (v != state.zeroTrackingEnabled) { state.zeroTrackingEnabled = v; result = StateUpdateResult::CHANGED; }
      }
      if (root.containsKey("zero_tracking_range")) {
        float v = root["zero_tracking_range"];
        if (v > 0 && v != state.zeroTrackingRange) { state.zeroTrackingRange = v; result = StateUpdateResult::CHANGED; }
      }
      if (root.containsKey("power_on_zero_range")) {
        float v = root["power_on_zero_range"];
        if (v > 0 && v != state.powerOnZeroRange) { state.powerOnZeroRange = v; result = StateUpdateResult::CHANGED; }
      }
      if (root.containsKey("manual_zero_range")) {
        float v = root["manual_zero_range"];
        if (v > 0 && v != state.manualZeroRange) { state.manualZeroRange = v; result = StateUpdateResult::CHANGED; }
      }
    }

    // speed_mode and samples_to_average are not legally relevant, always patchable
    if (root.containsKey("speed_mode")) {
      uint8_t v = (uint8_t)root["speed_mode"].as<int>();
      if (v <= 1 && v != state.speedMode) { state.speedMode = v; result = StateUpdateResult::CHANGED; }
    }
    if (root.containsKey("samples_to_average")) {
      uint8_t v = (uint8_t)root["samples_to_average"].as<int>();
      if (v >= 1 && v <= 32 && v != state.samplesToAverage) { state.samplesToAverage = v; result = StateUpdateResult::CHANGED; }
    }

    return result;
  }
};

#endif
