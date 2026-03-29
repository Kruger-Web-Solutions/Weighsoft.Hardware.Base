#include <examples/weighing/ADS1231.h>

ADS1231::ADS1231(uint8_t doutPin, uint8_t sclkPin, uint8_t pdwnPin, uint8_t speedPin)
    : _doutPin(doutPin), _sclkPin(sclkPin), _pdwnPin(pdwnPin), _speedPin(speedPin) {
}

void ADS1231::begin() {
  pinMode(_doutPin, INPUT);
  pinMode(_sclkPin, OUTPUT);
  pinMode(_pdwnPin, OUTPUT);
  pinMode(_speedPin, OUTPUT);

  digitalWrite(_sclkPin, LOW);
  digitalWrite(_pdwnPin, HIGH);   // Power on
  digitalWrite(_speedPin, LOW);   // 10 SPS default (low noise)

  // ADS1231 needs ~400ms after power-up before first conversion
  delay(400);
}

bool ADS1231::isReady() {
  return digitalRead(_doutPin) == LOW;
}

bool ADS1231::isConnected() {
  unsigned long start = millis();
  while (millis() - start < ADS1231_READY_TIMEOUT_MS) {
    if (isReady()) {
      return true;
    }
    delay(1);
  }
  return false;
}

bool ADS1231::_clockBit() {
  digitalWrite(_sclkPin, HIGH);
  delayMicroseconds(1);
  bool bit = digitalRead(_doutPin);
  digitalWrite(_sclkPin, LOW);
  delayMicroseconds(1);
  return bit;
}

int32_t ADS1231::readRaw() {
  // Wait for DOUT LOW (data ready)
  unsigned long start = millis();
  while (digitalRead(_doutPin) == HIGH) {
    if (millis() - start > ADS1231_READY_TIMEOUT_MS) {
      return INT32_MIN;  // Timeout -- ADC not responding
    }
    delayMicroseconds(10);
  }

  // Clock out 24 bits MSB first
  int32_t raw = 0;
  for (int i = 23; i >= 0; i--) {
    if (_clockBit()) {
      raw |= (1L << i);
    }
  }

  // 25th clock pulse -- transitions DOUT HIGH (end of conversion)
  // Also selects the next output (channel A, gain 128 for ADS1231)
  _clockBit();

  // Sign-extend 24-bit two's complement to 32-bit
  if (raw & 0x800000) {
    raw |= 0xFF000000;
  }

  return raw;
}

int32_t ADS1231::readRawAverage(uint8_t samples) {
  if (samples == 0) samples = 1;
  int64_t sum = 0;
  uint8_t valid = 0;

  for (uint8_t i = 0; i < samples; i++) {
    int32_t val = readRaw();
    if (val == INT32_MIN) {
      return INT32_MIN;  // ADC disconnect propagates
    }
    sum += val;
    valid++;
  }

  return (valid > 0) ? (int32_t)(sum / valid) : INT32_MIN;
}

void ADS1231::powerDown() {
  digitalWrite(_pdwnPin, LOW);
}

void ADS1231::powerUp() {
  digitalWrite(_pdwnPin, HIGH);
  delay(400);  // Settling time after power-up
}

void ADS1231::setSpeed(bool fast) {
  // SPEED LOW = 10 SPS, SPEED HIGH = 80 SPS
  digitalWrite(_speedPin, fast ? HIGH : LOW);
  // Allow one conversion cycle to settle
  delay(fast ? 15 : 110);
}
