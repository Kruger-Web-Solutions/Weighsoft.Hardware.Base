#ifndef Features_h
#define Features_h

#define FT_ENABLED(feature) feature

// project feature off by default
#ifndef FT_PROJECT
#define FT_PROJECT 0
#endif

// security feature on by default
#ifndef FT_SECURITY
#define FT_SECURITY 1
#endif

// mqtt feature on by default
#ifndef FT_MQTT
#define FT_MQTT 1
#endif

// ntp feature on by default
#ifndef FT_NTP
#define FT_NTP 1
#endif

// mqtt feature on by default
#ifndef FT_OTA
#define FT_OTA 1
#endif

// upload firmware feature off by default
#ifndef FT_UPLOAD_FIRMWARE
#define FT_UPLOAD_FIRMWARE 0
#endif

// BLE feature ON by default for ESP32 IF there's enough flash
// Note: BLE adds ~600KB. On smaller partitions, disable with -D FT_BLE=0
#ifndef FT_BLE
#ifdef ESP32
#define FT_BLE 0  // Disabled by default due to flash constraints
#else
#define FT_BLE 0  // Always disabled on ESP8266
#endif
#endif

// --- Per-board display features (NewEspAllInOne) ---
// All OFF by default; enabled per-board in that env's build_flags in platformio.ini.

// ESP-to-ESP weight receiver (hard dependency of the TFT weight screen)
#ifndef FT_REMOTE_WEIGHT
#define FT_REMOTE_WEIGHT 0
#endif

// TFT weight screen (TFT_eSPI; ILI9488 / CYD ILI9341). Requires FT_REMOTE_WEIGHT.
#ifndef FT_TFT_WEIGHT_SCREEN
#define FT_TFT_WEIGHT_SCREEN 0
#endif

// Character-LCD device (16x2 I2C via LiquidCrystal_I2C)
#ifndef FT_DISPLAY_LCD
#define FT_DISPLAY_LCD 0
#endif


#endif
