#include "wsoft_view.h"

#include <string.h>

static const int WSOFT_MAX_DECIMALS = 6;

static int64_t pow10_i64(int exponent) {
  int64_t result = 1;
  for (int i = 0; i < exponent; i++) result *= 10;
  return result;
}

bool wsoft_weight_grams(int32_t value, int decimals, int unit, int64_t *grams) {
  if (decimals < 0 || decimals > WSOFT_MAX_DECIMALS) return false;
  int gram_exponent;
  switch (unit) {
    case 1: gram_exponent = 3; break;  // kg
    case 2: gram_exponent = 6; break;  // t
    case 3: gram_exponent = 0; break;  // g
    default: return false;
  }
  int shift = gram_exponent - decimals;
  if (shift >= 0) {
    *grams = (int64_t)value * pow10_i64(shift);
    return true;
  }
  int64_t divisor = pow10_i64(-shift);
  int64_t magnitude = value < 0 ? -(int64_t)value : (int64_t)value;
  int64_t rounded = (magnitude + divisor / 2) / divisor;
  *grams = value < 0 ? -rounded : rounded;
  return true;
}

bool wsoft_has_weight(const WSoftSnapshot &s) {
  return s.valid && s.linked && s.fresh;
}

static void write_weight(const WSoftSnapshot &s, WSoftJsonSink &out) {
  int64_t grams = 0;
  if (wsoft_has_weight(s) && wsoft_weight_grams(s.value, s.decimals, s.unit, &grams)) {
    out.put_int("weight_g", grams);
  } else {
    out.put_null("weight_g");
  }
  if (wsoft_has_weight(s)) {
    char text[24];
    format_weight(s.value, s.decimals, text, sizeof text);
    const char *unit = weight_unit_text(s.unit);
    if (unit[0] != '\0') {
      size_t used = strlen(text);
      if (used + 1 < sizeof text) {
        text[used] = ' ';
        text[used + 1] = '\0';
        strncat(text, unit, sizeof text - strlen(text) - 1);
      }
    }
    out.put_str("weight_text", text);
  } else {
    out.put_null("weight_text");
  }
}

static const char *link_text(const WSoftSnapshot &s) {
  if (!s.valid) return "starting";
  if (!s.linked) return "no_signal";
  if (!s.fresh) return "no_reply";
  return s.alarm ? "alarm" : "live";
}

void wsoft_view_write(const WSoftSnapshot &s, WSoftJsonSink &out) {
  out.put_bool("ready", s.valid);
  out.put_bool("display_ready", s.display_ready);
  out.put_str("link", link_text(s));
  out.put_bool("linked", s.valid && s.linked);
  write_weight(s, out);
  out.put_str("unit", weight_unit_text(s.unit));
  out.put_int("decimals", s.decimals);
  out.put_bool("stable", wsoft_has_weight(s) && s.stable);
  out.put_bool("alarm", s.valid && s.alarm);
  out.put_bool("approval_on", s.approval_on);
  out.put_bool("for_trade", s.trade_check == TRADE_OK);
  out.put_str("trade_check", trade_check_text(s.trade_check));
  out.put_int("audit_counter", s.audit_counter);
  out.put_int("last_trade_seq", s.last_trade_seq);
  out.put_int("trade_queue", s.trade_queue);
  out.put_str("hub", s.hub_text);
  out.put_bool("hub_online", s.hub_online);
  out.put_int("polls", s.polls);
}
