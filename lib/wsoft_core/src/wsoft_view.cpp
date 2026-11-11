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

// notice_code / notice_title / notice_detail / notice_level ("stop", "wait", "info"), all
// null when nothing stops a save.
static void write_notice(const WSoftSnapshot &s, WSoftJsonSink &out) {
  WsoftSaveNotice notice;
  if (!s.valid || !wsoft_save_notice(s.save_waiting, &notice)) {
    out.put_null("notice_code");
    out.put_null("notice_title");
    out.put_null("notice_detail");
    out.put_null("notice_level");
    return;
  }
  out.put_str("notice_code", notice.code);
  out.put_str("notice_title", notice.title);
  out.put_str("notice_detail", notice.detail);
  out.put_str("notice_level", notice.level == WSOFT_NOTICE_STOP   ? "stop"
                              : notice.level == WSOFT_NOTICE_WAIT ? "wait"
                                                                  : "info");
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
  out.put_str("source", s.rs232 ? "rs232" : "rs485");
  write_notice(s, out);
}

bool wsoft_parse_weight_text(const char *text, int32_t *value, int *decimals) {
  if (text == nullptr) return false;
  const char *p = text;
  while (*p == ' ') p++;
  bool negative = false;
  if (*p == '+' || *p == '-') negative = *p++ == '-';
  while (*p == ' ') p++;  // W113 pads between sign and digits: "-   2820"
  int64_t magnitude = 0;
  int digits = 0, places = -1;
  for (; *p; p++) {
    if (*p >= '0' && *p <= '9') {
      if (places >= 0 && ++places > 4) return false;
      magnitude = magnitude * 10 + (*p - '0');
      if (magnitude > INT32_MAX) return false;
      digits++;
    } else if ((*p == '.' || *p == ',') && places < 0) {
      places = 0;
    } else {
      break;
    }
  }
  while (*p == ' ') p++;
  if (*p != '\0' || digits == 0) return false;
  *value = (int32_t)(negative ? -magnitude : magnitude);
  *decimals = places < 0 ? 0 : places;
  return true;
}
