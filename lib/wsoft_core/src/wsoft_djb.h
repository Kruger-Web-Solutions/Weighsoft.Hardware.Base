#pragma once

// DJB-M1X / DJB-W/M[1-16] holding registers. Pure C++ so host tests can pin frames.
//
// Doc addresses are the 41xxx map (PDU = doc - 40001). Signed long, two registers,
// byte order 4321 (high word first). Command 03 reads. Command 10 writes.
// Weight and alarm are read-only. Keypad names and sensitivity/capacity have no
// address in the manual, so they are not in this map.
//
// Alarm bits are numbered on the high 16-bit word (bit 0 of that word is 0x00010000).

#include <stddef.h>
#include <stdint.h>

enum WsoftDjbField {
  WSOFT_DJB_AD = 0,
  WSOFT_DJB_WEIGHT,
  WSOFT_DJB_ALARM,
  WSOFT_DJB_UNUSED,
  WSOFT_DJB_UNIT,
  WSOFT_DJB_DECIMAL,
  WSOFT_DJB_DIVISION,
  WSOFT_DJB_STABILITY_RANGE,
  WSOFT_DJB_STABILITY_TIME,
  WSOFT_DJB_ZERO_TRACK_LIMIT,
  WSOFT_DJB_ZERO_TRACK_TIME,
  WSOFT_DJB_ZERO_DISPLAY,
  WSOFT_DJB_SAMPLING,
  WSOFT_DJB_FILTER1,
  WSOFT_DJB_FILTER2,
  WSOFT_DJB_ADDRESS,
  WSOFT_DJB_BAUD,
  WSOFT_DJB_BYTE_ORDER,
  WSOFT_DJB_COM1_MODE,
  WSOFT_DJB_FIELD_COUNT
};

struct WsoftDjbAlarmBits {
  bool parameter_changed;  // high-word bit 0
  bool software_match;     // high-word bit 1
  bool adc_failure;        // high-word bit 13
  bool over_range;         // high-word bit 14
  bool eeprom;             // high-word bit 15
};

struct WsoftDjbPlan {
  bool ok;
  char error[24];
  bool write;
  WsoftDjbField field;
  int channel;
  int32_t value;
  uint16_t doc;
  uint16_t pdu;
  uint8_t command;
  bool alarm;
};

// Modbus CRC-16 (init 0xFFFF, poly 0xA001, reflected). The frame stores it low byte first.
uint16_t wsoft_djb_crc16(const uint8_t *data, size_t length);

// "AA BB" uppercase hex. Empty when len is 0.
void wsoft_djb_hex(const uint8_t *data, size_t len, char *out, size_t cap);

// Doc address for a field. Channel registers need channel 1..16. Returns false when
// the channel is out of range.
bool wsoft_djb_doc(WsoftDjbField field, int channel, uint16_t *doc);

bool wsoft_djb_writable(WsoftDjbField field);

// Fills a plan from the HTTP names. op is "read" or "write". Channel fields need
// channel. Writes need value. An AD write is a calibration (0 zero, CW > 0 load) and
// must be in 0..1000000 ("ad_range"). unit 0..3, decimal 0..4, address 1..10 (the
// link scan range), baud 0..2, byte_order 0 and com1_mode 1, else "range". Sampling
// and the filters stay raw signed longs (the manuals disagree on the codes).
bool wsoft_djb_plan(const char *op,
                    const char *field,
                    int channel,
                    bool has_channel,
                    int32_t value,
                    bool has_value,
                    WsoftDjbPlan *plan);

// Read frame is 8 bytes. Write frame is 13 bytes (quantity 2, byte count 4, 4321).
// Returns 0 when the buffer is too small.
size_t wsoft_djb_build(uint8_t slave, const WsoftDjbPlan *plan, uint8_t *out, size_t cap);

// 0 ok, 1 short, 2 crc, 3 exception, 4 mismatch.
// On ok, *value is the signed long (writes leave the caller's value unchanged).
// On exception, *exception_code is the Modbus exception byte.
int wsoft_djb_parse(const uint8_t *reply,
                    size_t len,
                    uint8_t slave,
                    uint8_t command,
                    uint16_t pdu,
                    int32_t *value,
                    uint8_t *exception_code);

void wsoft_djb_alarm_bits(int32_t raw, WsoftDjbAlarmBits *bits);
