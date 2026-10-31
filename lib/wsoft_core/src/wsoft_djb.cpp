#include "wsoft_djb.h"

#include <stdio.h>
#include <string.h>

static const char *const FIELD_NAMES[] = {
    "ad",
    "weight",
    "alarm",
    "unused",
    "unit",
    "decimal",
    "division",
    "stability_range",
    "stability_time",
    "zero_track_limit",
    "zero_track_time",
    "zero_display",
    "sampling",
    "filter1",
    "filter2",
    "address",
    "baud",
    "byte_order",
    "com1_mode",
};

// 5LED parameter rows, then the protocol-table COM rows. Same order as the enum.
static const uint16_t PARAM_DOC[] = {
    41301, 41303, 41305, 41307, 41309, 41311, 41313, 41315, 41317, 41319, 41321, 41323,
    41203, 41205, 41207, 41209,
};

// An AD write is a calibration: 0 is zero calibration, CW > 0 is load calibration.
static const int32_t AD_MIN = 0;
static const int32_t AD_MAX = 1000000;
// The screen's link scan only searches slaves 1..10, so a higher address would lose the scale.
static const int32_t ADDRESS_MAX = 10;

struct Limit {
  WsoftDjbField field;
  int32_t min;
  int32_t max;
};

// Writes that would cut the link or that the live poll cannot read are refused.
// byte_order stays 4321 and com1_mode stays Modbus RTU because the poll needs both.
static const Limit LIMITS[] = {
    {WSOFT_DJB_AD, AD_MIN, AD_MAX},
    {WSOFT_DJB_UNIT, 0, 3},
    {WSOFT_DJB_DECIMAL, 0, 4},
    {WSOFT_DJB_ADDRESS, 1, ADDRESS_MAX},
    {WSOFT_DJB_BAUD, 0, 2},
    {WSOFT_DJB_BYTE_ORDER, 0, 0},
    {WSOFT_DJB_COM1_MODE, 1, 1},
};

uint16_t wsoft_djb_crc16(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}

void wsoft_djb_hex(const uint8_t *data, size_t len, char *out, size_t cap) {
  if (cap == 0) return;
  out[0] = 0;
  size_t pos = 0;
  for (size_t i = 0; i < len; i++) {
    int wrote = snprintf(out + pos, cap - pos, i ? " %02X" : "%02X", data[i]);
    if (wrote < 0 || (size_t)wrote >= cap - pos) {
      out[cap - 1] = 0;
      return;
    }
    pos += (size_t)wrote;
  }
}

bool wsoft_djb_doc(WsoftDjbField field, int channel, uint16_t *doc) {
  if (field < 0 || field >= WSOFT_DJB_FIELD_COUNT || doc == nullptr) return false;
  if (field <= WSOFT_DJB_ALARM) {
    if (channel < 1 || channel > 16) return false;
    uint16_t base = (channel <= 8) ? (uint16_t)(41003 + (channel - 1) * 10)
                                   : (uint16_t)(41503 + (channel - 9) * 10);
    *doc = (uint16_t)(base + (uint16_t)field * 2);
    return true;
  }
  *doc = PARAM_DOC[field - WSOFT_DJB_UNUSED];
  return true;
}

bool wsoft_djb_writable(WsoftDjbField field) {
  return field != WSOFT_DJB_WEIGHT && field != WSOFT_DJB_ALARM && field >= 0 && field < WSOFT_DJB_FIELD_COUNT;
}

static void fail(WsoftDjbPlan *plan, const char *error) {
  plan->ok = false;
  snprintf(plan->error, sizeof plan->error, "%s", error);
}

