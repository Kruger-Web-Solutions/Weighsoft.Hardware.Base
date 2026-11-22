#include "wsoft_products.h"

#include <string.h>

const char *wsoft_product_result_code(WsoftProductResult result) {
  switch (result) {
    case WSOFT_PRODUCT_OK: return "OK";
    case WSOFT_PRODUCT_BAD_CODE: return "BAD_CODE";
    case WSOFT_PRODUCT_BAD_NAME: return "BAD_NAME";
    case WSOFT_PRODUCT_BAD_LIMITS: return "BAD_LIMITS";
    case WSOFT_PRODUCT_HUB_CODE: return "HUB_CODE";
    case WSOFT_PRODUCT_DUPLICATE: return "DUPLICATE";
    case WSOFT_PRODUCT_FULL: return "FULL";
    case WSOFT_PRODUCT_NOT_FOUND: return "NOT_FOUND";
  }
  return "ERROR";
}

const char *wsoft_product_result_text(WsoftProductResult result) {
  switch (result) {
    case WSOFT_PRODUCT_OK: return "Saved.";
    case WSOFT_PRODUCT_BAD_CODE: return "Code: 1 to 32 letters, numbers, - or _. No spaces.";
    case WSOFT_PRODUCT_BAD_NAME: return "Name: 1 to 24 plain letters, numbers or signs.";
    case WSOFT_PRODUCT_BAD_LIMITS: return "Low must be below High, and Target between them.";
    case WSOFT_PRODUCT_HUB_CODE: return "That code belongs to the Hub. Pick another code.";
    case WSOFT_PRODUCT_DUPLICATE: return "This board already has a product with that code.";
    case WSOFT_PRODUCT_FULL: return "This board holds 20 products. Delete one first.";
    case WSOFT_PRODUCT_NOT_FOUND: return "That product is not on this board any more.";
  }
  return "Something went wrong.";
}

static bool name_ok(const char *name) {
  size_t n = strnlen(name, WSOFT_HILO_NAME_MAX + 1);
  if (n == 0 || n > WSOFT_HILO_NAME_MAX) return false;
  bool any = false;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)name[i];
    if (c < 0x20 || c > 0x7e) return false;  // the screen font is plain ASCII
    if (c != ' ') any = true;
  }
  return any;
}

WsoftProductResult wsoft_product_check(const WsoftHiloProduct &p) {
  if (strnlen(p.code, sizeof p.code) >= sizeof p.code || !wsoft_hilo_code_ok(p.code)) return WSOFT_PRODUCT_BAD_CODE;
  if (strnlen(p.name, sizeof p.name) >= sizeof p.name || !name_ok(p.name)) return WSOFT_PRODUCT_BAD_NAME;
  if (p.low_g <= 0 || p.low_g >= p.high_g || p.target_g < p.low_g || p.target_g > p.high_g ||
      p.high_g > WSOFT_HILO_GRAMS_MAX) {
    return WSOFT_PRODUCT_BAD_LIMITS;
  }
  return WSOFT_PRODUCT_OK;
}

static int local_index(const WsoftLocalProducts &local, const char *code) {
  if (code == nullptr || code[0] == 0) return -1;
  for (size_t i = 0; i < local.count; i++) {
    if (strcmp(local.products[i].code, code) == 0) return (int)i;
  }
  return -1;
}

WsoftProductResult wsoft_products_put(WsoftLocalProducts *local, const WsoftHiloList &hub, const char *old_code,
                                      const WsoftHiloProduct &product) {
  WsoftProductResult checked = wsoft_product_check(product);
  if (checked != WSOFT_PRODUCT_OK) return checked;
  if (wsoft_hilo_find(hub, product.code) != nullptr) return WSOFT_PRODUCT_HUB_CODE;
  bool editing = old_code != nullptr && old_code[0] != 0;
  int at = editing ? local_index(*local, old_code) : -1;
  if (editing && at < 0) return WSOFT_PRODUCT_NOT_FOUND;
  int same = local_index(*local, product.code);
  if (same >= 0 && same != at) return WSOFT_PRODUCT_DUPLICATE;
  if (!editing) {
    if (local->count >= WSOFT_LOCAL_MAX) return WSOFT_PRODUCT_FULL;
    at = (int)local->count++;
  }
  local->products[at] = WsoftHiloProduct();
  strcpy(local->products[at].code, product.code);
  strcpy(local->products[at].name, product.name);
  local->products[at].target_g = product.target_g;
  local->products[at].low_g = product.low_g;
  local->products[at].high_g = product.high_g;
  return WSOFT_PRODUCT_OK;
}

