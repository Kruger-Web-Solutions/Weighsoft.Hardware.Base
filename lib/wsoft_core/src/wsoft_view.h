#pragma once

// Live W-Soft view for the web page, WebSocket and MQTT. Pure C++ (no Arduino or
// framework headers) so host tests can pin the JSON field by field.
//
// The screen task fills a WSoftSnapshot after every scale poll; the services copy it
// and turn it into JSON through wsoft_view_write(). Rules that must not drift:
// - weight_g is whole grams, or null when the scale is not linked or the last poll
//   failed. A failed poll is never a weight.
// - approval is read-only here. Nothing in this view can switch it.

#include <stddef.h>
#include <stdint.h>

#include "trade_logic.h"

static const size_t WSOFT_HUB_TEXT_SIZE = 20;

struct WSoftSnapshot {
  bool valid = false;          // false until the screen task has published once
  bool display_ready = false;  // the RGB panel and frame buffers came up
  bool linked = false;         // DJB answered recently (fewer than 3 failed polls in a row)
  bool fresh = false;          // the most recent poll answered
  bool alarm = false;          // DJB blocking alarm bits set
  int32_t value = 0;           // raw DJB value, scaled by decimals
  int decimals = 3;
  int unit = 1;                // DJB unit code: 0 none, 1 kg, 2 t, 3 g
  bool approval_on = false;
  bool stable = false;
  TradeCheck trade_check = TRADE_OFF;
  uint32_t audit_counter = 0;
  uint32_t last_trade_seq = 0;
  uint32_t trade_queue = 0;    // trade weights waiting for the Hub
  bool hub_online = false;
  char hub_text[WSOFT_HUB_TEXT_SIZE] = "HUB SETUP";
  uint32_t polls = 0;
  uint32_t updated_ms = 0;     // millis() of the poll this snapshot came from
};

// Where the view writes its fields. The ESP service maps this onto an ArduinoJson
// object; the host tests build a JSON string.
class WSoftJsonSink {
 public:
  virtual ~WSoftJsonSink() {}
  virtual void put_null(const char *key) = 0;
  virtual void put_bool(const char *key, bool value) = 0;
  virtual void put_int(const char *key, int64_t value) = 0;
  virtual void put_str(const char *key, const char *value) = 0;
};

// Converts a DJB reading to whole grams. Returns false when the unit has no gram
// meaning (code 0 or unknown) or the decimals are out of range. Sub-gram readings
// round half away from zero.
bool wsoft_weight_grams(int32_t value, int decimals, int unit, int64_t *grams);

// True when the snapshot carries a weight that may be shown as a number.
bool wsoft_has_weight(const WSoftSnapshot &s);

void wsoft_view_write(const WSoftSnapshot &s, WSoftJsonSink &out);
