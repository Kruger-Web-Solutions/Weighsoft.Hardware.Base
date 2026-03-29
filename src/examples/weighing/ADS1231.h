#ifndef ADS1231_h
#define ADS1231_h

#include <Arduino.h>

// ADS1231 24-bit ADC driver (bit-banged GPIO)
// PCB connections: DOUT=GPIO25, SCLK=GPIO23, PDWN=GPIO22, SPEED=GPIO19
#define ADS1231_DOUT_PIN  25
#define ADS1231_SCLK_PIN  23
#define ADS1231_PDWN_PIN  22
#define ADS1231_SPEED_PIN 19

// Timeout waiting for DOUT LOW (data ready): 500ms
#define ADS1231_READY_TIMEOUT_MS 500

class ADS1231 {
 public:
  ADS1231(uint8_t doutPin  = ADS1231_DOUT_PIN,
          uint8_t sclkPin  = ADS1231_SCLK_PIN,
          uint8_t pdwnPin  = ADS1231_PDWN_PIN,
          uint8_t speedPin = ADS1231_SPEED_PIN);

  // Initialize GPIO pins; call once in setup/begin
  void begin();

  // Returns true when DOUT is LOW (new conversion result ready)
  bool isReady();

  // Block until ready (with timeout) then clock out 24-bit signed value.
  // Returns INT32_MIN on timeout.
  int32_t readRaw();

  // Take 'samples' readings and return the rounded average.
  // Invalid on ADC disconnect (returns INT32_MIN).
  int32_t readRawAverage(uint8_t samples);

  // Power management -- PDWN HIGH = normal, PDWN LOW = power-down
  void powerDown();
  void powerUp();

  // Speed: false = 10 SPS (low-noise), true = 80 SPS (fast)
  void setSpeed(bool fast);

  // Returns false if DOUT never went LOW within ADS1231_READY_TIMEOUT_MS
  bool isConnected();

 private:
  uint8_t _doutPin;
  uint8_t _sclkPin;
  uint8_t _pdwnPin;
  uint8_t _speedPin;

  // Clock out one bit (SCLK HIGH then LOW, read DOUT)
  bool _clockBit();
};

#endif