bool wsoft_djb_plan(const char *op,
                    const char *field,
                    int channel,
                    bool has_channel,
                    int32_t value,
                    bool has_value,
                    WsoftDjbPlan *plan) {
  if (plan == nullptr) return false;
  memset(plan, 0, sizeof *plan);
  if (op == nullptr || (strcmp(op, "read") != 0 && strcmp(op, "write") != 0)) {
    fail(plan, "bad_op");
    return false;
  }
  plan->write = strcmp(op, "write") == 0;
  if (field == nullptr) {
    fail(plan, "bad_field");
    return false;
  }
  int found = -1;
  for (int i = 0; i < WSOFT_DJB_FIELD_COUNT; i++) {
    if (strcmp(field, FIELD_NAMES[i]) == 0) found = i;
  }
  if (found < 0) {
    fail(plan, "bad_field");
    return false;
  }
  plan->field = (WsoftDjbField)found;
  plan->alarm = plan->field == WSOFT_DJB_ALARM;
  bool channel_field = plan->field <= WSOFT_DJB_ALARM;
  if (channel_field) {
    if (!has_channel || !wsoft_djb_doc(plan->field, channel, &plan->doc)) {
      fail(plan, "bad_channel");
      return false;
    }
    plan->channel = channel;
  } else if (!wsoft_djb_doc(plan->field, 0, &plan->doc)) {
    fail(plan, "bad_field");
    return false;
  }
  if (plan->write && !wsoft_djb_writable(plan->field)) {
    fail(plan, "read_only");
    return false;
  }
  if (plan->write && !has_value) {
    fail(plan, "need_value");
    return false;
  }
  if (plan->write) {
    for (size_t i = 0; i < sizeof LIMITS / sizeof LIMITS[0]; i++) {
      if (LIMITS[i].field == plan->field && (value < LIMITS[i].min || value > LIMITS[i].max)) {
        fail(plan, plan->field == WSOFT_DJB_AD ? "ad_range" : "range");
        return false;
      }
    }
  }
  plan->value = plan->write ? value : 0;
  plan->pdu = (uint16_t)(plan->doc - 40001);
  plan->command = plan->write ? 0x10 : 0x03;
  plan->ok = true;
  return true;
}

size_t wsoft_djb_build(uint8_t slave, const WsoftDjbPlan *plan, uint8_t *out, size_t cap) {
  if (plan == nullptr || !plan->ok || out == nullptr) return 0;
  if (!plan->write) {
    if (cap < 8) return 0;
    out[0] = slave;
    out[1] = 0x03;
    out[2] = (uint8_t)(plan->pdu >> 8);
    out[3] = (uint8_t)plan->pdu;
    out[4] = 0;
    out[5] = 2;
    uint16_t crc = wsoft_djb_crc16(out, 6);
    out[6] = (uint8_t)crc;
    out[7] = (uint8_t)(crc >> 8);
    return 8;
  }
  if (cap < 13) return 0;
  uint32_t raw = (uint32_t)plan->value;
  out[0] = slave;
  out[1] = 0x10;
  out[2] = (uint8_t)(plan->pdu >> 8);
  out[3] = (uint8_t)plan->pdu;
  out[4] = 0;
  out[5] = 2;
  out[6] = 4;
  out[7] = (uint8_t)(raw >> 24);
  out[8] = (uint8_t)(raw >> 16);
  out[9] = (uint8_t)(raw >> 8);
  out[10] = (uint8_t)raw;
  uint16_t crc = wsoft_djb_crc16(out, 11);
  out[11] = (uint8_t)crc;
  out[12] = (uint8_t)(crc >> 8);
  return 13;
}

int wsoft_djb_parse(const uint8_t *reply,
                    size_t len,
                    uint8_t slave,
                    uint8_t command,
                    uint16_t pdu,
                    int32_t *value,
                    uint8_t *exception_code) {
  if (reply == nullptr || len < 5) return 1;
  if ((reply[1] & 0x80) != 0) {
    if (len < 5) return 1;
    if (wsoft_djb_crc16(reply, 3) != (uint16_t)(reply[3] | (reply[4] << 8))) return 2;
    if (reply[0] != slave || reply[1] != (uint8_t)(command | 0x80)) return 4;
    if (exception_code) *exception_code = reply[2];
    return 3;
  }
  size_t expected = command == 0x10 ? 8 : 9;
  if (len < expected) return 1;
  if (len != expected) return 4;
  if (wsoft_djb_crc16(reply, expected - 2) != (uint16_t)(reply[expected - 2] | (reply[expected - 1] << 8))) return 2;
  if (reply[0] != slave || reply[1] != command) return 4;
  if (command == 0x10) {
    uint16_t got_pdu = (uint16_t)((reply[2] << 8) | reply[3]);
    uint16_t qty = (uint16_t)((reply[4] << 8) | reply[5]);
    if (got_pdu != pdu || qty != 2) return 4;
    return 0;
  }
  if (reply[2] != 4) return 4;
  if (value) {
    uint32_t raw = ((uint32_t)reply[3] << 24) | ((uint32_t)reply[4] << 16) | ((uint32_t)reply[5] << 8) | reply[6];
    *value = (int32_t)raw;
  }
  return 0;
}

void wsoft_djb_alarm_bits(int32_t raw, WsoftDjbAlarmBits *bits) {
  if (bits == nullptr) return;
  uint32_t u = (uint32_t)raw;
  bits->parameter_changed = (u & 0x00010000u) != 0;
  bits->software_match = (u & 0x00020000u) != 0;
  bits->adc_failure = (u & 0x20000000u) != 0;
  bits->over_range = (u & 0x40000000u) != 0;
  bits->eeprom = (u & 0x80000000u) != 0;
}
