#include "wsoft_hilo.h"

#include <stdio.h>
#include <string.h>

#include "wsoft_json_reader.h"
#include "wsoft_weighing.h"

bool wsoft_hilo_code_ok(const char *code) {
  if (code == nullptr) return false;
  size_t length = strlen(code);
  if (length == 0 || length > WSOFT_HILO_CODE_MAX) return false;
  for (size_t i = 0; i < length; i++) {
    char c = code[i];
    bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

bool wsoft_hilo_request(const char *current, char *out, size_t size) {
  if (out == nullptr || size == 0) return false;
  int n;
  if (current == nullptr || current[0] == 0) {
    n = snprintf(out, size, "{\"current\":null}");
  } else {
    if (!wsoft_hilo_code_ok(current)) return false;
    n = snprintf(out, size, "{\"current\":\"%s\"}", current);
  }
  return n > 0 && (size_t)n < size;
}

// ---------- strict reader for the answer ----------

namespace {

bool read_grams(WsoftJsonReader &r, int32_t *out) {
  int64_t v = 0;
  if (!r.whole(&v, WSOFT_HILO_GRAMS_MAX)) return false;
  *out = (int32_t)v;
  return true;
}

// A code is read one character past the limit, so a long one fails the check.
bool read_code(WsoftJsonReader &r, char *out) {
  char code[WSOFT_HILO_CODE_MAX + 2];
  if (!r.string(code, sizeof code) || !wsoft_hilo_code_ok(code)) return false;
  strcpy(out, code);
  return true;
}

bool read_product(WsoftJsonReader &r, WsoftHiloProduct *p) {
  if (!r.take('{')) return false;
  enum { CODE = 1, NAME = 2, TARGET = 4, LOW = 8, HIGH = 16, ALL = 31 };
  int seen = 0;
  do {
    char key[16];
    if (!r.string(key, sizeof key) || !r.take(':')) return false;
    int bit;
    bool ok;
    if (strcmp(key, "code") == 0) {
      bit = CODE;
      ok = read_code(r, p->code);
    } else if (strcmp(key, "name") == 0) {
      bit = NAME;
      ok = r.string(p->name, sizeof p->name);
    } else if (strcmp(key, "target_g") == 0) {
      bit = TARGET;
      ok = read_grams(r, &p->target_g);
    } else if (strcmp(key, "low_g") == 0) {
      bit = LOW;
      ok = read_grams(r, &p->low_g);
    } else if (strcmp(key, "high_g") == 0) {
      bit = HIGH;
      ok = read_grams(r, &p->high_g);
    } else {
      return false;
    }
    if (!ok || (seen & bit)) return false;
    seen |= bit;
  } while (r.take(','));
  return r.take('}') && seen == ALL && p->low_g <= p->target_g && p->target_g <= p->high_g;
}

bool read_products(WsoftJsonReader &r, WsoftHiloList *out) {
  if (!r.take('[')) return false;
  size_t n = 0;
  if (!r.peek(']')) {
    do {
      if (n == WSOFT_HILO_MAX) return false;
      out->products[n] = WsoftHiloProduct();
      if (!read_product(r, &out->products[n])) return false;
      n++;
    } while (r.take(','));
  }
  out->count = n;
  return r.take(']');
}

bool read_default(WsoftJsonReader &r, WsoftHiloList *out) {
  if (r.word("null")) {
    out->has_default = false;
    return true;
  }
  if (!r.take('{')) return false;
  enum { CODE = 1, SET_AT = 2, ALL = 3 };
  int seen = 0;
  do {
    char key[16];
    if (!r.string(key, sizeof key) || !r.take(':')) return false;
    int bit;
    bool ok;
    if (strcmp(key, "code") == 0) {
      bit = CODE;
      ok = read_code(r, out->default_code);
    } else if (strcmp(key, "set_at") == 0) {
      bit = SET_AT;
      char set_at[WSOFT_HILO_SET_AT_MAX + 2];
      ok = r.string(set_at, sizeof set_at) && set_at[0] != 0 && strlen(set_at) <= WSOFT_HILO_SET_AT_MAX;
      if (ok) strcpy(out->default_set_at, set_at);
    } else {
      return false;
    }
    if (!ok || (seen & bit)) return false;
    seen |= bit;
  } while (r.take(','));
  out->has_default = true;
  return r.take('}') && seen == ALL;
}

}  // namespace

bool wsoft_hilo_parse(const char *body, size_t length, WsoftHiloList *out) {
  if (body == nullptr || out == nullptr) return false;
  *out = WsoftHiloList();
  WsoftJsonReader r = {body, body + length};
  enum { PRODUCTS = 1, DEFAULT = 2, ALL = 3 };
  int seen = 0;
  bool ok = r.take('{');
  while (ok) {
    char key[16];
    if (!r.string(key, sizeof key) || !r.take(':')) {
      ok = false;
      break;
    }
    int bit;
    if (strcmp(key, "products") == 0) {
      bit = PRODUCTS;
      ok = read_products(r, out);
    } else if (strcmp(key, "default") == 0) {
      bit = DEFAULT;
      ok = read_default(r, out);
    } else {
      ok = false;
    }
    if (!ok || (seen & bit)) {
      ok = false;
      break;
    }
    seen |= bit;
    if (!r.take(',')) break;
  }
  ok = ok && r.take('}') && r.done() && seen == ALL;
  if (!ok) *out = WsoftHiloList();
  return ok;
}

const WsoftHiloProduct *wsoft_hilo_find(const WsoftHiloList &list, const char *code) {
  if (code == nullptr || code[0] == 0) return nullptr;
  for (size_t i = 0; i < list.count; i++) {
    if (strcmp(list.products[i].code, code) == 0) return &list.products[i];
  }
  return nullptr;
}

bool wsoft_hilo_apply(const WsoftHiloList &list, WsoftHiloPick *pick) {
  bool changed = false;
  if (list.has_default && strcmp(list.default_set_at, pick->applied_set_at) != 0 &&
      wsoft_hilo_find(list, list.default_code) != nullptr) {
    strcpy(pick->current, list.default_code);
    strcpy(pick->applied_set_at, list.default_set_at);
    changed = true;
  }
  if (pick->current[0] != 0 && wsoft_hilo_find(list, pick->current) == nullptr) {
    pick->current[0] = 0;
    changed = true;
  }
  return changed;
}

const char *wsoft_hilo_next(const WsoftHiloList &list, const char *current) {
  if (list.count == 0) return "";
  if (current == nullptr || current[0] == 0) return list.products[0].code;
  for (size_t i = 0; i < list.count; i++) {
    if (strcmp(list.products[i].code, current) == 0) return i + 1 < list.count ? list.products[i + 1].code : "";
  }
  return list.products[0].code;
}

WsoftHiloState wsoft_hilo_check(const WsoftHiloProduct &product, int32_t value, int decimals, int unit) {
  if (decimals < 0 || decimals > 9) return WSOFT_HILO_NONE;
  // Weight in grams = value * per_unit / 10^decimals. Compare value * per_unit with
  // limit * 10^decimals so nothing is rounded.
  int64_t per_unit;
  if (unit == 1) per_unit = 1000;
  else if (unit == 2) per_unit = 1000000;
  else if (unit == 3) per_unit = 1;
  else return WSOFT_HILO_NONE;
  int64_t scale = 1;
  for (int i = 0; i < decimals; i++) scale *= 10;
  // |value| <= 2^31 and per_unit <= 10^6 stay inside int64; limits <= 10^9 times 10^9 too.
  int64_t weight = (int64_t)value * per_unit;
  if (weight < (int64_t)product.low_g * scale) return WSOFT_HILO_LOW;
  if (weight > (int64_t)product.high_g * scale) return WSOFT_HILO_HIGH;
  return WSOFT_HILO_PASS;
}

const char *wsoft_hilo_state_wire(WsoftHiloState state) {
  switch (state) {
    case WSOFT_HILO_LOW: return "LOW";
    case WSOFT_HILO_PASS: return "PASS";
    case WSOFT_HILO_HIGH: return "HIGH";
    default: return "";
  }
}

const char *wsoft_hilo_state_words(WsoftHiloState state) {
  switch (state) {
    case WSOFT_HILO_LOW: return "TOO LOW";
    case WSOFT_HILO_PASS: return "OK";
    case WSOFT_HILO_HIGH: return "TOO HIGH";
    default: return "";
  }
}

bool wsoft_hilo_json(const WsoftHiloStamp &stamp, const char *reading_kg, char *out, size_t size) {
  if (out == nullptr || size == 0 || reading_kg == nullptr || reading_kg[0] == 0) return false;
  if (!stamp.on || stamp.state == WSOFT_HILO_NONE || !wsoft_hilo_code_ok(stamp.code)) return false;
  char target[24], low[24], high[24];
  // Grams as kg with up to 3 decimals: "50", "49.9", "0.1".
  if (!wsoft_weight_kg_text(stamp.target_g, 3, 1, target, sizeof target) ||
      !wsoft_weight_kg_text(stamp.low_g, 3, 1, low, sizeof low) ||
      !wsoft_weight_kg_text(stamp.high_g, 3, 1, high, sizeof high)) {
    return false;
  }
  int n = snprintf(out, size,
                   ",\"hi_lo\":{\"state\":\"%s\",\"reading_kg\":%s,\"target_kg\":%s,\"low_limit_kg\":%s,"
                   "\"high_limit_kg\":%s,\"deadband_kg\":0,\"tolerance\":{\"source\":\"hub-hilo-v1\"},"
                   "\"auto_saved\":%s,\"product_code\":\"%s\"}",
                   wsoft_hilo_state_wire(stamp.state), reading_kg, target, low, high,
                   stamp.auto_saved ? "true" : "false", stamp.code);
  return n > 0 && (size_t)n < size;
}
