#include "wsoft_view.h"

#include <stdio.h>
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

// The number of value (in the snapshot's decimals). False when there is no live weight.
static bool weight_number(const WSoftSnapshot &s, int32_t value, char *weight, size_t size) {
  if (weight == nullptr || size == 0) return false;
  weight[0] = '\0';
  if (!wsoft_has_weight(s)) return false;
  format_weight(value, s.decimals, weight, size);
  return true;
}

// The feed sends the net when a tare is on.
static int32_t feed_value(const WSoftSnapshot &s) { return s.tared ? s.net_value : s.value; }

bool wsoft_feed_weight(const WSoftSnapshot &s, char *weight, size_t size) {
  return weight_number(s, feed_value(s), weight, size);
}

// "12.345 kg": the number of value and the unit. False when there is no live weight.
static bool weight_with_unit(const WSoftSnapshot &s, int32_t value, char *text, size_t size) {
  if (!weight_number(s, value, text, size)) return false;
  const char *unit = weight_unit_text(s.unit);
  size_t used = strlen(text);
  if (unit[0] != '\0' && used + 1 < size) {
    text[used] = ' ';
    text[used + 1] = '\0';
    strncat(text, unit, size - used - 2);
  }
  return true;
}

static void write_weight(const WSoftSnapshot &s, WSoftJsonSink &out) {
  int64_t grams = 0;
  if (wsoft_has_weight(s) && wsoft_weight_grams(s.value, s.decimals, s.unit, &grams)) {
    out.put_int("weight_g", grams);
  } else {
    out.put_null("weight_g");
  }
  char text[24];
  if (weight_with_unit(s, s.value, text, sizeof text)) {
    out.put_str("weight_text", text);
  } else {
    out.put_null("weight_text");
  }
}

void wsoft_feed_write(const WSoftSnapshot &s, WSoftJsonSink &out) {
  char line[24];
  char weight[24];
  bool live = weight_with_unit(s, feed_value(s), line, sizeof line) && wsoft_feed_weight(s, weight, sizeof weight);
  out.put_str("last_line", live ? line : "");
  if (live) {
    out.put_str("weight", weight);
  } else {
    out.put_null("weight");
  }
  out.put_str("unit", weight_unit_text(s.unit));
  out.put_bool("stable", live && s.stable);
  out.put_int("timestamp", s.updated_ms);
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
  out.put_bool("alarm_allowed", s.valid && s.alarm_allowed);
  if (s.valid && s.alarm_allowed) {
    out.put_str("alarm_note", "EEPROM alarm allowed (supplier: factory flag), monitoring");
  } else {
    out.put_null("alarm_note");
  }
  if (s.valid && s.alarm_word_valid) {
    char hex[12];
    snprintf(hex, sizeof hex, "%08lX", (unsigned long)s.alarm_word);
    out.put_str("alarm_word_hex", hex);
  } else {
    out.put_null("alarm_word_hex");
  }
  out.put_int("alarm_changes", s.alarm_changes);
  out.put_bool("alarm_test", s.alarm_test);
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
  out.put_str("role", s.writer ? "writer" : "weigh");
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
