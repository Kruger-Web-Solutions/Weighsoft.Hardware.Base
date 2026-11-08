#pragma once

// Board side of the Hub Hi-Lo setup (Hub ADR 0008, contract C1 to C3). Pure C++ for host
// tests (test/test_wsoft_hilo).
//
// After a heartbeat, at most once a minute, the board posts {"current":"<code>"|null} to
// POST /api/v1/ingest/hilo, signed like every ingest request. The Hub answers with the
// products that have Hi-Lo limits and the scale's default, signed under its own prefix with
// the request's key id and nonce. The board keeps the list and its current product.
// Last change wins: a default is applied once (by its set_at); the operator can change the
// product on the screen after that. Each saved weighing carries its Hi-Lo result.

#include <stddef.h>
#include <stdint.h>

static const char *const WSOFT_HUB_HILO_PREFIX = "scale-models-hub-hilo-v1";
static const size_t WSOFT_HILO_MAX = 50;
static const size_t WSOFT_HILO_CODE_MAX = 32;
static const size_t WSOFT_HILO_NAME_MAX = 24;  // longer names are cut (ends in '~')
static const size_t WSOFT_HILO_SET_AT_MAX = 31;
static const int32_t WSOFT_HILO_GRAMS_MAX = 1000000000;
// {"current":"<32 chars>"} with room to spare.
static const size_t WSOFT_HILO_REQUEST_MAX = 64;

struct WsoftHiloProduct {
  char code[WSOFT_HILO_CODE_MAX + 1] = "";
  char name[WSOFT_HILO_NAME_MAX + 1] = "";
  int32_t target_g = 0;
  int32_t low_g = 0;
  int32_t high_g = 0;
};

struct WsoftHiloList {
  WsoftHiloProduct products[WSOFT_HILO_MAX];
  size_t count = 0;
  bool has_default = false;
  char default_code[WSOFT_HILO_CODE_MAX + 1] = "";
  char default_set_at[WSOFT_HILO_SET_AT_MAX + 1] = "";
};

// What the board uses now, and the set_at of the last Hub default it applied.
struct WsoftHiloPick {
  char current[WSOFT_HILO_CODE_MAX + 1] = "";
  char applied_set_at[WSOFT_HILO_SET_AT_MAX + 1] = "";
};

enum WsoftHiloState { WSOFT_HILO_NONE, WSOFT_HILO_LOW, WSOFT_HILO_PASS, WSOFT_HILO_HIGH };

// The Hi-Lo result kept with a saved weighing (on = a product was picked).
struct WsoftHiloStamp {
  bool on = false;
  char code[WSOFT_HILO_CODE_MAX + 1] = "";
  int32_t target_g = 0;
  int32_t low_g = 0;
  int32_t high_g = 0;
  WsoftHiloState state = WSOFT_HILO_NONE;
  bool auto_saved = false;
};

// A product code the Hub accepts: [A-Za-z0-9_-]{1,32}.
bool wsoft_hilo_code_ok(const char *code);

// The C1 request body. current empty or null = no product.
bool wsoft_hilo_request(const char *current, char *out, size_t size);

// The C1 answer, strictly: {"products":[...],"default":null|{"code":..,"set_at":..}} with at
// most 50 products, each with exactly code, name, target_g, low_g, high_g (whole grams,
// low <= target <= high). False for anything else; then nothing in it may be used.
bool wsoft_hilo_parse(const char *body, size_t length, WsoftHiloList *out);

// The product with this code, or nullptr.
const WsoftHiloProduct *wsoft_hilo_find(const WsoftHiloList &list, const char *code);

// Brings the pick up to date with a fresh list: applies a new Hub default once, and clears a
// current product the list no longer has. True when the pick changed.
bool wsoft_hilo_apply(const WsoftHiloList &list, WsoftHiloPick *pick);

// Same products (in the same order) and the same default. Field by field, so padding and
// the bytes after a string's end do not count.
bool wsoft_hilo_same(const WsoftHiloList &a, const WsoftHiloList &b);

// The operator's next product on the screen: none -> first -> ... -> last -> none.
const char *wsoft_hilo_next(const WsoftHiloList &list, const char *current);

// LOW below low_g, HIGH above high_g, else PASS, exactly (no float). value is in the reading's
// decimals and unit (1 kg, 2 t, 3 g). NONE for an unknown unit or decimals outside 0..9.
WsoftHiloState wsoft_hilo_check(const WsoftHiloProduct &product, int32_t value, int decimals, int unit);

// "LOW", "PASS", "HIGH" as the Hub expects; "" for NONE.
const char *wsoft_hilo_state_wire(WsoftHiloState state);
// Words for the screen: "TOO LOW", "OK", "TOO HIGH"; "" for NONE.
const char *wsoft_hilo_state_words(WsoftHiloState state);

// The ,"hi_lo":{...} part of a weighing (contract C3). reading_kg is the weighing's net kg
// text. False when the stamp is off, a value is out of range or out is too small.
bool wsoft_hilo_json(const WsoftHiloStamp &stamp, const char *reading_kg, char *out, size_t size);
