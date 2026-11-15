#pragma once

// Hardware.Base only (WSOFT_ON_BASE): the extra Approval calls the switches and the scale
// setup need. Defined in approval.cpp inside #ifdef WSOFT_ON_BASE, and kept out of
// approval.h so that file stays identical to the Weighsoft.WSoft original.

#include <wsoft_scale_setup.h>
#include <wsoft_switch.h>

#include "approval.h"

// Writes the switch: log line (with where=), audit counter + 1, engine reset.
// Screen task only, and only through switches.cpp, which checks the rules first.
void approval_set(bool on, const char *who, WsoftSwitchWhere where, const char *reason);

// An audit event that is not the Approval switch itself (a Wi-Fi update, its switch or its
// password): audit counter + 1 and one line in the Approval log. make writes the line for the
// new counter and the time ("no-clock" before the clock is set); false = write nothing.
// Approval itself and the trade engine are left as they are. Screen task only (it writes flash).
typedef bool (*ApprovalEventLine)(uint32_t counter, const char *when, char *out, size_t size, void *context);
void approval_audit_event(ApprovalEventLine make, void *context);

// The last approval log lines, oldest first (at most max, and at most 8). Any task.
int approval_log_lines(char lines[][WSOFT_SWITCH_LOG_MAX], int max);

// approval_update() with the scale setup's hold for this reading (see approval.cpp).
TradeCheck approval_update_held(bool linked, bool fresh, bool alarm, int32_t value, int decimals, int unit,
                                WsoftScaleHold hold);