WsoftProductResult wsoft_products_remove(WsoftLocalProducts *local, const char *code) {
  int at = local_index(*local, code);
  if (at < 0) return WSOFT_PRODUCT_NOT_FOUND;
  for (size_t i = (size_t)at; i + 1 < local->count; i++) local->products[i] = local->products[i + 1];
  local->count--;
  local->products[local->count] = WsoftHiloProduct();
  return WSOFT_PRODUCT_OK;
}

const WsoftHiloProduct *wsoft_products_find_local(const WsoftLocalProducts &local, const char *code) {
  int at = local_index(local, code);
  return at < 0 ? nullptr : &local.products[at];
}

bool wsoft_products_hidden(const WsoftHiloList &hub, const WsoftHiloProduct &product) {
  return wsoft_hilo_find(hub, product.code) != nullptr;
}

const WsoftHiloProduct *wsoft_products_find(const WsoftHiloList &hub, const WsoftLocalProducts &local,
                                            const char *code) {
  if (code == nullptr || code[0] == 0) return nullptr;
  const WsoftHiloProduct *p = wsoft_hilo_find(hub, code);
  return p != nullptr ? p : wsoft_products_find_local(local, code);
}

size_t wsoft_products_count(const WsoftHiloList &hub, const WsoftLocalProducts &local) {
  size_t n = hub.count;
  for (size_t i = 0; i < local.count; i++) {
    if (!wsoft_products_hidden(hub, local.products[i])) n++;
  }
  return n;
}

// The code at position i of the screen order (Hub first, then visible board products).
static const char *code_at(const WsoftHiloList &hub, const WsoftLocalProducts &local, size_t i) {
  if (i < hub.count) return hub.products[i].code;
  i -= hub.count;
  for (size_t j = 0; j < local.count; j++) {
    if (wsoft_products_hidden(hub, local.products[j])) continue;
    if (i == 0) return local.products[j].code;
    i--;
  }
  return "";
}

const char *wsoft_products_next(const WsoftHiloList &hub, const WsoftLocalProducts &local, const char *current) {
  size_t n = wsoft_products_count(hub, local);
  if (n == 0) return "";
  if (current == nullptr || current[0] == 0) return code_at(hub, local, 0);
  for (size_t i = 0; i < n; i++) {
    if (strcmp(code_at(hub, local, i), current) == 0) return i + 1 < n ? code_at(hub, local, i + 1) : "";
  }
  return code_at(hub, local, 0);
}

bool wsoft_products_apply(const WsoftHiloList &hub, const WsoftLocalProducts &local, WsoftHiloPick *pick) {
  bool changed = false;
  if (hub.has_default && strcmp(hub.default_set_at, pick->applied_set_at) != 0 &&
      wsoft_hilo_find(hub, hub.default_code) != nullptr) {
    strcpy(pick->current, hub.default_code);
    strcpy(pick->applied_set_at, hub.default_set_at);
    changed = true;
  }
  if (pick->current[0] != 0 && wsoft_products_find(hub, local, pick->current) == nullptr) {
    pick->current[0] = 0;
    changed = true;
  }
  return changed;
}

bool wsoft_products_valid(const WsoftLocalProducts &local) {
  if (local.count > WSOFT_LOCAL_MAX) return false;
  for (size_t i = 0; i < local.count; i++) {
    if (wsoft_product_check(local.products[i]) != WSOFT_PRODUCT_OK) return false;
    for (size_t j = 0; j < i; j++) {
      if (strcmp(local.products[i].code, local.products[j].code) == 0) return false;
    }
  }
  return true;
}
