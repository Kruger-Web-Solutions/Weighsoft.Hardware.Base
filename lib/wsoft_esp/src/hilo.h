#pragma once

// Hardware.Base only (WSOFT_ON_BASE): the Hub Hi-Lo products on the board (Hub ADR 0008).
// The rules live in wsoft_core (wsoft_hilo), host-tested.
//
// The Hub task pulls the list at most once a minute (sooner after the operator changes the
// product) and hands it over with hilo_take(). The screen task reads the current product with
// hilo_current() and changes it with hilo_select_next(). Last change wins: a Hub default is
// applied once; after that the operator can pick another product on the screen.
// List and pick are kept in flash (NVS namespace "hilo"), so the board checks weights while
// the Hub is away, and after a restart.

#include <Arduino.h>

#include <wsoft_hilo.h>

// Once, before the Hub and screen tasks start.
void hilo_begin();

// Any task: the product in use now. False (and *out untouched) when none is picked.
bool hilo_current(WsoftHiloProduct *out);

// Screen task: the operator's next product (none -> first -> ... -> last -> none).
void hilo_select_next();

// Writer mode: use the weigh board's product (its code; "" = none), so the screen, the relays
// and the Hub show the same product on both boards. A code this board's list does not hold
// means no product (relays OFF). Does nothing when the pick is already that one.
void hilo_follow(const char *code);

// Hub task: time for a pull (60 s since the last one, or soon after an operator change).
bool hilo_pull_due();
// Hub task: the C1 request body for this pull.
bool hilo_request(char *out, size_t size);
// Hub task: a pull answered and checked out. Applies a new default, keeps the list.
void hilo_take(const WsoftHiloList &list);
// Hub task: a pull failed (no answer, bad status, bad signature or bad body). Try again later.
void hilo_failed();

struct HiloInfo {
  size_t count;               // products in the list
  bool picked;                // a product is in use
  WsoftHiloProduct current;   // valid when picked
  uint32_t last_ok_ms;        // millis() of the last good pull, 0 when none since boot
};
void hilo_info(HiloInfo *out);
