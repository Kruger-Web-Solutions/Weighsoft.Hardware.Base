#pragma once

#include <Arduino.h>

#include "trade_logic.h"

// Approval ON/OFF (decision paper 2026-10-25, Scale Models drafts).
// OFF: every screen and message says "Not for trade".
// ON: a weight goes out as a trade weight only when it is stable, with a sequence number and time.
// Trade weights wait in a queue in flash until the Hub accepts them; when the queue is full no new
// trade weight is issued, so none is ever dropped.
// Nothing here makes a weight legal: that needs NRCS approval and on-site verification.
// Only a technician with a USB cable can switch it; every switch is logged and moves the audit counter.

void approval_begin();
// Lines over USB: "APPROVAL STATUS", "APPROVAL LOG", "APPROVAL ON <technician> <reason>", "APPROVAL OFF <technician> <reason>".
void approval_handle_command(const String &command, Stream &port);
// Called on every scale poll. fresh is false when this poll failed and the reading is carried over.
TradeCheck approval_update(bool linked, bool fresh, bool alarm, int32_t value, int decimals, int unit);

bool approval_on();
bool approval_stable();
uint32_t approval_audit_counter();
TradeCheck approval_check();
TradeWeight approval_last_trade_weight();
size_t approval_pending();

// Hub side. approval_health_fields() returns the fields added inside the heartbeat's "health" object,
// starting with a comma, and the highest queued seq it included (0 if none). After the Hub answers 202,
// pass that seq to approval_delivered(); the main loop then removes those entries from flash.
String approval_health_fields(uint32_t *included_through);
void approval_delivered(uint32_t seq);
