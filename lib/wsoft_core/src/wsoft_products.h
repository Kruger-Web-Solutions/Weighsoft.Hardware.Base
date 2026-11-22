#pragma once

// Products made on the board itself (Rhyno, 13 Nov 2026: "also add/edit on the board").
// Pure C++ for host tests (test/test_wsoft_products).
//
// The board keeps two lists side by side:
//   - Hub products (wsoft_hilo): pulled from the Hub, read-only on the board.
//   - Board products (this file): added, edited and deleted on the board's web page, kept
//     in flash, never sent to the Hub as products. A weighing made with one still carries
//     its code, target and limits, so the Hub sees which product was weighed.
// A Hub product always wins: a board product may not take a Hub code, and if the Hub later
// adds the same code, the board product is hidden until it is renamed or deleted.
// Hub sync of board products can be added later without changing this store.

#include <stddef.h>
#include <stdint.h>

#include "wsoft_hilo.h"

static const size_t WSOFT_LOCAL_MAX = 20;

struct WsoftLocalProducts {
  WsoftHiloProduct products[WSOFT_LOCAL_MAX];
  size_t count = 0;
};

enum WsoftProductResult {
  WSOFT_PRODUCT_OK,
  WSOFT_PRODUCT_BAD_CODE,    // not [A-Za-z0-9_-]{1,32}
  WSOFT_PRODUCT_BAD_NAME,    // empty, longer than 24, or not plain printable text
  WSOFT_PRODUCT_BAD_LIMITS,  // needs 0 < low < high, low <= target <= high, high <= 1 000 000 kg
  WSOFT_PRODUCT_HUB_CODE,    // the Hub already uses this code
  WSOFT_PRODUCT_DUPLICATE,   // another board product has this code
  WSOFT_PRODUCT_FULL,        // already 20 board products
  WSOFT_PRODUCT_NOT_FOUND,   // no board product with that code
};

// Short code for the web page ("HUB_CODE"), and plain words for the operator.
const char *wsoft_product_result_code(WsoftProductResult result);
const char *wsoft_product_result_text(WsoftProductResult result);

// The product on its own: code, name and limits.
WsoftProductResult wsoft_product_check(const WsoftHiloProduct &product);

// Add (old_code null or "") or edit (old_code = the code it has now; the code may change).
WsoftProductResult wsoft_products_put(WsoftLocalProducts *local, const WsoftHiloList &hub, const char *old_code,
                                      const WsoftHiloProduct &product);

// Delete one board product.
WsoftProductResult wsoft_products_remove(WsoftLocalProducts *local, const char *code);

// The board product with this code, or nullptr (Hub products are not looked at).
const WsoftHiloProduct *wsoft_products_find_local(const WsoftLocalProducts &local, const char *code);

// True when this board product is hidden because the Hub now has the same code.
bool wsoft_products_hidden(const WsoftHiloList &hub, const WsoftHiloProduct &product);

// Any product the board can use: Hub first, then a board product. nullptr when none.
const WsoftHiloProduct *wsoft_products_find(const WsoftHiloList &hub, const WsoftLocalProducts &local,
                                            const char *code);

// Products the board can use (Hub + board products not hidden).
size_t wsoft_products_count(const WsoftHiloList &hub, const WsoftLocalProducts &local);

// The operator's next product on the screen: none -> Hub products -> board products -> none.
const char *wsoft_products_next(const WsoftHiloList &hub, const WsoftLocalProducts &local, const char *current);

// Like wsoft_hilo_apply, but a board product in use stays in use while it exists.
// True when the pick changed.
bool wsoft_products_apply(const WsoftHiloList &hub, const WsoftLocalProducts &local, WsoftHiloPick *pick);

// Kept in flash as raw bytes: a stored list is used only when it passes this check.
bool wsoft_products_valid(const WsoftLocalProducts &local);
